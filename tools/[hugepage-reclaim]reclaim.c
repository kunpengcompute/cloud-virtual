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

#define LOW_WATERMARK (1.0 / 5.0)
#define BOTTOM_SIZE (1.0 / 10.0)
#define HUGEPAGE_SIZE (2 * 1024 * 1024UL)

#define LOG_INFO(...) printf("[INFO] " __VA_ARGS__)
#define LOG_WARN(...) printf("[WARN] " __VA_ARGS__)
#define LOG_ERR(...) printf("[ERROR] " __VA_ARGS__)

typedef struct {
    pid_t pid;
    int node;
    bool warning;
} reclaim_task_t;

typedef struct {
    uint64_t addr;
    size_t score;
} PageScore;

/* 大顶堆 反向排序 */
int cmp(const void *a, const void *b)
{
    PageScore *pa = (PageScore *)a;
    PageScore *pb = (PageScore *)b;

    return pb->score - pa->score;
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
int trigger_swap_multi(void **addrs, int count, int fd, int pid, bool warning)
{
    /* Process in batches of 1000 to stay well under 8MB kernel limit */
    const int BATCH_SIZE = 1000;
    int processed = 0;
    int N = count / 4;
    if (warning)
        N = count;

    if (fd < 0) {
        LOG_ERR("Cannot open swap_pages: %s\n", strerror(errno));
        return -1;
    }

    PageScore *topn = malloc(N * sizeof(PageScore));
    if (!topn) {
        LOG_ERR("Failed to allocate memory for topn\n");
        return -1;
    }
    int size = 0;

    for (int i = 0; i < count; i++) {
        PageScore ps;
        ps.addr = (uint64_t)addrs[i];

        if (warning) {
            if (size < N)
                topn[size++] = ps;
            continue;
        }
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
        for (int i = processed; i < N && batch_count < BATCH_SIZE; i++, batch_count++) {
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
    LOG_INFO("Swap trigger sent for %d pages\n", N);
    return 0;
}

/* reclaim function */
int reclaim_hugepage(pid_t pid, unsigned long start, unsigned long size, bool warning)
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

    int num_pages = size / HUGEPAGE_SIZE;
    void **addrs = malloc(num_pages * sizeof(void*));
    if (!addrs) {
        LOG_ERR("Failed to allocate memory for addrs\n");
        close(fd);
        return -1;
    }

    for (int i = 0; i < num_pages; i++) {
        addrs[i] = (void*)((uintptr_t)start + i * HUGEPAGE_SIZE);
    }

    int ret = trigger_swap_multi(addrs, num_pages, fd, pid, warning);
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
    pid_t pid = task->pid;
    int node = task->node;
    bool warning = task->warning;

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

        if (found_on_node) {
            reclaim_hugepage(pid, start, size, warning);
        }
    }

    fclose(maps);
    fclose(numa);

out:
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

int get_vm_pids(pid_t *pids, int max) {
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

        if (name) {
            pid_t pid = read_pid_from_file(name);
            if (pid > 0) {
                pids[count++] = pid;
            }
        }
        virDomainFree(domains[i]);
    }

    free(domains);
    virConnectClose(conn);
    return count;
}

/* Monitor and reclaim NUMA hugepages */
void monitor_and_reclaim()
{
    if (numa_available() < 0) {
        LOG_ERR("System does not support NUMA\n");
        return;
    }

    int numa_nodes = numa_num_configured_nodes();

    while (1) {
        for (int node = 0; node < numa_nodes; node++) {
            unsigned long nr_pages = get_node_nr_hugepages(node);
            if (!nr_pages) {
                LOG_WARN("Node %d doesn't support hugepage\n", node);
                continue;
            }

            unsigned long free_pages = get_node_free_hugepages(node);

            if ((double)free_pages / nr_pages >= LOW_WATERMARK)
                continue;

            if ((double)free_pages / nr_pages <= BOTTOM_SIZE)
                LOG_WARN("Node %d low hugepage: %lu MB (free/total: %lu/%lu)\n",
                   node, free_pages * 2, free_pages, nr_pages);
            else
                LOG_INFO("Node %d hugepage: %lu MB (free/total: %lu/%lu)\n",
                   node, free_pages * 2, free_pages, nr_pages);

            pid_t vm_pids[128];
            int vm_count = get_vm_pids(vm_pids, 128);

            pthread_t tids[128];
            int tcount = 0;

            for (int i = 0; i < vm_count; i++) {
                pid_t pid = vm_pids[i];

                if (!is_process_on_node(pid, node))
                    continue;

                reclaim_task_t *task = malloc(sizeof(*task));
                task->pid = pid;
                task->node = node;
                if ((double)free_pages / nr_pages <= BOTTOM_SIZE)
                    task->warning = true;
                else
                    task->warning = false;

                if (pthread_create(&tids[tcount], NULL,
                                   reclaim_worker, task) == 0) {
                    tcount++;
                } else {
                    free(task);
                }
            }

            for (int i = 0; i < tcount; i++) {
                pthread_join(tids[i], NULL);
            }
        }

        sleep(3);
    }
}

int main()
{
    LOG_INFO("Starting NUMA hugepage reclaim (libvirt mode)...\n");
    monitor_and_reclaim();
    return 0;
}
