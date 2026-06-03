# SGI注入亲和性优化 用户指南

## 介绍

### 简介

本文介绍如何在鲲鹏950服务器中部署和使用SGI注入亲和性优化特性。

在ARM64 KVM虚拟化中，虚拟机通过写ICC_SGI1R_EL1寄存器向目标vCPU发送SGI（Software Generated Interrupt，软件生成中断）。当Guest发送SGI时，KVM需要在宿主机侧根据亲和性信息计算目标MPID，再按照MPID遍历所有vCPU逐一匹配，时间复杂度为O(n)。当vCPU数量较多时，每次SGI投递都需要全量遍历，成为性能瓶颈。

SGI注入亲和性优化特性通过在VM首次运行时预计算MPIDR到vCPU索引的压缩映射表（`kvm_mpidr_data`），将SGI分发路径从O(n)遍历优化为O(1)直接查找，显著降低SGI投递延迟。

### 功能架构

SGI注入亲和性优化特性由MPIDR映射初始化模块、快速查找模块和SGI分发路径优化模块协同完成。

| 模块 | 功能 |
|--|--|
| MPIDR映射初始化（`kvm_init_mpidr_data`） | 在VM首次运行时，分析所有vCPU的MPIDR亲和性，提取差异位构造压缩映射表，存入`kvm->arch.mpidr_data`。 |
| 快速查找（`kvm_mpidr_index` / `kvm_mpidr_to_vcpu`） | 利用压缩映射表，将MPIDR亲和性直接转换为vCPU索引，无需遍历。 |
| SGI分发路径优化（`vgic_v3_dispatch_sgi`） | 重写SGI分发逻辑，将原来的全量遍历改为按目标位图逐个直接查找，并提取`vgic_v3_queue_sgi`辅助函数。 |

典型流程如下。

1. VM首次运行时，`kvm_init_mpidr_data`分析所有vCPU的MPIDR，构造压缩映射表。
2. Guest写ICC_SGI1R_EL1触发SGI分发。
3. KVM根据亲和性信息计算目标MPID，通过映射表直接查找对应vCPU。
4. 调用`vgic_v3_queue_sgi`向目标vCPU投递中断。

## 环境要求

启用本特性之前，请确认软硬件环境满足要求。

**硬件要求**

| 项目 | 说明 |
|--|--|
| 处理器 | 鲲鹏950处理器 |

**软件要求**

| 项目 | 版本或说明                    |
|--|--------------------------|
| OS | `openEuler 2403 LTS SP3` |
| 内核源码基线 | `OLK-6.6 6.6.0-135.0.0`  |

## 获取并合入SGI优化补丁

### 获取补丁

1. SGI优化补丁获取链接如下。

   ```text
   https://gitcode.com/boostkit/cloud-virtual/tree/master/kernel/kernel-6.6.0
   ```

2. 需要获取以下补丁文件。

   `[kvm]arm64:vgic-v3:Optimize_affinity-based_SGI_injection.patch`

### 获取目标内核源码

1. 克隆openEuler内核源码并切换到`OLK-6.6`分支，确保源码包含tag 6.6.0-135.0.0。

   ```bash
   git clone https://gitcode.com/openeuler/kernel.git -b OLK-6.6 --depth=1
   cd kernel
   git branch --show-current
   ```

2. 期望输出。

   ```text
   OLK-6.6
   ```

### 合入补丁

1. 合入补丁前，建议确认源码目录干净。

   ```bash
   git status --short
   ```

    若命令没有输出，表示当前工作区没有未提交修改。

2. 在内核源码根目录执行以下命令，合入SGI优化补丁。

   ```bash
   git apply ~/sgi-patch/[kvm]arm64:vgic-v3:Optimize_affinity-based_SGI_injection.patch
   git log --oneline -n 1
   ```

   期望最新一条提交类似如下。

   ```text
   <newest> KVM: arm64: vgic-v3: Optimize affinity-based SGI injection
   ```

## 编译并安装内核

### 编译前准备

开始编译前，请确保系统已安装openEuler内核RPM构建依赖，且构建目录有足够空间保存源码、中间文件和RPM包。可再次确认当前源码状态。

```bash
git status --short
```

### 编译config文件

使用openEuler默认内核配置生成编译所需的.config文件。

```bash
make openeuler_defconfig
```

### 编译RPM
在内核源码根目录执行。构建时间与服务器配置、内核配置和并发数有关。构建完成后，RPM包通常生成在源码目录的上一级目录。

```bash
make binrpm-pkg -j$(nproc)
```

### 安装RPM

1. 根据实际生成的RPM文件名安装新内核包。以下命令仅为示例，请在执行前确认内核RPM包是本次构建产物。

   ```bash
   sudo rpm -ivh <内核rpm名称> --force
   ```

2. 安装完成后，确认已安装的内核。

   ```bash
   rpm -qa | grep '^kernel' | sort
   ```

3. 重启系统使新内核生效。

   ```bash
   sudo reboot
   ```

4. 系统重启后，确认当前运行的内核版本已经切换到新安装版本。

   ```bash
   uname -r
   ```

## 使用SGI注入亲和性优化特性

本特性在内核层面自动生效，无需用户额外配置。安装包含本特性的内核并重启后，KVM虚拟机的SGI分发路径自动使用优化后的快速查找逻辑。

### 验证特性是否生效

1. 确认当前内核版本已更新。

   ```bash
   uname -r
   ```

2. 启动虚拟机。

   ```bash
   virsh start <vm name> --console
   ```

3. 在虚拟机内触发SGI（如通过`ipistat`或内核调度活动），观察SGI投递延迟是否降低。

4. 可在宿主机上通过ftrace或perf观察`vgic_v3_dispatch_sgi`函数的执行时间是否显著缩短。

## 注意事项

* 本特性仅影响KVM宿主机侧的SGI分发路径，不改变Guest可见行为，对Guest操作系统透明。
* 当GICv4.1硬件IPIv启用时，Guest的SGI由硬件直接处理，不经过KVM软件路径，本优化不会生效。
