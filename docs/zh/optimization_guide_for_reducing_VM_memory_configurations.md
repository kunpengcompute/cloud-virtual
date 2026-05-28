# 虚拟机大页内存减配优化 特性指南

## 特性描述

### 简介

本文介绍如何在鲲鹏服务器中部署和使用虚拟机大页内存减配优化特性。

当前内存市场价格持续上涨，为降低虚拟机内存开销、提高物理内存利用率，需要通过技术手段优化内存配置。本特性利用ZRAM模块对虚拟机冷页内存进行压缩存储，并结合KAE硬件加速技术提升压缩效率，同时解决大页内存不支持swap的问题，实现内存资源的高效利用，从而在有限物理内存下部署更多虚拟机。

### 功能架构

虚拟机大页内存减配优化特性由ZRAM模块、KAE硬件加速引擎、内核大页内存swap系统和大页内存管理工具协同完成。

| 模块 | 功能 |
|--|--|
| ZRAM模块 | 提供大页内存swap后端，将swap回收冷页内存压缩存储以节省物理内存。 |
| KAE硬件加速 | 为ZRAM压缩/解压缩操作提供硬件级加速，提升压缩效率，降低CPU占用率。 |
| 大页内存管理工具 | 实现用户态大页内存冷热分级与主动swap回收。|
| 内核大页内存swap系统 | 提供大页内存支持swap的能力。|

## 环境要求

启用本特性之前，请确认软硬件环境满足要求。

**硬件要求**

| 项目 | 说明 |
|--|--|
| 处理器 | 鲲鹏920新型号处理器 |

**软件要求**

| 项目 | 版本或说明 |
|--|--|
| OS | `openEuler 2403 SP3` |
| 内核源码基线 | `OLK-6.6 6.6.0-132.0.0` |
| libvirt | `9.10.0（yum源）` |
| qemu | `8.2.0（yum源）` |
| redis | `6.2.0（自行安装，版本不作限制，文档以6.2.0为例）` |
| nginx | `1.24.0（自行安装，版本不作限制，文档以1.24.0为例）` |
| wrk | `4.1.0（自行安装，版本不作限制，文档以4.1.0为例）` |  

## 软件编译

### 配置编译环境

安装编译依赖。
```bash
yum -y install rpm-build openssl-devel bc rsync gcc gcc-c++ flex bison m4 git glib2-devel spice-protocol zlib-devel libffi-devel libgcrypt-devel libnfs-devel libiscsi-devel libseccomp-devel libaio-devel libcap-ng-devel librados2-devel librbd1-devel libfdt-devel libpng-devel spice-server-devel numactl-devel dwarves elfutils-libelf-devel ncurses-devel cmake make liburing-devel ninja-build autoconf automake libtool patch libvirt-devel
```

### 获取内核源码与patch
1. 获取内核源码。
```bash
cd /home/
git clone https://gitcode.com/nashzhou/kernel.git -b feat-hugetlb-2403-v12
cd kernel
```
2. 获取ZRAM支持KAEpatch。
```bash
cd /home/
git clone https://gitcode.com/lucky1e/cloud-virtual.git
```
3. 应用ZRAM支持KAEpatch。
```bash
cd /home/kernel
git apply /home/cloud-virtual/kernel/kernel-6.6.0/[zram]0001-zram-switch-to-async-acomp-and-mutex.patch
```


### 编译并安装内核

1. 准备编译config。
```bash
cd /home/kernel
cp /boot/config-6.6.0-132.0.0.111.oe2403sp3.aarch64 .config
vim .config
# 删除TRUSTED_KEYS
CONFIG_SYSTEM_TRUSTED_KEYS=""
```
2. 生成编译配置。
```bash
make menuconfig
```
弹出配置窗口后直接退出。

3. 编译内核，打包RPM包。
```bash
make rpm-pkg -j $(nproc)
```
4. 安装新内核。（RPM包的路径以实际为准）
```bash
rpm -ivh /root/rpmbuild/RPMS/aarch64/kernel-6.6.0+-1.aarch64.rpm --force
rpm -ivh /root/rpmbuild/RPMS/aarch64/kernel-devel-6.6.0+-1.aarch64.rpm --force
```
5. 编辑 `/etc/grub2-efi.cfg` 文件，添加大页内存参数，大页数量根据实际环境调整。

```text
"default_hugepagesz=2M hugepagesz=2M hugepages=131072 hugetlb_swap=on"
```
6. 重启物理机，更换新内核，使配置生效。

```bash
reboot
```

### 编译安装KAE
1. 获取KAE源码。
```bash
cd /home/
git clone https://gitcode.com/boostkit/KAE.git -b kae2
cd KAE
```
2. 编译安装KAE。
```bash
sh build.sh all
```
3. 执行以下命令检查KAE安装是否成功。
```bash
ll /sys/class/uacce/
```
![KAE安装成功示例](figures/zh-cn_image_0000002518691588.png)

### 获取memlink工具

1. 获取memlink源码。
```bash
cd /home/
git clone https://gitcode.com/leizongkun/memlink_6340.git -b dev
```
2. 编译源码。

```bash
cd memlink_6340
yum-builddep memlinkd.spec
tar jcvf memlinkd.tar.bz2 --exclude=.git src
mkdir -p /root/rpmbuild/SOURCES/
cp memlinkd.tar.bz2 /root/rpmbuild/SOURCES/
rpmbuild -ba memlinkd.spec
```
3. 安装memlink SDK。

```bash
cd /root/rpmbuild/RPMS/aarch64/;rpm -ivh memlinkd-*
```

### 获取大页内存管理工具
该工具仅作为功能演示工具，提供大页内存冷热分级与主动swap回收功能的使用方法以及基本的内存管理策略，不建议直接在生产环境中使用，用户可以参考该工具，结合实际使用场景进行定制化配置。

1. 获取reclaim源码。
```bash
git clone https://gitcode.com/lucky1e/cloud-virtual.git
```
2. 编译源码。

```bash
cd cloud-virtual/tools/
gcc *reclaim.c -o reclaim  $(pkg-config --cflags --libs glib-2.0) -lvirt -lnuma -lmemlink_sdk
```

## 安装软件

### host软件安装

安装libvirt/qemu相关软件包。

```bash
yum install -y libvirt qemu edk2-aarch64
```

### 虚拟机软件安装

安装redis、nginx、wrk等相关软件包。

```bash
yum install -y redis nginx wrk
```

## 使用特性

### 配置大页数量

测试过程中，可以根据需要动态调整每个NUMA节点的大页数量（例如，调整node0的大页数量为10000）。
```bash
echo 10000 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```
### 配置虚拟机

1. 编辑虚拟机XML配置文件，添加大页内存支持，开启大页pod选项，启用大页动态分配，并对虚拟机进行绑核/绑内存（核心与内存绑定需要根据实际情况配置）。

|标签名|说明|
|--|--|
|hugepages|添加大页内存支持。|
|hugepage-ondemand|开启大页pod选项，支持大页动态分配，无此配置默认关闭。|
|cputune|对虚拟机进行绑核。|
|numatune|对虚拟机进行绑内存。|

```xml
<memoryBacking>
  <hugepages>
    <page size='2048' unit='KiB'/>
  </hugepages>
  <allocation mode='hugepage-ondemand'/>
</memoryBacking>
<cputune>
  <vcpupin vcpu='0' cpuset='10'/>
  <vcpupin vcpu='1' cpuset='11'/>
  <emulatorpin cpuset='10-11'/>
</cputune>
<numatune>
  <memnode cellid='0' mode='strict' nodeset='0'/>
</numatune>
<cpu mode="host-passthrough" check="none">
  <numa>
    <cell id='0' cpus='0-1' memory='8388608' unit='KiB' memAccess='shared'/>
  </numa>
</cpu>
```
2. 编辑虚拟机XML配置文件，配置虚拟机内存回收策略标签。所有虚拟机都需要配置此标签，通过标签内容区分是否允许回收。
注意：回收标签为自定义元数据标签，文档给出的仅为示例，用户可根据实际情况自定义标签名称，并结合大页内存回收工具使用。

- **允许内存回收**（推荐配置）：
```xml
<metadata>
  <reclaim xmlns="http://reclaim.io"/>
</metadata>
```

- **禁止内存回收**：
```xml
<metadata>
  <no_reclaim xmlns="http://reclaim.io"/>
</metadata>
```

### 启动memlink工具

启动memlink工具。

```bash
modprobe etmem_scan
systemctl start memlinkd
```
### 启用KAE加速

1. 加载KAE内核模块。

```bash
rmmod hisi_zip
rmmod hisi_sec2
rmmod hisi_hpre
rmmod hisi_qm
rmmod uacce
modprobe hisi_zip uacce_mode=2 perf_mode=1 pf_q_num=256
```

2. 验证KAE加速是否生效。
执行以下命令，观察KAE队列是否存在，队列数是否与配置的队列数一致。

```bash
watch -n 1 "cat /sys/class/uacce/hisi_zip-*/available_instances"
```
![KAE加速生效示例](figures/zh-cn_image_0000002518691590.png)



### 配置ZRAM与swap接口

```bash
modprobe etmem_swap
```
注意：zram快设备大小根据实际大页内存大小配置，建议配置为大页内存的50%。
```bash
modprobe zram
echo deflate > /sys/block/zram0/comp_algorithm
echo 32G > /sys/block/zram0/disksize
mkswap /dev/zram0
swapon -p 100 /dev/zram0
```
配置完成后检查KAE队列数量是否发生变化，若队列数量对比配置前减少，说明KAE加速已生效。
```bash
watch -n 1 "cat /sys/class/uacce/hisi_zip-*/available_instances"
```
### 启动大页内存管理工具

```bash
./reclaim
```

## 启动虚拟机开始测试

### 单虚拟机超分测试
该测试仅用于可靠性测试，以8U32G虚拟机为例，配置大页内存为12288页（24GB），虚拟机内存为32GB（注：如果虚拟机使用dpdk/spdk等大页内存应用，大页数量配置时应预留出应用所需的大页数量，目前dpdk/spdk应用的内存申请不受控，可能会出现跨NUMA直接申请大页内存的情况）。
1. 配置大页数量为12288页（24GB）。
```bash
echo 12288 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```
2. 启动虚拟机。

```bash
virsh start vm_name
```

3. 在虚拟机内部启动redis或nginx服务。
```bash
systemctl start redis
```
或
```bash
systemctl start nginx
```

4. 在物理机执行以下命令观测剩余大页数量。
```bash
watch -n 1 "cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/free_hugepages"
```

5. 在客户端启动压测工具，对redis或nginx服务进行压测。
```bash
redis-benchmark -h \<虚拟机IP地址\> -p 6379 -t set -c 1000 -r 10000000 -n 10000000 -d 1024
```
或
```bash
wrk -H "Connection: Close" -t 10 -c 8000 -d 60s http://\<虚拟机IP地址\>:\<端口号\>/index.html
```

6. 记录压测结果，对比内存不超分情况，在剩余大页数量大于大页总量10%前，压测性能下降小于15%。

7. 观察剩余大页内存数量小于大页总量20%时，是否触发大页内存主动swap回收，以下两种方式都可以观察到，同时注意reclaim工具的日志。
```bash
watch -n 1 "swapon --show"
```
![image.png](https://raw.gitcode.com/user-images/assets/9882662/85fa1eec-1006-4881-a39b-de41e2a60bdc/image.png 'image.png')
```bash
watch -n 1 "cat /proc/meminfo | grep Huge"
```
![image.png](https://raw.gitcode.com/user-images/assets/9882662/a73c2a2c-7844-4fe0-a0c3-87e71fd0a768/image.png 'image.png')


8. 在虚拟机内观测到虚拟机内存占用超过24G后，查看物理机剩余大页数量出现下降至0的情况时，观测虚拟机是否能正常使用；继续压测，虚拟机内部内存占用是否持续上升；此状态下不再保证虚拟机的性能。

### 多虚拟机超分测试
以单NUMA 60G大页为例，配置单个NUMA节点的大页数量为30720页（60GB），启动三台虚拟机，内存规格分别为16G、32G、32G。
1. 配置大页数量为30720页（60GB）。
```bash
echo 30720 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```
2. 启动虚拟机。
```bash
virsh start vm_name1
virsh start vm_name2
virsh start vm_name3
```
3. 在每个虚拟机内部启动redis或nginx服务。
```bash
systemctl start redis
```
或
```bash
systemctl start nginx
```
4. 在物理机执行以下命令观测剩余大页数量。
```bash
watch -n 1 "cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/free_hugepages"
```
5. 在客户端启动压测工具，对redis或nginx服务进行压测。
```bash
redis-benchmark -h \<虚拟机IP地址\> -p 6379 -t set -c 1000 -r 10000000 -n 10000000 -d 1024
```
或
```bash
wrk -H "Connection: Close" -t 10 -c 8000 -d 60s http://\<虚拟机IP地址\>:\<端口号\>/index.html
```
6. 记录压测结果，对比内存不超分情况，在剩余大页数量大于大页总量10%前，压测性能下降小于15%。

7. 观察剩余大页内存数量小于大页总量20%时，是否触发大页内存主动swap回收，以下两种方式都可以观察到，同时注意reclaim工具的日志。
```bash
watch -n 1 "swapon --show"
```
```bash
watch -n 1 "cat /proc/meminfo | grep Huge"
```

8. 在虚拟机内观测到虚拟机内存占用超过64G后，查看物理机剩余大页数量出现下降至0的情况时，观测虚拟机是否能正常使用；继续压测，虚拟机内部内存占用是否持续上升；此状态下不再保证虚拟机的性能。

9. 建议在剩余大页内存数量低于大页总量10%时，挑选低压力虚拟机进行热迁移，以释放大页内存给高压力虚拟机使用，实际使用中可根据场景和需求进行调整迁移阈值，避免陷入内存不足OOM的情况。任何场景下，物理机陷入OOM情况，都无法再保证虚拟机性能。
