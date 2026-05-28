# 虚拟机缓存信息配置 用户指南

## 功能介绍

本功能通过虚拟机用户设置虚拟机缓存信息，向虚拟机中透传相信缓存信息，使得虚拟机OS内核能够正确识别CCL级别调度域，改善内核调度效率。

## 运行环境

启用本特性之前，请确认软硬件环境满足要求。

**硬件要求**

| 项目 | 说明 |
|--|--|
| 处理器 | 鲲鹏920新型号处理器或鲲鹏950处理器 |

**软件要求**

| 项目 | 版本或说明 |
|--|--|
| OS | openEuler 2403 SP3 |
| libvirt | 9.1.0（yum源） |
| qemu | 8.2.0（yum源） |

## 软件编译

### 获取补丁

| 项目 | 补丁链接 |
|--|--|
| libvirt | `https://gitcode.com/openeuler/libvirt/pull/429` |
| qemu | `https://gitcode.com/openeuler/qemu/pull/1906` |

### 编译qemu

1. 下载qemu源码

```bash
git clone https://gitcode.com/lixuyuan7/qemu.git -b qemu-8.2.0
```

2. 编译qemu

```bash
mkdir -p build && cd build
../configure --disable-docs --target-list=aarch64-softmmu --disable-werror
make -j 100
```

3. 确认编译产物

```bash
ls build/qemu-system-aarch64
```

若文件存在，则编译成功。

### 编译libvirt

1. 下载libvirt源码

```bash
git clone https://gitcode.com/lixuyuan7/libvirt.git -b libvirt-9.10.0
```

2. 编译libvirt

```bash
meson setup build --prefix=/usr -Ddocs=disabled
ninja -C build -j 100
ninja -C build install
```

3. 确认安装结果

```bash
virsh version
```

若显示的libvirt版本为编译版本，则安装成功。

## 软件使用

**通过libvirt XML配置缓存信息**

1. 编辑虚拟机配置文件

```bash
virsh edit <vm name>
```

参考如下示例配置虚拟机缓存信息：

```xml
    ...
    <devices>
      <emulator>"使用之前编译好的qemu-system-aarch64文件路径"</emulator>
    </devices>
    <cpu mode='host-passthrough' migratable='off'>
      <topology sockets='2' dies='1' clusters='2' cores='8' threads='2'/>
      <cacheinfo cache='l1i' topology='core' size='131072' sets='512' associativity='4' line='64'/>
      <cacheinfo cache='l1d' topology='core' size='65536' sets='256' associativity='4' line='64'/>
      <cacheinfo cache='l2' topology='core' size='1048576' sets='2048' associativity='8' line='64'/>
      <cacheinfo cache='l3' topology='cluster' size='23855104' sets='16384' associativity='19' line='64'/>
    </cpu>
    ...
```

参数说明：

| 参数 | 说明 |
|--|--|
| cache | 缓存层级和类型，格式为Lx[i/d]，x表示缓存层级，[i/d]表示指令缓存或数据缓存，不写表示统一缓存 |
| topology | 缓存绑定的处理器层级，可选thread、core、cluster、die、socket；同一层级上包含多个不同level的缓存时，表示它们之间存在next level关系 |
| size | 缓存大小（字节） |
| sets | 缓存组数 |
| associativity | 缓存关联度（ways数） |
| line | 缓存行大小（字节） |

输入的参数不完整时，部分信息按照鲲鹏950的物理缓存提供默认参数。cache参数为必选，其余参数为可选。topology参数的"die"选项由于qemu不支持，目前仅做预留。

2. 启动虚拟机

```bash
virsh start <vm name> --console
```

3. 验证配置是否生效，若打印信息中出现Caches一栏，则表示配置成功

```bash
lscpu
```

## 注意事项

* 缓存信息配置功能仅支持KVM虚拟化，且使用qemu作为虚拟化平台。
* 缓存信息通过ACPI注入虚拟机，理论上可支持ACPI平台虚拟化，但当前仅对ARM架构生效。
* 不支持非对称处理器的硬件平台注入缓存信息。
* 功能需要向vcpu注入CLIDR寄存器，需确保内核包含特定版本补丁。
* guest OS的内核配置需打开相关调度域功能开关。
* 下一级缓存存在时，上一级缓存必须存在，否则虚拟机启动时会报错并停止。
* 若使用的qemu不支持缓存信息参数，将触发默认报错机制并停止虚拟机启动。