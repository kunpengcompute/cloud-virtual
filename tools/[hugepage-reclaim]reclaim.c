/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2026. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <libvirt/libvirt.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <numa.h>
#include <memlink_sdk.h>
#include <securec.h>
#include <signal.h>

#define LOW_WATERMARK (1.0 / 5.0)
#define BOTTOM_SIZE (1.0 / 10.0)
#define HUGEPAGE_SIZE (2 * 1024 * 1024UL)
#define NUMA_NODE_MAX 50

#define LOG_INFO(...) printf("[INFO] " __VA_ARGS__)
#define LOG_WARN(...) printf("[WARN] " __VA_ARGS__)
#define LOG_ERR(...) printf("[ERROR] " __VA_ARGS__)

static volatile bool g_stop = false;

typedef struct {
    uint64_t addr;
    uint64_t score;
} PageScore;

typedef enum {
    PRESSURE_LOW,
    PRESSURE_MEDIUM,
    PRESSURE_HIGH,
    PRESSURE_CRITICAL,
} PressureLevel;

typedef struct {
    pid_t pid;
    unsigned long memory_kb;
    char name[128];
} VMInfo;

typedef struct VMHeatInfo {
    pid_t pid;
    bool alive;
    /* VM总页数 */
    uint64_t total_pages;
    /* 当前可回收页 */
    uint64_t reclaimable_pages;
    /* 热度值 [0,1] */
    double heat_score;
    /* 冷度值 [0,1] */
    double cold_score;
    /* 最近更新时间 */
    uint64_t last_update_ns;
    /* 正在回收计数 */
    int in_reclaim;
    pthread_mutex_t lock;
    struct VMHeatInfo *next;
} VMHeatInfo;

typedef struct {
    VMHeatInfo *head;
    pthread_rwlock_t rwlock;
    uint64_t sample_interval_s;
    double ema_alpha;
    int vm_count;
} HeatManager;

typedef struct {
    int node;
    uint64_t reclaim_pages;
    VMHeatInfo *vm;
} ReclaimTask;

static void SignalHandler(int signo)
{
    g_stop = true;
}

static inline uint64_t MonotonicTimeNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static double EmaUpdate(double old, double current, double alpha)
{
    return old * alpha + current * (1.0 - alpha);
}

static void VMHeatUpdate(HeatManager *mgr, VMHeatInfo *vm)
{
    uint64_t now;
    uint64_t deltaNs;
    uint64_t accessed;
    double currentHeat;
    int ret;

    now = MonotonicTimeNs();
    pthread_mutex_lock(&vm->lock);
    deltaNs = now - vm->last_update_ns;

    if (!deltaNs) {
        pthread_mutex_unlock(&vm->lock);
        return;
    }

    /*
     * 查询接口：
     * 返回delta访问页数
     * 并清零
     */
    ret = QueryAndClearPageAccessedCount(vm->pid, &accessed);
    if (ret) {
        LOG_ERR("QueryAndClearPageAccessedCount failed for pid %d: %d\n", vm->pid, ret);
        pthread_mutex_unlock(&vm->lock);
        return;
    }
    /*
     * 采样窗口内被访问过的页面比例
     */
    currentHeat = vm->total_pages ?
        (double)accessed / vm->total_pages : 0;
    if (currentHeat > 1.0)
        currentHeat = 1.0;

    /*
     * EMA平滑
     */
    vm->heat_score = EmaUpdate(vm->heat_score, currentHeat, mgr->ema_alpha);
    vm->cold_score = 1.0 - vm->heat_score;
    vm->reclaimable_pages = vm->total_pages * vm->cold_score;
    vm->last_update_ns = now;
    pthread_mutex_unlock(&vm->lock);
}

static double PressureFactor(PressureLevel level)
{
    /* 计算回收比例 */
    switch (level) {
        case PRESSURE_LOW:
            return 0.3;
        case PRESSURE_MEDIUM:
            return 0.5;
        case PRESSURE_HIGH:
            return 0.7;
        case PRESSURE_CRITICAL:
            return 0.9;
        default:
            return 0.1;
    }
}

static const char *PressureName(PressureLevel level)
{
    switch (level) {
        case PRESSURE_LOW:    return "LOW";
        case PRESSURE_MEDIUM: return "MEDIUM";
        case PRESSURE_HIGH:   return "HIGH";
        case PRESSURE_CRITICAL: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

static uint64_t VMCalculateReclaimPages(VMHeatInfo *vm, PressureLevel pressure)
{
    uint64_t reclaimPages;
    uint64_t maxReclaim;
    pthread_mutex_lock(&vm->lock);

    /* 每台虚拟机至少保留20%内存 */
    reclaimPages = vm->reclaimable_pages * PressureFactor(pressure);
    maxReclaim = vm->total_pages - vm->total_pages / 5;

    if (reclaimPages > maxReclaim)
        reclaimPages = maxReclaim;

    pthread_mutex_unlock(&vm->lock);
    return reclaimPages;
}

/* 大顶堆 反向排序 */
static int PageScoreCmp(const void *a, const void *b)
{
    PageScore *pa = (PageScore *)a;
    PageScore *pb = (PageScore *)b;

    if (pa->score < pb->score) return 1;
    if (pa->score > pb->score) return -1;
    return 0;
}

/*
 * TriggerSwapMulti - Swap multiple pages at once (with batching)
 * @addrs Array of virtual addresses
 * @count Number of addresses
 *
 * Returns: 0 on success (request sent), -1 on failure
 *
 * Note: Like trigger_swap(). this only sends the request. Actual swap
 * success must be verified by checking HugePages_Swpd or free hugepages.
 *
 * Batching: Kernel has ~8MB kmalloc limit. We batch to stay well under it.
 * Each address: "0x%lx\n" ~ 20 bytes, so 1000 addresses ~ 20KB
 */
static int TriggerSwapMulti(uint64_t *addrs, uint64_t count, int fd, int pid, uint64_t reclaimPages)
{
    /* Process in batches of 1000 to stay well under 8MB kernel limit */
    const int batchSize = 1000;
    uint64_t processed = 0;
    uint64_t numReclaim = reclaimPages;

    if (fd < 0) {
        LOG_ERR("Cannot open swap_pages: %s\n", strerror(errno));
        return -1;
    }

    PageScore *topN = malloc(numReclaim * sizeof(PageScore));
    if (!topN) {
        LOG_ERR("Failed to allocate memory for topn\n");
        return -1;
    }
    uint64_t size = 0;

    for (uint64_t i = 0; i < count; i++) {
        PageScore ps;
        ps.addr = addrs[i];

        QueryPageScore(ps.addr, pid, &ps.score);

        if (size < numReclaim) {
            topN[size++] = ps;
            if (size == numReclaim)
                qsort(topN, numReclaim, sizeof(PageScore), PageScoreCmp);
        } else {
            if (ps.score < topN[0].score) {
                topN[0] = ps;
                qsort(topN, numReclaim, sizeof(PageScore), PageScoreCmp);
            }
        }
    }

    while (processed < numReclaim) {
        char buf[32 * batchSize];
        size_t len = 0;
        int batchCount = 0;

        /* Build batch */
        for (uint64_t i = processed; i < numReclaim && batchCount < batchSize; i++, batchCount++) {
            int sret = snprintf_s(buf + len, sizeof(buf) - len, sizeof(buf) - len, "0x%lx\n", (uintptr_t)topN[i].addr);
            if (sret < 0) {
                LOG_ERR("snprintf_s failed in batch build\n");
                free(topN);
                return -1;
            }
            len += sret;
        }

        /* Send batch */
        ssize_t ret = write(fd, buf, len);
        if (ret < 0) {
            LOG_ERR("Swap trigger write failed at batch %d: %s\n",
                 processed / batchSize, strerror(errno));
            free(topN);
            return -1;
        }

        processed += batchCount;
    }

    free(topN);
    LOG_INFO("Swap trigger sent for %lu pages\n", numReclaim);
    return 0;
}

/* reclaim function */
static int ReclaimHugepage(pid_t pid, unsigned long start, unsigned long size, uint64_t reclaimPages)
{
    char path[256];
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/swap_pages", pid) < 0) {
        LOG_ERR("Failed to format path for pid %d\n", pid);
        return -1;
    }

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERR("Failed to open /proc/%d/swap_pages: %s\n", pid, strerror(errno));
        return -1;
    }

    uint64_t numPages = size / HUGEPAGE_SIZE;
    uint64_t *addrs = malloc(numPages * sizeof(uint64_t));
    if (!addrs) {
        LOG_ERR("Failed to allocate memory for addrs\n");
        close(fd);
        return -1;
    }

    for (uint64_t i = 0; i < numPages; i++) {
        addrs[i] = (uint64_t)start + i * HUGEPAGE_SIZE;
    }

    int ret = TriggerSwapMulti(addrs, numPages, fd, pid,
        reclaimPages < numPages ? reclaimPages : numPages);
    if (ret != 0) {
        LOG_ERR("Failed to trigger swap for pid %d\n", pid);
    }

    free(addrs);
    close(fd);
    return ret;
}

/* NUMA hugepages statistics */
static unsigned long GetNodeFreeHugepages(int node)
{
    char path[256];
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1,
        "/sys/devices/system/node/node%d/hugepages/hugepages-2048kB/free_hugepages",
        node) < 0) {
        LOG_ERR("snprintf_s failed for free_hugepages path on node %d\n", node);
        return 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG_WARN("Failed to open free_hugepages on node %d\n", node);
        return 0;
    }

    unsigned long pages = 0;
    if (fscanf_s(fp, "%lu", &pages) != 1) {
        LOG_WARN("Failed to read free_hugepages on node %d\n", node);
        fclose(fp);
        return 0;
    }
    fclose(fp);

    return pages;
}

static unsigned long GetNodeNrHugepages(int node)
{
    char path[256];
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1,
        "/sys/devices/system/node/node%d/hugepages/hugepages-2048kB/nr_hugepages",
        node) < 0) {
        LOG_ERR("snprintf_s failed for nr_hugepages path on node %d\n", node);
        return 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG_WARN("Failed to open nr_hugepages on node %d\n", node);
        return 0;
    }

    unsigned long pages = 0;
    if (fscanf_s(fp, "%lu", &pages) != 1) {
        LOG_WARN("Failed to read nr_hugepages on node %d\n", node);
        fclose(fp);
        return 0;
    }
    fclose(fp);

    return pages;
}

/* Check if process is on specific NUMA node */
static int IsProcessOnNode(pid_t pid, int node)
{
    char path[256], line[512];
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/numa_maps", pid) < 0) {
        LOG_ERR("snprintf_s failed for numa_maps path of pid %d\n", pid);
        return 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG_WARN("Failed to open /proc/%d/numa_maps\n", pid);
        return 0;
    }

    char pattern[32];
    if (snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "N%d=", node) < 0) {
        LOG_ERR("snprintf_s failed for numa pattern of node %d\n", node);
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, pattern)) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

/* Check if VMA is on specific NUMA node */
static int VMAOnNode(const char *line, int node)
{
    char pattern[32];
    if (snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "N%d=", node) < 0) {
        LOG_ERR("snprintf_s failed for vma pattern of node %d\n", node);
        return 0;
    }
    return strstr(line, pattern) != NULL;
}

/* reclaim worker thread */
static void *ReclaimWorker(void *arg)
{
    ReclaimTask *task = (ReclaimTask *)arg;
    pid_t pid = task->vm->pid;
    int node = task->node;
    uint64_t reclaimPages = task->reclaim_pages;

    char mapsPath[256], numaPath[256];
    if (snprintf_s(mapsPath, sizeof(mapsPath), sizeof(mapsPath) - 1, "/proc/%d/maps", pid) < 0) {
        LOG_ERR("Failed to format maps path for pid %d\n", pid);
        goto out;
    }
    if (snprintf_s(numaPath, sizeof(numaPath), sizeof(numaPath) - 1, "/proc/%d/numa_maps", pid) < 0) {
        LOG_ERR("Failed to format numa_maps path for pid %d\n", pid);
        goto out;
    }

    FILE *maps = fopen(mapsPath, "r");
    if (!maps) {
        LOG_ERR("Failed to open /proc/%d/maps: %s\n", pid, strerror(errno));
        goto out;
    }

    FILE *numa = fopen(numaPath, "r");
    if (!numa) {
        LOG_ERR("Failed to open /proc/%d/numa_maps: %s\n", pid, strerror(errno));
        fclose(maps);
        goto out;
    }

    char mapLine[512];

    while (fgets(mapLine, sizeof(mapLine), maps)) {
        if (!strstr(mapLine, "hugepages"))
            continue;

        unsigned long start, end, size;
        if (sscanf_s(mapLine, "%lx-%lx", &start, &end) != 2)
            continue;
        size = end - start;

        char numaLine[512];
        int foundOnNode = 0;

        fseek(numa, 0, SEEK_SET);
        while (fgets(numaLine, sizeof(numaLine), numa)) {
            unsigned long vmaAddr;
            if (sscanf_s(numaLine, "%lx", &vmaAddr) == 1) {
                if (vmaAddr >= start && vmaAddr < end) {
                    if (VMAOnNode(numaLine, node)) {
                        foundOnNode = 1;
                        break;
                    }
                }
            }
        }

        if (foundOnNode)
            ReclaimHugepage(pid, start, size, reclaimPages);
    }

    fclose(maps);
    fclose(numa);

out:
    pthread_mutex_lock(&task->vm->lock);
    task->vm->in_reclaim--;
    pthread_mutex_unlock(&task->vm->lock);
    free(task);
    return NULL;
}

/* Read active domains PID from libvirt domain file */
static pid_t ReadPidFromFile(const char *domainName)
{
    char path[256];
    char pidStr[16];
    pid_t pid = 0;

    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/var/run/libvirt/qemu/%s.pid", domainName) < 0) {
        LOG_ERR("snprintf_s failed for pid path of domain %s\n", domainName);
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(pidStr, sizeof(pidStr), f)) {
            pid = (pid_t)atoi(pidStr);
        }
        fclose(f);
    }
    return pid;
}

static int GetVMInfos(VMInfo *vms, int max)
{
    virConnectPtr conn;
    virDomainPtr *domains;
    int num, i, count = 0;

    conn = virConnectOpenReadOnly("qemu:///system");
    if (!conn) {
        LOG_ERR("Failed to connect to libvirt\n");
        return 0;
    }

    num = virConnectListAllDomains(conn, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
    if (num < 0) {
        LOG_ERR("Failed to list domains from libvirt\n");
        virConnectClose(conn);
        return 0;
    }

    for (i = 0; i < num && count < max; i++) {
        char *meta = virDomainGetMetadata(domains[i], VIR_DOMAIN_METADATA_ELEMENT, "http://reclaim.io", 0);
        const char *name = virDomainGetName(domains[i]);
        virDomainInfo info;

        // 检查是否包含禁止回收标签
        int skip = 0;
        if (meta != NULL) {
            // 如果包含 <no_reclaim> 标签，则跳过该虚拟机
            if (strstr(meta, "no_reclaim") != NULL) {
                skip = 1;
            }
            free(meta);
        }

        if (skip) {
            virDomainFree(domains[i]);
            continue;
        }

        if (virDomainGetInfo(domains[i], &info) < 0) {
            virDomainFree(domains[i]);
            continue;
        }

        if (name) {
            pid_t pid = ReadPidFromFile(name);
            if (pid > 0) {
                vms[count].pid = pid;
                vms[count].memory_kb = info.memory;
                if (snprintf_s(vms[count].name, sizeof(vms[count].name), sizeof(vms[count].name) - 1, "%s", name) < 0) {
                    LOG_ERR("snprintf_s failed for vm name %s\n", name);
                    virDomainFree(domains[i]);
                    continue;
                }
                count++;
            }
        }
        virDomainFree(domains[i]);
    }

    free(domains);
    virConnectClose(conn);
    return count;
}

static void HeatManagerSyncVMs(HeatManager *mgr)
{
    VMInfo vmInfos[128];
    int vmCount = GetVMInfos(vmInfos, 128);
    pthread_rwlock_wrlock(&mgr->rwlock);

    /*
     * 第一阶段：
     * 全部标记为dead
     */
    VMHeatInfo *vm = mgr->head;

    while (vm) {
        vm->alive = false;
        vm = vm->next;
    }

    /*
     * 第二阶段：
     * 扫描当前VM
     */
    for (int i = 0; i < vmCount; i++) {
        pid_t pid = vmInfos[i].pid;
        VMHeatInfo *found = NULL;
        vm = mgr->head;
        while (vm) {
            if (vm->pid == pid) {
                found = vm;
                break;
            }

            vm = vm->next;
        }

        /*
         * 已存在
         */
        if (found) {
            found->alive = true;
            continue;
        }

        /*
         * 新VM
         */
        VMHeatInfo *newVm = calloc(1, sizeof(VMHeatInfo));
        if (!newVm) {
            LOG_ERR("Failed to allocate VMHeatInfo for pid %d\n", pid);
            continue;
        }
        newVm->pid = pid;
        newVm->alive = true;
        newVm->heat_score = 0.5;
        newVm->cold_score = 0.5;
        newVm->total_pages = vmInfos[i].memory_kb / 2048;
        newVm->reclaimable_pages = newVm->total_pages * newVm->cold_score;
        newVm->last_update_ns = MonotonicTimeNs();
        pthread_mutex_init(&newVm->lock, NULL);
        newVm->next = mgr->head;
        mgr->head = newVm;
        mgr->vm_count++;
        LOG_INFO("add vm pid=%d\n", pid);
    }

    /*
     * 第三阶段：
     * 删除dead VM
     */
    VMHeatInfo **pprev = &mgr->head;
    vm = mgr->head;
    while (vm) {
        if (!vm->alive) {
            pthread_mutex_lock(&vm->lock);
            int inUse = (vm->in_reclaim > 0);
            pthread_mutex_unlock(&vm->lock);

            if (inUse)
                goto next_dead;

            LOG_INFO("remove vm pid=%d\n", vm->pid);
            *pprev = vm->next;
            pthread_mutex_destroy(&vm->lock);
            free(vm);
            mgr->vm_count--;
            vm = *pprev;
            continue;
        }
next_dead:
        pprev = &vm->next;
        vm = vm->next;
    }
    pthread_rwlock_unlock(&mgr->rwlock);
}

static void *HeatSamplingThread(void *arg)
{
    HeatManager *mgr = arg;
    while (!g_stop) {
        /*
         * 动态同步VM
         */
        HeatManagerSyncVMs(mgr);
        pthread_rwlock_rdlock(&mgr->rwlock);
        VMHeatInfo *vm = mgr->head;
        while (vm) {
            VMHeatUpdate(mgr, vm);
            vm = vm->next;
        }
        pthread_rwlock_unlock(&mgr->rwlock);
        sleep(mgr->sample_interval_s);
    }

    LOG_INFO("heat thread exit\n");
    return NULL;
}

/* Monitor and reclaim NUMA hugepages */
static void MonitorAndReclaim(HeatManager *mgr)
{
    if (numa_available() < 0) {
        LOG_ERR("System does not support NUMA\n");
        return;
    }

    int numaNodes = numa_num_configured_nodes();
    if (numaNodes <= 0 || numaNodes > NUMA_NODE_MAX) {
        LOG_ERR("Invalid numa node count %d, max supported is %d\n", numaNodes, NUMA_NODE_MAX);
        return;
    }

    int *consecutive = calloc(numaNodes, sizeof(int));
    if (!consecutive) {
        LOG_ERR("Failed to allocate consecutive array for %d nodes\n", numaNodes);
        return;
    }

    while (!g_stop) {
        for (int node = 0; node < numaNodes; node++) {
            unsigned long nrPages = GetNodeNrHugepages(node);
            if (nrPages == 0) {
                LOG_WARN("Node %d doesn't support hugepage\n", node);
                continue;
            }

            unsigned long freePages = GetNodeFreeHugepages(node);
            double freeRatio = (double)freePages / nrPages;

            if (freeRatio >= LOW_WATERMARK) {
                consecutive[node] = 0;
                continue;
            }

            consecutive[node]++;

            /*
             * 回收可能失败，两次算一轮，每两轮压力上升一级：
             *  4次(2轮)  → MEDIUM
             *  8次(4轮)  → HIGH
             *  12次(6轮) → CRITICAL
             *
             * 低于10%时快速升级：
             *  1-3次 → HIGH,  ≥4次 → CRITICAL
             */
            PressureLevel pressure;
            if (freeRatio <= BOTTOM_SIZE) {
                pressure = (consecutive[node] >= 4) ? PRESSURE_CRITICAL : PRESSURE_HIGH;
            } else {
                if (consecutive[node] >= 12)      pressure = PRESSURE_CRITICAL;
                else if (consecutive[node] >= 8)  pressure = PRESSURE_HIGH;
                else if (consecutive[node] >= 4)  pressure = PRESSURE_MEDIUM;
                else                              pressure = PRESSURE_LOW;
            }

            LOG_INFO("Node %d free/total: %lu/%lu, pressure: %s, count: %d\n",
               node, freePages, nrPages, PressureName(pressure), consecutive[node]);

            pthread_t tids[128];
            int tCount = 0;
            pthread_rwlock_rdlock(&mgr->rwlock);
            VMHeatInfo *vm = mgr->head;

            while (vm) {
                pid_t pid = vm->pid;

                if (!IsProcessOnNode(pid, node)) {
                    vm = vm->next;
                    continue;
                }

                pthread_mutex_lock(&vm->lock);
                int skip = (vm->cold_score < 0.1);
                if (!skip)
                    vm->in_reclaim++;
                pthread_mutex_unlock(&vm->lock);

                if (skip) {
                    vm = vm->next;
                    continue;
                }

                ReclaimTask *task = malloc(sizeof(*task));
                if (!task) {
                    LOG_ERR("Failed to allocate reclaim task\n");
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    vm = vm->next;
                    continue;
                }
                task->vm = vm;
                task->node = node;
                task->reclaim_pages = VMCalculateReclaimPages(vm, pressure);

                if (!task->reclaim_pages) {
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                    vm = vm->next;
                    continue;
                }

                if (tCount >= 128) {
                    LOG_ERR("Worker count exceeded limit\n");
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                    break;
                }

                if (pthread_create(&tids[tCount], NULL,
                                   ReclaimWorker, task) == 0) {
                    tCount++;
                } else {
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                }
                vm = vm->next;
            }
            pthread_rwlock_unlock(&mgr->rwlock);

            for (int i = 0; i < tCount; i++) {
                pthread_join(tids[i], NULL);
            }
        }
        /* 每5s触发一次回收 */
        sleep(5);
    }

    LOG_INFO("monitor loop exit\n");
    free(consecutive);
}

static void HeatManagerDestroy(HeatManager *mgr)
{
    pthread_rwlock_wrlock(&mgr->rwlock);
    VMHeatInfo *vm = mgr->head;

    while (vm) {
        VMHeatInfo *next = vm->next;
        pthread_mutex_destroy(&vm->lock);
        free(vm);
        vm = next;
    }

    mgr->head = NULL;
    pthread_rwlock_unlock(&mgr->rwlock);
}

int main()
{
    LOG_INFO("Starting NUMA hugepage reclaim (libvirt mode)...\n");
    pthread_t heatTid;
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    HeatManager mgr = {0};
    pthread_rwlock_init(&mgr.rwlock, NULL);
    /* 热度采样间隔，单位：秒 */
    mgr.sample_interval_s = 10;
    /* EMA平滑系数 (0~1)，越大越平滑，越小越敏感 */
    mgr.ema_alpha = 0.7;

    if (pthread_create(&heatTid, NULL, HeatSamplingThread, &mgr)) {
        LOG_ERR("create heat thread failed\n");
        pthread_rwlock_destroy(&mgr.rwlock);
        return -1;
    }
    MonitorAndReclaim(&mgr);
    pthread_join(heatTid, NULL);

    /*
     * 清理所有VM
     */
    HeatManagerDestroy(&mgr);
    pthread_rwlock_destroy(&mgr.rwlock);
    LOG_INFO("daemon exit\n");
    return 0;
}
