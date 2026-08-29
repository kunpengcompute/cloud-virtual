# Flexible Virtual Machine Topology Derivation User Guide

## 特性描述

### 简介

引入虚拟机拓扑自动推导，允许ARM用户在已知vCPU（virtual CPU，虚拟处理器）的绑核规则的条件下，自动按照绑定的pCPU（physical CPU，物理处理器）结构配置虚拟机内的拓扑。

传统方式下，虚拟机拓扑需通过sockets、clusters、cores、threads显式配置，当显式配置与vCPU实际绑定的物理拓扑不一致时，虚拟机OS无法正确划分调度域，多线程任务中容易产生跨CCL（CPU Core Cluster，处理器核簇）调度。本特性由qemu在虚拟机启动阶段根据vCPU绑核关系自动推导虚拟机拓扑，并据此构建虚拟机的PPTT（Processor Physical Topology Table，处理器物理拓扑表），使虚拟机内呈现的处理器拓扑与实际绑核的物理结构一致，从而正确划分调度域，减少跨CCL调度。

### 约束与限制

- 应用限制
    - 安装的libvirt和qemu可能替换系统版本，可能造成其他用户或者虚拟机的异常。
    - 补丁基于openEuler社区版本，使用其他版本的qemu或libvirt可能导致异常，需要用户根据实际情况进行评估。
    - 本特性仅适用于ARM架构（aarch64）平台。
    - 本特性要求qemu基于openEuler社区qemu v8.2.0版本并应用特性补丁，libvirt基于openEuler社区libvirt v9.10.0版本并应用特性补丁，补丁获取与编译安装请参见[软件编译](#软件编译)。
    - 当**auto_topology='yes'**且`<cputune>`中每个vCPU有且仅有一个绑定的pCPU时，才使用自动推导拓扑；条件不满足时特性降级，降级规则请参见[原理描述](#原理描述)。
    - auto_topology启用时，vCPU可能被分配在不同的NUMA（Non-Uniform Memory Access，非统一内存访问）节点上，需要用户按照实际的NUMA节点配置合适的内存绑定节点信息，且内存绑定规则需符合真实硬件结构。

#### 与其他特性的交互

本特性与其他特性无交互关系。

### 应用场景

在应用特性前，请先了解虚拟机拓扑灵活自动推导特性的应用场景。

- 场景一：保留部分cpu用于物理机事务。通常虚拟机不使用直通设备时，需要保留一定cpu算力用于处理虚拟设备的IO（Input/Output，输入输出）、中断，可根据实际情况保留2-4个cpu用于物理机事务，其余cpu绑定给虚拟机使用。
- 场景二：利用零散剩余处理器创建大规格虚拟机。物理机上已经分配多个虚拟机的情况，剩余的处理器很可能分散于多个CCL中，自动拓扑推导可以将物理cpu在同一CCL上的vCPU也分配在同一CCL中。

以上场景的完整配置示例请参见[使用效果](#使用效果)。

### 原理描述

虚拟机拓扑灵活自动推导的整体流程如下：

1. 用户在虚拟机xml中配置vCPU绑核信息（`<cputune>`），并在`<cpu>`小节中启用自动拓扑（`<topology auto_topology='yes'/>`）。
2. libvirt在虚拟机启动阶段解析xml，当满足自动推导条件时，通过QMP（QEMU Machine Protocol，QEMU机器协议）命令`set-vcpu-pinning`将vCPU与pCPU的绑核关系传递给qemu。
3. qemu收到后查询宿主机pCPU的CCL/socket拓扑信息，据此构建虚拟机的PPTT。
4. 虚拟机OS根据PPTT呈现的拓扑正确划分调度域，在多线程任务中减少跨CCL调度。

auto_topology可与sockets、clusters、cores、threads显式拓扑配置共存，显式拓扑作为降级回退配置：

- 当`auto_topology='yes'`且`<cputune>`中每个vCPU有且仅有一个绑定的pCPU时，使用自动推导拓扑，忽略显式拓扑配置。
- 当`auto_topology='yes'`但`<cputune>`条件不满足时，给出告警日志，降级为`auto_topology=no`，使用显式拓扑配置。
- 当`auto_topology='yes'`但未配置显式拓扑且`<cputune>`条件不满足时，将使用libvirt默认的拓扑配置。

## 软件编译

### 编译流程

本特性的编译分为qemu补丁和libvirt补丁两部分，每部分均包括获取基线版本、应用补丁、编译安装三个步骤，详细操作请参见[编译代码](#编译代码)。

### 配置编译环境

#### 环境要求

本特性无特殊编译环境要求，使用qemu、libvirt开源社区默认编译环境即可。

### 编译代码

#### 编译安装qemu补丁

qemu基于openEuler社区的qemu v8.2.0版本。

补丁地址：

- [qemu](https://atomgit.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0)
  - `0001-qemu-8.2.0-set-vcpu-pinning-qmp-command.patch`
  - `0002-qemu-8.2-build-pptt-auto-topology.patch`
  - `0003-hw-arm-virt-build-PPTT-vcpu-nodes-in-uniform-core-th.patch`

1. 获取基线版本。

    ```shell
    git clone https://gitcode.com/openeuler/qemu.git
    git checkout c4f98565a40cf88401640243525286597166cb86
    ```

2. 应用补丁。

    ```shell
    cd qemu
    git am /path/to/patches/*.patch
    ```

3. 编译安装。

    ```shell
    mkdir build
    cd build
    ../configure --target-list=aarch64-softmmu --disable-werror
    make -j 10
    make install
    ```

#### 编译安装libvirt补丁

libvirt基于openEuler社区的libvirt v9.10.0版本。

补丁地址：

- [libvirt](https://atomgit.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0)
  - `0001-auto-topology_support.patch`
  - `0002-new_qemu_qmp_command.patch`
  - `0003-auto-topology_xml_parse.patch`

1. 获取基线版本。

    ```shell
    git clone https://gitcode.com/openeuler/libvirt.git
    git checkout e3229f189aa3551316919de496dbc1c466b9a47a
    ```

2. 应用补丁。

    ```shell
    cd libvirt
    git am /path/to/patches/*.patch
    ```

3. 编译安装。

    ```shell
    meson setup build -Dsystem=true -Ddriver_qemu=enabled -Dc_args='-Wno-error=switch -Wno-error=switch-enum'
    ninja -C build install
    ```

## 部署软件/安装软件

### 环境要求

**硬件要求**

| 项目 | 要求 |
| ---- | ---- |
| 处理器 | 鲲鹏920新型号处理器或鲲鹏950处理器（本文以鲲鹏950处理器为例） |

**软件要求**

| 软件名称 | 软件版本 | 说明 |
| ------ | ------ | ---- |
| qemu | openEuler社区 qemu v8.2.0 + 特性补丁 | 补丁编译安装请参见[编译安装qemu补丁](#编译安装qemu补丁)。 |
| libvirt | openEuler社区 libvirt v9.10.0 + 特性补丁 | 补丁编译安装请参见[编译安装libvirt补丁](#编译安装libvirt补丁)。 |

### 获取软件

本特性软件为qemu与libvirt特性补丁，补丁地址如下：

- [qemu补丁](https://atomgit.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0)
- [libvirt补丁](https://atomgit.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0)

### 部署/安装虚拟机拓扑灵活自动推导

本特性无独立部署步骤，qemu与libvirt特性补丁编译安装完成后即完成部署，请参见[软件编译](#软件编译)。

## 使用特性

### 使能虚拟机拓扑灵活自动推导

libvirt通过解析虚拟机的xml配置接收外部输入，使能本特性需要完成vCPU绑核配置、自动拓扑配置和NUMA内存绑定配置。

**配置vCPU绑核信息和numa绑定信息**

用户需要配置vCPU的绑核信息，以下为8C虚拟机的xml配置示例：

```xml
<domain>
  <vcpu placement='static'>8</vcpu>
  <cputune>
    <vcpupin vcpu='0' cpuset='0'/>
    <vcpupin vcpu='1' cpuset='1'/>
    <vcpupin vcpu='2' cpuset='2'/>
    <vcpupin vcpu='3' cpuset='3'/>
    <vcpupin vcpu='4' cpuset='16'/>
    <vcpupin vcpu='5' cpuset='17'/>
    <vcpupin vcpu='6' cpuset='18'/>
    <vcpupin vcpu='7' cpuset='19'/>
  </cputune>
</domain>
```

**配置自动拓扑**

同时在xml的`<cpu>`小节中写入自动拓扑配置，需要同步配置numa的拓扑信息：

```xml
<domain>
  <cpu mode='host-passthrough' check='none'>
    <topology auto_topology='yes'/>
  </cpu>
</domain>
```

`auto_topology='yes'`表示拓扑从vCPU绑核信息自动推导，降级规则请参见[原理描述](#原理描述)。推荐配置方式（带降级回退）：

```xml
<domain>
  <cpu mode='host-passthrough' check='none'>
    <topology auto_topology='yes' sockets='1' clusters='1' cores='8' threads='1'/>
  </cpu>
</domain>
```

**配置NUMA内存绑定**

auto_topology启用时，按照绑核的配置，需要用户按照实际的NUMA节点去配置合适的内存绑定节点信息。

| vCPU id | vccl | vsocket | pCPU id | ccl id | socket id |
|---------|------|---------|---------|--------|-----------|
| 0~15 | 0 | 0 | 0~15 | 0 | 0 |
| 16~31 | 1 | 1 | 96~111 | 6 | 0 |

例如用户按照以上方法绑核时，vCPU会被分配在两个NUMA节点上，内存也应当分配在NUMA0和NUMA1两个节点上。需要libvirt检查用户的内存绑定规则是否符合真实硬件结构，配置合适的内存绑定节点。

```xml
<domain>
  ……
  <numatune>
    <memnode cellid='0' mode='strict' nodeset='0'/>
    <memnode cellid='1' mode='strict' nodeset='1'/>
  </numatune>

  <cpu mode='host-passthrough' check='none'>
    <topology auto_topology='yes'/>
    <numa>
        <cell id='0' cpus='0-15' memory='24' unit='GiB'/>
        <cell id='1' cpus='16-31' memory='24' unit='GiB'/>
    </numa>
  </cpu>
</domain>
```

### QMP接口说明

qemu通过QMP命令接收libvirt传递的vCPU绑核信息。

**新增QMP命令：`set-vcpu-pinning`**

libvirt在虚拟机启动阶段，通过该命令将vCPU与pCPU的绑核关系传递给qemu。
qemu收到后查询宿主机pCPU的CCL/socket拓扑信息，据此构建虚拟机的PPTT。

命令格式：

```json
{
  "execute": "set-vcpu-pinning",
  "arguments": {
    "auto-topology": true,
    "vcpu-pinning": [
      { "vcpu-id": 0, "pcpu-id": 0 },
      { "vcpu-id": 1, "pcpu-id": 1 },
      { "vcpu-id": 2, "pcpu-id": 2 },
      { "vcpu-id": 3, "pcpu-id": 3 },
      { "vcpu-id": 4, "pcpu-id": 4 },
      { "vcpu-id": 5, "pcpu-id": 5 },
      { "vcpu-id": 6, "pcpu-id": 6 },
      { "vcpu-id": 7, "pcpu-id": 7 },
      { "vcpu-id": 8, "pcpu-id": 16 },
      { "vcpu-id": 9, "pcpu-id": 17 },
      { "vcpu-id": 10, "pcpu-id": 18 },
      { "vcpu-id": 11, "pcpu-id": 19 },
      { "vcpu-id": 12, "pcpu-id": 20 },
      { "vcpu-id": 13, "pcpu-id": 21 },
      { "vcpu-id": 14, "pcpu-id": 22 },
      { "vcpu-id": 15, "pcpu-id": 23 }
    ]
  }
}
```

返回：

```json
{
  "return": {}
}
```

错误返回：

```json
{
  "error": {
    "class": "GenericError",
    "desc": "vcpu-id 8 is out of range"
  }
}
```

**参数说明：**

| 参数 | 类型 | 必选 | 说明 |
|------|------|------|------|
| auto-topology | bool | 是 | 是否启用灵活CCL拓扑，需与xml中**auto_topology='yes'**对应。 |
| vcpu-pinning | array | 是 | vCPU绑核映射数组。 |
| vcpu-pinning[].vcpu-id | int | 是 | 虚拟cpu编号，从0开始。 |
| vcpu-pinning[].pcpu-id | int | 是 | 绑定的物理cpu编号。 |

### 调度域说明

虚拟机调度域按 core、CCL、NUMA、整机四个层级构建：                                                                                                                                                   

1. core 调度域：当服务器开启超线程时，同一物理核下的两个超线程构成 core 调度域，调度仅发生在这两个超线程之间。
2. CCL 调度域：以 L3 缓存为基本单位，范围随服务器类型而不同。鲲鹏 920 新型号处理器上一个 NUMA 共享一个 L3 缓存，CCL 调度域范围即 NUMA；鲲鹏 950处理器上一个 cluster 共享一个 L3 缓存，CCL 调度域范围即 cluster。
3. NUMA 调度域：由虚拟机 XML 文件指定。注意此处的 NUMA 并非物理机上的 NUMA，而是 XML 中为虚拟机绑定的 NUMA——若在 XML 中绑定了 NUMA 范围，则会在每个NUMA 范围内生成对应层级的调度域。
4. 整机调度域：覆盖虚拟机绑定的全部 vCPU，为最高层级调度域。 

### 使用效果

以下为两个典型应用场景的完整配置示例及效果。

**场景一：保留部分cpu用于物理机事务**

```xml
<domain>
  ……
  <vcpu placement='static'>28</vcpu>
  <iothreads>4</iothreads>
  <cputune>
    <iothreadpin iothread='1' cpuset='0'/>
    <iothreadpin iothread='2' cpuset='1'/>
    <iothreadpin iothread='3' cpuset='2'/>
    <iothreadpin iothread='4' cpuset='3'/>
    <vcpupin vcpu='0' cpuset='4'/>
    <vcpupin vcpu='1' cpuset='5'/>
    <vcpupin vcpu='2' cpuset='6'/>
    <vcpupin vcpu='3' cpuset='7'/>
    <vcpupin vcpu='4' cpuset='8'/>
    <vcpupin vcpu='5' cpuset='9'/>
    <vcpupin vcpu='6' cpuset='10'/>
    <vcpupin vcpu='7' cpuset='11'/>
    <vcpupin vcpu='8' cpuset='12'/>
    <vcpupin vcpu='9' cpuset='13'/>
    <vcpupin vcpu='10' cpuset='14'/>
    <vcpupin vcpu='11' cpuset='15'/>
    <vcpupin vcpu='12' cpuset='16'/>
    <vcpupin vcpu='13' cpuset='17'/>
    <vcpupin vcpu='14' cpuset='18'/>
    <vcpupin vcpu='15' cpuset='19'/>
    <vcpupin vcpu='16' cpuset='20'/>
    <vcpupin vcpu='17' cpuset='21'/>
    <vcpupin vcpu='18' cpuset='22'/>
    <vcpupin vcpu='19' cpuset='23'/>
    <vcpupin vcpu='20' cpuset='24'/>
    <vcpupin vcpu='21' cpuset='25'/>
    <vcpupin vcpu='22' cpuset='26'/>
    <vcpupin vcpu='23' cpuset='27'/>
    <vcpupin vcpu='24' cpuset='28'/>
    <vcpupin vcpu='25' cpuset='29'/>
    <vcpupin vcpu='26' cpuset='30'/>
    <vcpupin vcpu='27' cpuset='31'/>
  </cputune>
  <numatune>
    <memnode='0' mode='strict' nodeset='0'/>
  </numatune>
  <cpu mode='host-passthrough' check='none'>
    <topology auto_topology='yes'/>
    <numa>
      <cell id='0' cpus='0-27' memory='8' unit='GiB'/>
    </numa>
  </cpu>
  ……
</domain>
```

鲲鹏 950 处理器（CCL = cluster）上启动时，预期有 core、CCL、NUMA、整机四层调度域：
                                                
1. core 域：只有当同一物理核下的两个超线程被绑定给一对 vCPU 时，这两个 vCPU 之间才会形成 core 调度域——如 vCPU 0-1 对应同一物理核的两个超线程、vCPU2-3 对应下一个物理核的两个超线程，以此类推，调度仅发生在这对超线程之间；若两个 vCPU 绑定在不同的物理核上，即使编号相邻也不会形成 core 调度域。       
2. CCL 域：950 的 CCL 即 cluster（一个 cluster 共享一个 L3 缓存），划分跟随物理 L3 边界——vCPU 0-11 属于一个 CCL，vCPU 12-27 属于另一个 CCL。
3. NUMA 域：由虚拟机 XML 文件中绑定的 NUMA（guest cell）决定，而非物理机 NUMA——本例 XML 只定义了一个 cell（cpus='0-27'），因此虚机 NUMA 域覆盖全部vCPU 0-27，与被物理 L3 切成两段的 CCL 域边界并不重合。
4. 整机域：覆盖全部 vCPU 0-27。                                                                                          

由于虚机 NUMA 域与整机域范围完全重复，NUMA 域被折叠，实际呈现 3 层调度域：core、CCL、整机。

鲲鹏 920 新型号处理器（CCL = NUMA）上启动时，预期同样有四层调度域：

1. core 域：形成条件同 950——仅存在于同一物理核下两个超线程对应的 vCPU 之间。
2. CCL 域：920 的 CCL 即 NUMA（一个 NUMA 共享一个 L3 缓存），全部 vCPU 0-27 落在同一 L3/NUMA 范围内，因此 CCL 域覆盖全部 vCPU。
3. NUMA 域：同样由 XML 中绑定的 guest cell 决定（非物理机 NUMA），本例单 cell 覆盖 vCPU 0-27。
4. 整机域：覆盖全部 vCPU 0-27。

由于 CCL 域、NUMA 域与整机域范围完全重复，三者合并为一层，实际呈现 2 层调度域：core、整机。

**场景二：利用零散剩余处理器创建大规格虚拟机**

```xml
<domain>
  ……
  <vcpu placement='static'>16</vcpu>
  <cputune>
    <vcpupin vcpu='0' cpuset='4'/>
    <vcpupin vcpu='1' cpuset='5'/>
    <vcpupin vcpu='2' cpuset='6'/>
    <vcpupin vcpu='3' cpuset='30'/>
    <vcpupin vcpu='4' cpuset='31'/>
    <vcpupin vcpu='5' cpuset='108'/>
    <vcpupin vcpu='6' cpuset='109'/>
    <vcpupin vcpu='7' cpuset='110'/>
    <vcpupin vcpu='8' cpuset='112'/>
    <vcpupin vcpu='9' cpuset='200'/>
    <vcpupin vcpu='10' cpuset='201'/>
    <vcpupin vcpu='11' cpuset='202'/>
    <vcpupin vcpu='12' cpuset='203'/>
    <vcpupin vcpu='13' cpuset='241'/>
    <vcpupin vcpu='14' cpuset='242'/>
    <vcpupin vcpu='15' cpuset='243'/>
  </cputune>
  <numatune>
    <memnode='0' mode='strict' nodeset='0'/>
    <memnode='1' mode='strict' nodeset='1'/>
    <memnode='2' mode='strict' nodeset='2'/>
  </numatune>
  <cpu mode='host-passthrough' check='none'>
    <topology auto_topology='yes'/>
    <numa>
      <cell id='0' cpus='0-4' memory='2' unit='GiB'/>
      <cell id='1' cpus='5-8' memory='2' unit='GiB'/>
      <cell id='2' cpus='9-15' memory='4' unit='GiB'/>
    </numa>
  </cpu>
  ……
</domain>
```

鲲鹏 950 处理器（CCL = cluster）上启动时，预期有 core、CCL、NUMA、整机四层调度域：

1. core 域：只有当同一物理核下的两个超线程被绑定给一对 vCPU 时才形成。本例形成配对的是：vCPU 0-1（pCPU 4,5）、vCPU 3-4（pCPU 30,31）、vCPU 5-6（pCPU108,109）、vCPU 9-10（pCPU 200,201）、vCPU 11-12（pCPU 202,203）、vCPU 14-15（pCPU 242,243）；而 vCPU 2、7、8、13 的超线程（pCPU7、111、113、240）未绑定给虚机，因此这 4 个 vCPU 不构成 core 域。 
2. CCL 域：950 的 CCL 即 cluster（一个 cluster 共享一个 L3 缓存），划分跟随物理 L3 边界——16 个 vCPU 分布在 6 个物理 cluster 上：vCPU 0-2、vCPU3-4、vCPU 5-7、vCPU 8、vCPU 9-12、vCPU 13-15 各属一个 CCL。
3. NUMA 域：由虚拟机 XML 文件中绑定的 NUMA（guest cell）决定，而非物理机 NUMA——本例定义了 3 个 cell：cell 0（vCPU 0-4）、cell 1（vCPU 5-8）、cell2（vCPU 9-15），生成 3 个 NUMA 域，其边界与物理 CCL 并不重合（如 cell 0 的 vCPU 0-4 横跨两个物理 cluster）。
4. 整机域：覆盖全部 16 个 vCPU。

四层范围互不重复，无折叠发生，实际呈现 4 层调度域：core、CCL、NUMA、整机。

鲲鹏 920 新型号处理器（CCL = NUMA）上启动时，预期同样有四层调度域：

1. core 域：形成条件与配对情况同 950。
2. CCL 域：920 的 CCL 即 NUMA（一个 NUMA 共享一个 L3 缓存），vCPU 按物理 NUMA 归属分为 3 段：vCPU 0-4（物理 node 0）、vCPU 5-8（物理 node 1）、vCPU9-15（物理 node 2）。
3. NUMA 域：同样由 XML 中绑定的 guest cell 决定（非物理机 NUMA），3 个 cell 的覆盖范围与 CCL 域完全一致。
4. 整机域：覆盖全部 16 个 vCPU。

由于 CCL 域与 NUMA 域范围完全重复，两者合并为一层，实际呈现 3 层调度域：core、CCL（NUMA）、整机。
