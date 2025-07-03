# Boostkit_Virtualization

#### 介绍
鲲鹏云计算虚拟化代码仓，存放鲲鹏BoostKit虚拟化加速套件的相关补丁。鲲鹏BoostKit虚拟化加速套件覆盖kvm、qemu、libvirt、dpdk、spdk等软件栈。当前仓库涵盖虚拟机跨代热迁移、L0内存加速、DPDK队列选择减小网络虚拟化损耗、SPDK中断聚合减小存储虚拟化损耗等特性补丁。
目录结构如下：
组件名称
 -- 组件版本
 ----组件patch
组件patch命名规范如下：
【特性类别】-【patch序号】-【patch名称】
例如：
[live-migration]0001-KVM-arm64-Allow-userspace-to-get-the-writable-masks-.patch

#### 操作系统

1. 虚拟机热迁移特性限定openEuler24.03 LTS SP1系统，6.6.0-72.0.0版本内核，qemu 8.2.0版本。
2. L0加速openvswtich内核态流表匹配特性 适用操作系统为openEuler22.03 LTS SP4，内核需要能够支持L0内存。
3. DPDK队列选择特性，适用操作系统openEuler 24.03 LTS SP1
4. SPDK中断聚合特性，使用操作系统openEuler 24.03 LTS SP1

#### 使用说明
以下内容针对本仓库中涉及的特性提供简单的使用说明，详细使用见鲲鹏社区对应特性的用户指南/使用指导。

1.  虚拟机跨代热迁移特性
    使用步骤如下：
    （1）到openEuler仓库拉取kernel、qemu代码
    （2）切换到对应分支，使用tools目录中的脚本完成patch应用
    （3）详细操作步骤见鲲鹏社区 https://support.huawei.com/enterprise/zh/doc/EDOC1100484197/35a9145e

2.  L0加速openvswtich内核态流表匹配特性
    使用步骤如下：
    （1）、拉下5.10.216源码仓
    git clone https://gitee.com/openeuler/kernel.git -b 5.10.0-216.0.0 --depth=1
    （2）、将补丁保存为.patch文件
    （3）、合入补丁
    git am --reject <补丁名称>
    （4）、编译openvswitch模块
    在../kernel/net/openvswitch
    make CONFIG_XENO_DRIVERS_NET_DRV_IGB=m -C <源码目录> M=pwd modules

1.  DPDK队列训责特性使用步骤如下：
    使用步骤如下：
    （1）git拉取dpdk 24.11源代码
    （2）合入DPDK队列选择patch
    （3）编译安装dpdk 24.11
    鲲鹏社区链接：待630版本发布后补充

3.  SPDK中断聚合特性使用步骤如下：
    使用步骤如下：
    （1）git拉取SPDK 24.05源代码
    （2）合入SPDK中断聚合patch
    （3）编译安装dpdk 24.05
    鲲鹏社区链接：待630版本发布后补充

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request

