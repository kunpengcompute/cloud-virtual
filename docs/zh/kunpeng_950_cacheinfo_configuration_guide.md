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
| qemu | 8.2.0 |
| libvirt | 9.1.0 |

## 软件编译

### 获取补丁
cacheinfo补丁获取链接如下：

| 项目 | 补丁链接 |
|--|--|
| qemu | `https://gitcode.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0` |
| libvirt | `https://gitcode.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0` |

libvirt需要获取以下11个补丁文件，并保持应用顺序不变。

1. `[cacheinfo-configuration]0001-libvirt-Support-specifying-the-cache-size-presented-.patch`               
2. `[cacheinfo-configuration]0002-fix-some-enumeration-value-not-handled-in-switch.patch`
3. `[cacheinfo-configuration]0003-libvirt-add-cacheinfo-parmater-details-support.patch`                     
4. `[cacheinfo-configuration]0004-libvirt-add-cacheinfo-parmater-UT.patch`
5. `[cacheinfo-configuration]0005-libvirt-rename-cacheinfo-name-to-cache-extract-macro.patch`
6. `[cacheinfo-configuration]0006-libvirt-fix-missing-rename-and-enum-cases.patch`
7. `[cacheinfo-configuration]0007-libvirt-eliminate-void-parameter-in-cpuTestCacheInfo.patch`
8. `[cacheinfo-configuration]0008-libvirt-change-QEMU-smp-cache-command-line-parameter.patch`
9. `[cacheinfo-configuration]0009-libvirt-rename-cacheinfo-location-to-topology-add-mo.patch`
10. `[cacheinfo-configuration]0010-libvirt-fix-Wshadow-error-by-renaming-local-variable.patch`
11. `[cacheinfo-configuration]0011-libvirt-fix-symbol-sorting-in-libvirt_private.syms-m.patch`

qemu需要获取以下12个补丁文件，并保持应用顺序不变。

1. `[cacheinfo-configuration]0001-qapi-qom-Define-cache-enumeration-and-properties-for.patch`
2. `[cacheinfo-configuration]0002-qemu-Support-specifying-the-cache-size-presented-to-.patch`
3. `[cacheinfo-configuration]0003-qemu-arm64-resolve-code-conflicts.patch`
4. `[cacheinfo-configuration]0004-qemu-arm64-add-ut-for-smp-cache-info.patch`
5. `[cacheinfo-configuration]0005-qemu-arm64-fix-ut-for-smp-cache-info.patch`
6. `[cacheinfo-configuration]0006-qemu-arm64-add-SMP-cache-topology-and-PPTT-support.patch`
7. `[cacheinfo-configuration]0007-qemu-arm64-fix-lint-DEFINE_TYPES-and-magic-numbers-i.patch`
8. `[cacheinfo-configuration]0008-qemu-arm64-fix-lint-extract-smp_cache-init-and-remov.patch`
9. `[cacheinfo-configuration]0009-qemu-arm64-remove-CacheBindProcessHierarchy-enum-and.patch`
10. `[cacheinfo-configuration]0010-qemu-arm64-add-cache_supported-for-all-cache-levels-.patch`
11. `[cacheinfo-configuration]0011-qemu-arm64-fix-CLIDR-injection-write-to-cpreg_values.patch`
12. `[cacheinfo-configuration]0012-qemu-arm64-move-CLIDR-injection-after-kvm_arm_writab.patch`

### 获取qemu源码和libvirt源码
1. 切换到qemu-8.2.0分支并克隆qemu源码。
    ```bash
    git clone https://gitcode.com/openeuler/qemu.git -b qemu-8.2.0
    ```
2. 切换到libvirt-9.10.0分支并克隆libvirt源码。
    ```bash
    git clone https://gitcode.com/openeuler/libvirt.git -b libvirt-9.10.0
    ```

### 合入qemu和libvirt补丁
1. 合入补丁前，建议确认源码目录干净，若命令没有输出，表示当前工作区没有未提交修改。

   ```bash
   git status --short
   ```
2. 在qemu源码根目录执行以下命令，按邮件补丁流合入VF ITS补丁：
   ```bash
    git am --reject ~/patch目录/[cacheinfo-configuration]0001-qapi-qom-Define-cache-enumeration-and-properties-for.patch                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0002-qemu-Support-specifying-the-cache-size-presented-to-.patch                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0003-qemu-arm64-resolve-code-conflicts.patch                                            
    git am --reject ~/patch目录/[cacheinfo-configuration]0004-qemu-arm64-add-ut-for-smp-cache-info.patch                                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0005-qemu-arm64-fix-ut-for-smp-cache-info.patch                                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0006-qemu-arm64-add-SMP-cache-topology-and-PPTT-support.patch                           
    git am --reject ~/patch目录/[cacheinfo-configuration]0007-qemu-arm64-fix-lint-DEFINE_TYPES-and-magic-numbers-i.patch                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0008-qemu-arm64-fix-lint-extract-smp_cache-init-and-remov.patch                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0009-qemu-arm64-remove-CacheBindProcessHierarchy-enum-and.patch                         
    git am --reject ~/patch目录/[cacheinfo-configuration]0010-qemu-arm64-add-cache_supported-for-all-cache-levels-.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0011-qemu-arm64-fix-CLIDR-injection-write-to-cpreg_values.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0012-qemu-arm64-move-CLIDR-injection-after-kvm_arm_writab.patch
   ```
3. 在libvirt源码根目录执行以下命令，按邮件补丁流合入VF ITS补丁：
   ```bash
    git am --reject ~/patch目录/[cacheinfo-configuration]0001-libvirt-Support-specifying-the-cache-size-presented-.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0002-fix-some-enumeration-value-not-handled-in-switch.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0003-libvirt-add-cacheinfo-parmater-details-support.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0004-libvirt-add-cacheinfo-parmater-UT.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0005-libvirt-rename-cacheinfo-name-to-cache-extract-macro.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0006-libvirt-fix-missing-rename-and-enum-cases.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0007-libvirt-eliminate-void-parameter-in-cpuTestCacheInfo.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0008-libvirt-change-QEMU-smp-cache-command-line-parameter.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0009-libvirt-rename-cacheinfo-location-to-topology-add-mo.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0010-libvirt-fix-Wshadow-error-by-renaming-local-variable.patch
    git am --reject ~/patch目录/[cacheinfo-configuration]0011-libvirt-fix-symbol-sorting-in-libvirt_private.syms-m.patch
   ```

### 编译qemu和libvirt

1. 编译qemu。
    ```bash
    mkdir -p build && cd build
    ../configure --disable-docs --target-list=aarch64-softmmu --disable-werror
    make -j 100
    ```
2. 编译libvirt。
    ```bash
    meson setup build --prefix=/usr -Ddocs=disabled
    ninja -C build -j 100
    ninja -C build install
    ```
   
## 软件使用

通过libvirt XML配置缓存信息。

1. 编辑虚拟机配置文件。
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
    
    | 参数 | 说明                                                                                    |
    |--|---------------------------------------------------------------------------------------|
    | cache | 缓存层级和类型，格式为lx[i/d]，x表示缓存层级，[i/d]表示指令缓存或数据缓存，不写表示统一缓存                                  |
    | topology | 缓存绑定的处理器层级，可选thread、core、cluster、die、socket；同一层级上包含多个不同level的缓存时，表示它们之间存在next level关系 |
    | size | 缓存大小（字节）                                                                              |
    | sets | 缓存组数                                                                                  |
    | associativity | 缓存关联度（ways数）                                                                          |
    | line | 缓存行大小（字节）                                                                             |
    
    注意事项：
    - 输入的参数不完整时，部分信息按照鲲鹏950的物理缓存提供默认参数。
    - cache参数为必选，其余参数为可选。
    - die参数由于qemu不支持，目前die参数恒定设置为1，不可更改。
    - 当cache设置为l1i和l1d时，不能将cache设置为l1；当cache设置为l1时，不能将cache设置为l1i和l1d。
    - l1i、l1d、l1、l2和l3的topology参数和size参数推荐与服务器硬件参数一致。

2. 启动虚拟机。

    ```bash
    virsh start <vm name> --console
    ```

3. 验证配置是否生效，若打印信息中出现Caches一栏，则表示配置成功。

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