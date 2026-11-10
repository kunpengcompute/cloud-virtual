# 虚拟化pvspinlock 特性指南

本文档介绍虚拟化pvspinlock特性，通过半虚拟化队列自旋锁将等待锁的vCPU从自旋等待改为睡眠加hypervisor定向唤醒，避免超分场景下CPU空转与调度延迟，适用于需要在KVM虚拟化环境中启用该特性的用户。

## 特性描述

### 简介

ARM架构的qspinlock在guest虚拟机内部自旋等待时，依赖WFE（Wait For Event）指令实现低功耗等待。但在 KVM 虚拟化环境下，这一机制存在明显缺陷。传统的qspinlock处理机制导致guest执行WFE指令时，可能被当作空操作处理，导致等待锁的vCPU持续空转或者WFE指令陷入到kypervisor层，由hypervisor让渡时间片，但是被唤醒的vCPU可能被调度去执行与锁无关的任务导致持有锁的vCPU无法及时释放锁，唤醒等待的vCPU，导致在超分场景下，CPU效率低下，调度延迟较高，影响业务性能。为此，本特性通过实现半虚拟化队列的自旋锁，使得锁持有锁释放锁以及发现队列头部的vCPU处于halted状态时，通过hypercall主动踢醒目标vCPU，避免被动等待调度，让等待锁的vCPU能够尽快获得运行机会。把guest的排队自旋锁慢路径从“自旋+一致性内存访问轮询”改为“睡眠（WFI）+由hypervisor定向唤醒”，避免大量vCPU同时自旋抢占同一把锁造成pCPU空转与cache抖动。

## 环境要求

启用本特性之前，请确认软硬件环境满足要求。

**硬件要求**

| 项目 | 说明 |
|--|--|
| 处理器 | - 鲲鹏920处理器 <br> - 鲲鹏920新型号处理器 <br> - 鲲鹏950处理器|

**操作系统与软件要求**

| 项目 | 版本或说明 | 获取地址 |
|--|--|--|
| OS | openEuler 24.03 LTS SP4 | [openEuler-24.03-LTS-SP4.iso](https://www.openeuler.openatom.cn/zh/download/#openEuler%2024.03%20LTS%20SP4) |
| host kernel | 6.6.0-180.0.0.159 及以上  |[日构建内核](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP5/openeuler-2026-11-09-02-08-51/OS/aarch64/Packages/) |
| guest kernel | 6.6.0-180.0.0.159 及以上  |[日构建内核](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP5/openeuler-2026-11-09-02-08-51/OS/aarch64/Packages/) |
| libvirt | 9.10.0-49及以上（yum源） | [日构建libvirt](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP5/openeuler-2026-11-09-02-08-51/OS/aarch64/Packages/libvirt-9.10.0-50.oe2403sp5.aarch64.rpm)|
| qemu | 8.2.0-96及以上（yum源） | [日构建qemu](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP5/openeuler-2026-11-09-02-08-51/OS/aarch64/Packages/qemu-8.2.0-97.oe2403sp5.aarch64.rpm)|

## 软件约束

guest内核需要支持pvspinlock特性，合入对应补丁。

## 软件安装和配置

1. 安装支持pvspinlock的host内核。

    ```bash
    rpm -ivh kernel-6.6.0-180.0.0.159-xxx.rpm
    ```

2. 安装libvirt/qemu等相关软件包。

    ```bash
    yum install -y libvirt qemu edk2-aarch64
    ```

3. 启动libvirtd服务。

    ```bash
    systemctl start libvirtd
    ```

4. 配置虚拟机使能pvspinlock特性。

    ```xml
    ...
    <features>
        <acpi/>
        <pvspinlock state='on'/>
        <gic version='3'/>
    </features>
    ```

5. 重启物理机选择支持特性的内核。

## 特性验证

1. 启动虚拟机。

    ```bash
    virsh create <vm_name>.xml --console
    ```

2. 进入虚拟机内部查看dmesg日志观察pvspinlock是否使能。

    ```bash
    dmesg -T | grep 'spinlock'
    ```

    如回显提示"arm-pv: PV qspinlocks enabled"则说明特性成功使能。

## 特性补丁

| 组件名称 | PR 链接 |
|---------|---------|
| kernel  | <https://atomgit.com/openeuler/kernel/pull/26044> |
| QEMU    | <https://atomgit.com/openeuler/qemu/pull/2008> |
| libvirt | <https://atomgit.com/openeuler/libvirt/pull/444> |

## 修订记录

|文档版本|发布日期|修改说明|
|-------|-------|-------|
|01|2026-12-30|第一次正式发布。|
