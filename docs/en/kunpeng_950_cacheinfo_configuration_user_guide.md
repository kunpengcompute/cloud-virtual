# VM Cache Information Configuration User Guide

## Function Description

After a VM user sets the VM cache information, this function injects detailed cache information into the VM so that the VM OS kernel can correctly identify the CPU cluster (CCL)-level scheduling domain, improving the kernel scheduling efficiency.

## Operating Environment

Before enabling this feature, ensure that the hardware and software environments meet the requirements.

**Hardware Requirements**

| Item| Description|
|--|--|
| Processor| New Kunpeng 920 processor model or Kunpeng 950 processor|

**Software Requirements**

| Item| Version or Description|
|--|--|
| OS | openEuler 24.03 SP3|
| QEMU| 8.2.0 |
| libvirt | 9.1.0 |

## Software Compilation

### Obtaining the Patches

1. To obtain the cacheinfo patches, visit:

    | Item| Patch Link|
    |--|--|
    | QEMU| [Link](https://gitcode.com/boostkit/cloud-virtual/tree/master/qemu/qemu-8.2.0)|
    | libvirt | [Link](https://gitcode.com/boostkit/cloud-virtual/tree/master/libvirt/libvirt-9.10.0)|

2. Obtain the following 11 patch files for libvirt and ensure that their application sequence remains unchanged.

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

3. Obtain the following 12 patch files for QEMU and ensure that their application sequence remains unchanged.

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

### Obtaining the QEMU and libvirt Source Code

1. Switch to the `qemu-8.2.0` branch and clone the QEMU source code.

    ```bash
    git clone https://gitcode.com/openeuler/qemu.git -b qemu-8.2.0
    ```

2. Switch to the `libvirt-9.10.0` branch and clone the libvirt source code.

    ```bash
    git clone https://gitcode.com/openeuler/libvirt.git -b libvirt-9.10.0
    ```

### Applying the QEMU and libvirt Patches

1. Before applying the patches, you are advised to check whether the source code directory is clean. If no command output is displayed, there are no uncommitted changes in the current workspace.

   ```bash
   git status --short
   ```

2. Run the following commands in the root directory of the QEMU source code to apply the feature patches:

   ```bash
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0001-qapi-qom-Define-cache-enumeration-and-properties-for.patch                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0002-qemu-Support-specifying-the-cache-size-presented-to-.patch                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0003-qemu-arm64-resolve-code-conflicts.patch                                           
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0004-qemu-arm64-add-ut-for-smp-cache-info.patch                                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0005-qemu-arm64-fix-ut-for-smp-cache-info.patch                                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0006-qemu-arm64-add-SMP-cache-topology-and-PPTT-support.patch                          
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0007-qemu-arm64-fix-lint-DEFINE_TYPES-and-magic-numbers-i.patch                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0008-qemu-arm64-fix-lint-extract-smp_cache-init-and-remov.patch                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0009-qemu-arm64-remove-CacheBindProcessHierarchy-enum-and.patch                        
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0010-qemu-arm64-add-cache_supported-for-all-cache-levels-.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0011-qemu-arm64-fix-CLIDR-injection-write-to-cpreg_values.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0012-qemu-arm64-move-CLIDR-injection-after-kvm_arm_writab.patch
   ```

3. Run the following commands in the root directory of the libvirt source code to apply the feature patches:

   ```bash
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0001-libvirt-Support-specifying-the-cache-size-presented-.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0002-fix-some-enumeration-value-not-handled-in-switch.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0003-libvirt-add-cacheinfo-parmater-details-support.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0004-libvirt-add-cacheinfo-parmater-UT.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0005-libvirt-rename-cacheinfo-name-to-cache-extract-macro.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0006-libvirt-fix-missing-rename-and-enum-cases.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0007-libvirt-eliminate-void-parameter-in-cpuTestCacheInfo.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0008-libvirt-change-QEMU-smp-cache-command-line-parameter.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0009-libvirt-rename-cacheinfo-location-to-topology-add-mo.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0010-libvirt-fix-Wshadow-error-by-renaming-local-variable.patch
    git am --reject ~/<Patch_directory>/[cacheinfo-configuration]0011-libvirt-fix-symbol-sorting-in-libvirt_private.syms-m.patch
   ```

### Compiling QEMU and libvirt

1. Compile QEMU.

    ```bash
    mkdir -p build && cd build
    ../configure --disable-docs --target-list=aarch64-softmmu --disable-werror
    make -j 100
    ```

2. Compile libvirt.

    ```bash
    meson setup build --prefix=/usr -Ddocs=disabled
    ninja -C build -j 100
    ninja -C build install
    ```
   
## Software Usage

Configure cache information using the libvirt XML file.

1. Edit the VM configuration file.

    ```bash
    virsh edit <vm name>
    ```

    Configure the cache information for high-specifications VMs using new Kunpeng 920 processor models by referring to the following example. (The parameters are for reference only. Set the parameters based on actual conditions.)

    ```xml
        ...
        <devices>
          <emulator>"Path to the compiled qemu-system-aarch64 file"</emulator>
        </devices>
        <cpu mode='host-passthrough' migratable='off'>
          <topology sockets='2' dies='1' clusters='10' cores='4' threads='2'/>
          <cacheinfo cache='l1i' topology='core' size='65536' sets='256' associativity='4' line='64'/>
          <cacheinfo cache='l1d' topology='core' size='65536' sets='256' associativity='4' line='64'/>
          <cacheinfo cache='l2' topology='core' size='1310720' sets='2048' associativity='10' line='64'/>
          <cacheinfo cache='l3' topology='socket' size='73400320' sets='2048' associativity='28' line='128'/>
        </cpu>
        ...
    ```

    Configure the cache information for high-specifications VMs using Kunpeng 950 processors by referring to the following example. (The parameters are for reference only. Set the parameters based on actual conditions.)

    ```xml
        ...
        <devices>
          <emulator>"Path to the compiled qemu-system-aarch64 file"</emulator>
        </devices>
        <cpu mode='host-passthrough' migratable='off'>
          <topology sockets='2' dies='1' clusters='6' cores='8' threads='2'/>
          <cacheinfo cache='l1i' topology='core' size='131072' sets='512' associativity='4' line='64'/>
          <cacheinfo cache='l1d' topology='core' size='65536' sets='256' associativity='4' line='64'/>
          <cacheinfo cache='l2' topology='core' size='1048576' sets='2048' associativity='8' line='64'/>
          <cacheinfo cache='l3' topology='cluster' size='23855104' sets='16384' associativity='19' line='64'/>
        </cpu>
        ...
    ```

    **Table 1** Parameter description
    
    | Parameter| Description                                                                                   | Value Range|
    |--|---------------------------------------------------------------------------------------|--------|
    | cache | Cache level and type. The format is <code>lx[i/d]</code>, where <code>x</code> indicates the cache level, and <code>[i/d]</code> indicates the instruction cache or data cache. If <code>[i/d]</code> is not specified, the unified cache is used.                                 | <code>l1i</code>, <code>l1d</code>, <code>l1</code>, <code>l2</code>, <code>l3</code>|
    | topology | Processor level bound to the cache. If caches of different levels are included at a processor level, these caches have inclusion relationships.| <code>thread</code>, <code>core</code>, <code>cluster</code>, <code>socket</code>|
    | size | Cache size (in bytes).                                                                             | 1 to 4294967295 (4-byte unsigned integer)|
    | sets | Number of cache sets.                                                                                 | 1 to 4294967295 (4-byte unsigned integer)|
    | associativity | Cache associativity (number of ways).                                                                         | 1 to 255 (1-byte unsigned integer)|
    | line | Cache line size (in bytes).                                                                            | 1 to 65535 (2-byte unsigned integer)|
    
    Precautions:
    - In the `<cacheinfo>` tag, the `cache` parameter is mandatory while other parameters are optional. If the parameters are incomplete, the default physical cache values of Kunpeng 950 are automatically used.
    - In the `<cacheinfo>` tag, the `topology` parameter does not support `die` (as QEMU does not support the die dimension). If it is set to `die`, the configuration does not take effect. It is recommended that the `topology` and `size` parameters be the same as the actual hardware parameters of the server.
    - L1 cache can be configured as `l1i` and `l1d` separately, or configured as `l1`. However, `l1` is mutually exclusive with `l1i` and `l1d`. L2 and L3 are unified caches and cannot be split.

2. Start the VM.

    ```bash
    virsh start <vm name> --console
    ```

3. Check whether the configuration takes effect.

    Log in to the VM and run the `lscpu` command to view the cache overview. Ensure that the parameters displayed in the `Caches` area are the same as those configured in the XML file.

    ```bash
    lscpu
    ```

    To further verify the details of each parameter, you can use sysfs to view the parameters one by one. The queried values should be the same as those configured in `<cacheinfo>` of the XML file. The following uses `cpu0` as an example. You can check any vCPU.

    ```bash
    # Viewing the overview of all cache levels
    ls /sys/devices/system/cpu/cpu0/cache/
    ```

    The expected output is similar to the following:

    ```text
    index0  index1  index2  index3
    ```

    Verify each cache parameter. The following uses `index0` (L1 data cache) as an example.

    ```bash
    # Verify the cache level and type (L1 data cache: level=1, type=Data).
    cat /sys/devices/system/cpu/cpu0/cache/index0/level
    cat /sys/devices/system/cpu/cpu0/cache/index0/type

    # Verify the cache size (corresponding to the size parameter in <cacheinfo>). Pay attention to the unit.
    cat /sys/devices/system/cpu/cpu0/cache/index0/size

    # Verify the number of cache sets (corresponding to the sets parameter).
    cat /sys/devices/system/cpu/cpu0/cache/index0/number_of_sets

    # Verify the cache associativity (corresponding to the associativity parameter).
    cat /sys/devices/system/cpu/cpu0/cache/index0/ways_of_associativity

    # Verify the cache line size (corresponding to the line parameter).
    cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size

    # Verify the topology binding level (shared_cpu_list displays the list of CPUs that share the cache).
    # When topology=core, threads in the same core share the cache. When topology=cluster, cores in the same cluster share the cache.
    cat /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list
    ```

    According to the example configuration, the expected verification results of each index of `cpu0` on the new Kunpeng 920 processor model and Kunpeng 950 processor are as follows: The mapping between `shared_cpu_list` and `topology` is as follows: When `topology` is set to `core`, `shared_cpu_list` indicates the threads in the same core. When `topology` is set to `cluster`, `shared_cpu_list` indicates all CPUs in the same cluster. When `topology` is set to `socket`, `shared_cpu_list` indicates all CPUs in the same socket.

    New Kunpeng 920 processor model:

    | index | level | type | size | number_of_sets | ways_of_associativity | coherency_line_size | shared_cpu_list |
    |--|--|--|--|--|--|--|--|
    | index0 | 1 | Data | 64K | 256 | 4 | 64 | 0-1 |
    | index1 | 1 | Instruction | 64K | 256 | 4 | 64 | 0-1 |
    | index2 | 2 | Unified | 1280K | 2048 | 10 | 64 | 0-1 |
    | index3 | 3 | Unified | 71680K | 2048 | 28 | 128 | 0-79 |

    Kunpeng 950 processor:

    | index | level | type | size | number_of_sets | ways_of_associativity | coherency_line_size | shared_cpu_list |
    |--|--|--|--|--|--|--|--|
    | index0 | 1 | Data | 64K | 256 | 4 | 64 | 0-1 |
    | index1 | 1 | Instruction | 128K | 512 | 4 | 64 | 0-1 |
    | index2 | 2 | Unified | 1024K | 2048 | 8 | 64 | 0-1 |
    | index3 | 3 | Unified | 23296K | 16384 | 19 | 64 | 0-15 |

    NOTE: The mapping between indexes and caches is subject to the actual enumeration sequence of the VM, which varies depending on the platform.

## Precautions

* The cache information configuration function is supported only in KVM virtualization scenarios with QEMU being used as the virtualization platform.
* Cache information is injected into VMs through Advanced Configuration and Power Interface (ACPI). Theoretically, ACPI platform virtualization is supported. However, this function is currently valid only for the Arm architecture.
* Cache information cannot be injected into VMs on hardware platforms using asymmetric processors.
* The related scheduling domain functions must be enabled in the kernel configuration of the guest OS.
* If an upper-level cache exists, its lower-level cache must exist. Otherwise, an error will be reported and the VM will stop during startup.
* If the used QEMU does not support the cache information parameters, the default error reporting mechanism will be triggered and the VM startup will be stopped.
