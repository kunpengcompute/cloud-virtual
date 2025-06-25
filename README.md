# Boostkit_Virtualization

#### 介绍
鲲鹏云计算虚拟化代码仓，存放Boostkit虚拟化加速相关补丁，包含虚拟机跨代热迁移特性补丁。

#### 操作系统

1. 虚拟机热迁移特性限定openEuler24.03 LTS SP1系统，6.6.0-72.0.0版本内核，qemu 8.2.0版本。
2. L0加速openvswtich内核态流表匹配特性 适用操作系统为openEuler22.03 LTS SP4，内核需要能够支持L0内存。

#### 使用说明

1.  L0加速openvswtich内核态流表匹配特性
    使用步骤如下：
    1、拉下5.10.216源码仓
    git clone https://gitee.com/openeuler/kernel.git -b 5.10.0-216.0.0 --depth=1
    2、将补丁保存为.patch文件
    3、合入补丁
    git am --reject <补丁名称>
    4、编译openvswitch模块
    在../kernel/net/openvswitch
    make CONFIG_XENO_DRIVERS_NET_DRV_IGB=m -C <源码目录> M=pwd modules

2.  xxxx
3.  xxxx

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request

