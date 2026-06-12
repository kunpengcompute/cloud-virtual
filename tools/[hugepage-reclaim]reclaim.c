#define _GNU_SOURCE
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
} vm_info_t;

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
} reclaim_task_t;

static void signal_handler(int signo)
{
    g_stop = true;
}



static inline uint64_t monotonic_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static double ema_update(double old,
                         double current,
                         double alpha)
{
    return old * alpha + current * (1.0 - alpha);
}

void vm_heat_update(HeatManager *mgr,
                    VMHeatInfo *vm)
{
    uint64_t now;
    uint64_t delta_ns;
    uint64_t accessed;
    double current_heat;
    int ret;

    now = monotonic_time_ns();
    pthread_mutex_lock(&vm->lock);
    delta_ns = now - vm->last_update_ns;

    if (!delta_ns) {
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
    current_heat = vm->total_pages ?
        (double)accessed / vm->total_pages : 0;
    if (current_heat > 1.0)
        current_heat = 1.0;

    /*
     * EMA平滑
     */
    vm->heat_score = ema_update(vm->heat_score, current_heat, mgr->ema_alpha);
    vm->cold_score = 1.0 - vm->heat_score;
    vm->reclaimable_pages = vm->total_pages * vm->cold_score;
    vm->last_update_ns = now;
    pthread_mutex_unlock(&vm->lock);
}

static double pressure_factor(PressureLevel level)
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

static const char *pressure_name(PressureLevel level)
{
    switch (level) {
    case PRESSURE_LOW:    return "LOW";
    case PRESSURE_MEDIUM: return "MEDIUM";
    case PRESSURE_HIGH:   return "HIGH";
    case PRESSURE_CRITICAL: return "CRITICAL";
    default:              return "UNKNOWN";
    }
}

uint64_t vm_calculate_reclaim_pages(VMHeatInfo *vm,
                                    PressureLevel pressure)
{
    uint64_t reclaim_pages;
    uint64_t max_reclaim;
    pthread_mutex_lock(&vm->lock);

    /* 每台虚拟机至少保留20%内存 */
    reclaim_pages = vm->reclaimable_pages * pressure_factor(pressure);
    max_reclaim = vm->total_pages - vm->total_pages / 5;

    if (reclaim_pages > max_reclaim)
        reclaim_pages = max_reclaim;

    pthread_mutex_unlock(&vm->lock);
    return reclaim_pages;
}

/* 大顶堆 反向排序 */
int cmp(const void *a, const void *b)
{
    PageScore *pa = (PageScore *)a;
    PageScore *pb = (PageScore *)b;

    if (pa->score < pb->score) return 1;
    if (pa->score > pb->score) return -1;
    return 0;
}

/*
 * trigger_swap_multi - Swap multiple pages at once (with batching)
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
int trigger_swap_multi(void **addrs, uint64_t count, int fd, int pid, uint64_t reclaim_pages)
{
    /* Process in batches of 1000 to stay well under 8MB kernel limit */
    const int BATCH_SIZE = 1000;
    uint64_t processed = 0;
    uint64_t N = reclaim_pages;

    if (fd < 0) {
        LOG_ERR("Cannot open swap_pages: %s\n", strerror(errno));
        return -1;
    }

    PageScore *topn = malloc(N * sizeof(PageScore));
    if (!topn) {
        LOG_ERR("Failed to allocate memory for topn\n");
        return -1;
    }
    uint64_t size = 0;

    for (uint64_t i = 0; i < count; i++) {
        PageScore ps;
        ps.addr = (uint64_t)addrs[i];

        QueryPageScore(ps.addr, pid, &ps.score);

        if (size < N) {
            topn[size++] = ps;
            if (size == N)
                qsort(topn, N, sizeof(PageScore), cmp);
        } else {
            if (ps.score < topn[0].score) {
                topn[0] = ps;
                qsort(topn, N, sizeof(PageScore), cmp);
            }
        }
    }

    while (processed < N) {
        char buf[32 * BATCH_SIZE];
        size_t len = 0;
        int batch_count = 0;

        /* Build batch */
        for (uint64_t i = processed; i < N && batch_count < BATCH_SIZE; i++, batch_count++) {
            int sret = snprintf_s(buf + len, sizeof(buf) - len, sizeof(buf) - len, "0x%lx\n", (uintptr_t)topn[i].addr);
            if (sret < 0) {
                LOG_ERR("snprintf_s failed in batch build\n");
                free(topn);
                return -1;
            }
            len += sret;
        }

        /* Send batch */
        ssize_t ret = write(fd, buf, len);
        if (ret < 0) {
            LOG_ERR("Swap trigger write failed at batch %d: %s\n",
                 processed / BATCH_SIZE, strerror(errno));
            free(topn);
            return -1;
        }

        processed += batch_count;
    }

    free(topn);
    LOG_INFO("Swap trigger sent for %lu pages\n", N);
    return 0;
}

/* reclaim function */
int reclaim_hugepage(pid_t pid, unsigned long start, unsigned long size, uint64_t reclaim_pages)
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

    uint64_t num_pages = size / HUGEPAGE_SIZE;
    void **addrs = malloc(num_pages * sizeof(void*));
    if (!addrs) {
        LOG_ERR("Failed to allocate memory for addrs\n");
        close(fd);
        return -1;
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        addrs[i] = (void*)((uintptr_t)start + i * HUGEPAGE_SIZE);
    }

    int ret = trigger_swap_multi(addrs, num_pages, fd, pid,
        reclaim_pages < num_pages ? reclaim_pages : num_pages);
    if (ret != 0) {
        LOG_ERR("Failed to trigger swap for pid %d\n", pid);
    }

    free(addrs);
    close(fd);
    return ret;
}

/* NUMA hugepages statistics */
unsigned long get_node_free_hugepages(int node)
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

unsigned long get_node_nr_hugepages(int node)
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
int is_process_on_node(pid_t pid, int node)
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
int vma_on_node(const char *line, int node)
{
    char pattern[32];
    if (snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "N%d=", node) < 0) {
        LOG_ERR("snprintf_s failed for vma pattern of node %d\n", node);
        return 0;
    }
    return strstr(line, pattern) != NULL;
}

/* reclaim worker thread */
void *reclaim_worker(void *arg)
{
    reclaim_task_t *task = (reclaim_task_t *)arg;
    pid_t pid = task->vm->pid;
    int node = task->node;
    uint64_t reclaim_pages = task->reclaim_pages;

    char maps_path[256], numa_path[256];
    if (snprintf_s(maps_path, sizeof(maps_path), sizeof(maps_path) - 1, "/proc/%d/maps", pid) < 0) {
        LOG_ERR("Failed to format maps path for pid %d\n", pid);
        goto out;
    }
    if (snprintf_s(numa_path, sizeof(numa_path), sizeof(numa_path) - 1, "/proc/%d/numa_maps", pid) < 0) {
        LOG_ERR("Failed to format numa_maps path for pid %d\n", pid);
        goto out;
    }

    FILE *maps = fopen(maps_path, "r");
    if (!maps) {
        LOG_ERR("Failed to open /proc/%d/maps: %s\n", pid, strerror(errno));
        goto out;
    }

    FILE *numa = fopen(numa_path, "r");
    if (!numa) {
        LOG_ERR("Failed to open /proc/%d/numa_maps: %s\n", pid, strerror(errno));
        fclose(maps);
        goto out;
    }

    char map_line[512];

    while (fgets(map_line, sizeof(map_line), maps)) {
        if (!strstr(map_line, "hugepages"))
            continue;

        unsigned long start, end, size;
        if (sscanf_s(map_line, "%lx-%lx", &start, &end) != 2)
            continue;
        size = end - start;

        char numa_line[512];
        int found_on_node = 0;

        fseek(numa, 0, SEEK_SET);
        while (fgets(numa_line, sizeof(numa_line), numa)) {
            unsigned long vma_addr;
            if (sscanf_s(numa_line, "%lx", &vma_addr) == 1) {
                if (vma_addr >= start && vma_addr < end) {
                    if (vma_on_node(numa_line, node)) {
                        found_on_node = 1;
                        break;
                    }
                }
            }
        }

        if (found_on_node)
            reclaim_hugepage(pid, start, size, reclaim_pages);
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
pid_t read_pid_from_file(const char *domain_name) {
    char path[256];
    char pid_str[16];
    pid_t pid = 0;

    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/var/run/libvirt/qemu/%s.pid", domain_name) < 0) {
        LOG_ERR("snprintf_s failed for pid path of domain %s\n", domain_name);
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(pid_str, sizeof(pid_str), f)) {
            pid = (pid_t)atoi(pid_str);
        }
        fclose(f);
    }
    return pid;
}

int get_vm_infos(vm_info_t *vms, int max) {
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
            pid_t pid = read_pid_from_file(name);
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

void heat_manager_sync_vms(HeatManager *mgr)
{
    vm_info_t vm_infos[128];
    int vm_count = get_vm_infos(vm_infos, 128);
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
    for (int i = 0; i < vm_count; i++) {
        pid_t pid = vm_infos[i].pid;
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
        VMHeatInfo *new_vm = calloc(1, sizeof(VMHeatInfo));
        if (!new_vm) {
            LOG_ERR("Failed to allocate VMHeatInfo for pid %d\n", pid);
            continue;
        }
        new_vm->pid = pid;
        new_vm->alive = true;
        new_vm->heat_score = 0.5;
        new_vm->cold_score = 0.5;
        new_vm->total_pages = vm_infos[i].memory_kb / 2048;
        new_vm->reclaimable_pages = new_vm->total_pages * new_vm->cold_score;
        new_vm->last_update_ns = monotonic_time_ns();
        pthread_mutex_init(&new_vm->lock, NULL);
        new_vm->next = mgr->head;
        mgr->head = new_vm;
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
            int in_use = (vm->in_reclaim > 0);
            pthread_mutex_unlock(&vm->lock);

            if (in_use)
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

void *heat_sampling_thread(void *arg)
{
    HeatManager *mgr = arg;
    while (!g_stop) {
        /*
         * 动态同步VM
         */
        heat_manager_sync_vms(mgr);
        pthread_rwlock_rdlock(&mgr->rwlock);
        VMHeatInfo *vm = mgr->head;
        while (vm) {
            vm_heat_update(mgr, vm);
            vm = vm->next;
        }
        pthread_rwlock_unlock(&mgr->rwlock);
        sleep(mgr->sample_interval_s);
    }

    LOG_INFO("heat thread exit\n");
    return NULL;
}

/* Monitor and reclaim NUMA hugepages */
void monitor_and_reclaim(HeatManager *mgr)
{
    if (numa_available() < 0) {
        LOG_ERR("System does not support NUMA\n");
        return;
    }

    int numa_nodes = numa_num_configured_nodes();
    int *consecutive = calloc(numa_nodes, sizeof(int));

    while (!g_stop) {
        for (int node = 0; node < numa_nodes; node++) {
            unsigned long nr_pages = get_node_nr_hugepages(node);
            if (!nr_pages) {
                LOG_WARN("Node %d doesn't support hugepage\n", node);
                continue;
            }

            unsigned long free_pages = get_node_free_hugepages(node);
            double free_ratio = (double)free_pages / nr_pages;

            if (free_ratio >= LOW_WATERMARK) {
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
            if (free_ratio <= BOTTOM_SIZE) {
                pressure = (consecutive[node] >= 4) ? PRESSURE_CRITICAL : PRESSURE_HIGH;
            } else {
                if (consecutive[node] >= 12)      pressure = PRESSURE_CRITICAL;
                else if (consecutive[node] >= 8)  pressure = PRESSURE_HIGH;
                else if (consecutive[node] >= 4)  pressure = PRESSURE_MEDIUM;
                else                              pressure = PRESSURE_LOW;
            }

            LOG_INFO("Node %d free/total: %lu/%lu, pressure: %s, count: %d\n",
               node, free_pages, nr_pages, pressure_name(pressure), consecutive[node]);

            pthread_t tids[128];
            int tcount = 0;
            pthread_rwlock_rdlock(&mgr->rwlock);
            VMHeatInfo *vm = mgr->head;

            while (vm) {
                pid_t pid = vm->pid;

                if (!is_process_on_node(pid, node)) {
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

                reclaim_task_t *task = malloc(sizeof(*task));
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
                task->reclaim_pages = vm_calculate_reclaim_pages(vm, pressure);

                if (!task->reclaim_pages) {
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                    vm = vm->next;
                    continue;
                }

                if (tcount >= 128) {
                    LOG_ERR("Worker count exceeded limit\n");
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                    break;
                }

                if (pthread_create(&tids[tcount], NULL,
                                   reclaim_worker, task) == 0) {
                    tcount++;
                } else {
                    pthread_mutex_lock(&vm->lock);
                    vm->in_reclaim--;
                    pthread_mutex_unlock(&vm->lock);
                    free(task);
                }
                vm = vm->next;
            }
            pthread_rwlock_unlock(&mgr->rwlock);

            for (int i = 0; i < tcount; i++) {
                pthread_join(tids[i], NULL);
            }
        }
        /* 每5s触发一次回收 */
        sleep(5);
    }

    LOG_INFO("monitor loop exit\n");
    free(consecutive);
}

void heat_manager_destroy(HeatManager *mgr)
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
    pthread_t heat_tid;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    HeatManager mgr = {0};
    pthread_rwlock_init(&mgr.rwlock, NULL);
    /* 热度采样间隔，单位：秒 */
    mgr.sample_interval_s = 10;
    /* EMA平滑系数 (0~1)，越大越平滑，越小越敏感 */
    mgr.ema_alpha = 0.7;

    if (pthread_create(&heat_tid, NULL, heat_sampling_thread, &mgr)) {
        LOG_ERR("create heat thread failed\n");
        pthread_rwlock_destroy(&mgr.rwlock);
        return -1;
    }
    monitor_and_reclaim(&mgr);
    pthread_join(heat_tid, NULL);

    /*
     * 清理所有VM
     */
    heat_manager_destroy(&mgr);
    pthread_rwlock_destroy(&mgr.rwlock);
    LOG_INFO("daemon exit\n");
    return 0;
}
