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
1. cacheinfo补丁获取链接如下。

    | 项目 | 补丁链接 |
    |--|--|
    | qemu | [https://gitcode.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0](https://gitcode.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0) |
    | libvirt | [https://gitcode.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0](https://gitcode.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0) |

2. libvirt需要获取以下11个补丁文件，并保持应用顺序不变。

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

3. qemu需要获取以下12个补丁文件，并保持应用顺序不变。

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
2. 在qemu源码根目录执行以下命令，按邮件补丁流合入VF ITS补丁。
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
3. 在libvirt源码根目录执行以下命令，按邮件补丁流合入VF ITS补丁。
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
    参考如下示例配置虚拟机缓存信息（示例参数仅供参考，请以实际场景为准）。
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

    参数说明。
    
    | 参数 | 说明                                                                                    | 取值范围 |
    |--|---------------------------------------------------------------------------------------|--------|
    | cache | 缓存层级和类型，格式为lx[i/d]，x表示缓存层级，[i/d]表示指令缓存或数据缓存，不写表示统一缓存                                  | l1i、l1d、l1、l2、l3 |
    | topology | 缓存绑定的处理器层级，同一层级上包含多个不同level的缓存时，表示它们之间存在next level关系 | thread、core、cluster、socket |
    | size | 缓存大小（字节）                                                                              | 1 ~ 4294967295（4字节无符号整数） |
    | sets | 缓存组数                                                                                  | 1 ~ 4294967295（4字节无符号整数） |
    | associativity | 缓存关联度（ways数）                                                                          | 1 ~ 255（1字节无符号整数） |
    | line | 缓存行大小（字节）                                                                             | 1 ~ 65535（2字节无符号整数） |
    
    注意事项。
    - `<cacheinfo>`标签下的cache参数为必选，其余参数为可选。参数不完整时，缺失部分自动使用鲲鹏950的物理缓存默认值。
    - `<cacheinfo>`标签下的topology参数不支持die（QEMU不支持die维度），写die时该条配置不生效。topology和size参数建议与服务器实际硬件参数一致。
    - L1支持分别配置l1i和l1d，也可统一配置为l1，但l1与l1i/l1d互斥；L2和L3为统一缓存，不支持拆分。

2. 启动虚拟机。

    ```bash
    virsh start <vm name> --console
    ```

3. 验证配置是否生效。

    进入虚拟机，先通过lscpu查看缓存概览，确认Caches一栏中显示的参数与XML配置一致。

    ```bash
    lscpu
    ```

    如需进一步验证各参数细节，可通过sysfs逐项查看，查询到的值应与XML中`<cacheinfo>`配置的参数一致。以下以cpu0为例，实际可检查任意vCPU。

    ```bash
    # 查看所有缓存层级概览
    ls /sys/devices/system/cpu/cpu0/cache/
    ```

    期望输出类似。

    ```text
    index0  index1  index2  index3
    ```

    逐项验证各缓存参数，以index0（L1数据缓存）为例。

    ```bash
    # 验证缓存层级和类型（L1数据缓存应为 level=1, type=Data）
    cat /sys/devices/system/cpu/cpu0/cache/index0/level
    cat /sys/devices/system/cpu/cpu0/cache/index0/type

    # 验证缓存大小（对应<cacheinfo>的size参数，单位不同需注意）
    cat /sys/devices/system/cpu/cpu0/cache/index0/size

    # 验证缓存组数（对应sets参数）
    cat /sys/devices/system/cpu/cpu0/cache/index0/number_of_sets

    # 验证缓存关联度（对应associativity参数）
    cat /sys/devices/system/cpu/cpu0/cache/index0/ways_of_associativity

    # 验证缓存行大小（对应line参数）
    cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size

    # 验证topology绑定层级（shared_cpu_list显示共享该缓存的CPU列表）
    # topology=core时，同一core下的线程共享；topology=cluster时，同一cluster下的核共享
    cat /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list
    ```

    根据示例配置，各index的期望验证结果如下。

    | index | level | type | size | number_of_sets | ways_of_associativity | coherency_line_size | topology |
    |--|--|--|--|--|--|--|--|
    | index0 | 1 | Data | 64K | 512 | 4 | 64 | core |
    | index1 | 1 | Instruction | 128K | 512 | 4 | 64 | core |
    | index2 | 2 | Unified | 1024K | 2048 | 8 | 64 | core |
    | index3 | 3 | Unified | 23296K | 16384 | 19 | 64 | cluster |

    注意：index与缓存的对应关系以虚拟机实际枚举顺序为准，不同平台可能不同。

## 注意事项

* 缓存信息配置功能仅支持KVM虚拟化，且使用qemu作为虚拟化平台。
* 缓存信息通过ACPI注入虚拟机，理论上可支持ACPI平台虚拟化，但当前仅对ARM架构生效。
* 不支持非对称处理器的硬件平台注入缓存信息。
* 功能需要向vcpu注入CLIDR寄存器，需确保内核包含特定版本补丁。
* guest OS的内核配置需打开相关调度域功能开关。
* 下一级缓存存在时，上一级缓存必须存在，否则虚拟机启动时会报错并停止。
* 若使用的qemu不支持缓存信息参数，将触发默认报错机制并停止虚拟机启动。