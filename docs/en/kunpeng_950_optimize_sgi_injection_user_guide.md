# SGI Injection Affinity Optimization User Guide

## Introduction

### Overview

This document describes how to deploy and use the software-generated interrupt (SGI) injection affinity optimization feature on Kunpeng 950 servers.

In AArch64 KVM virtualization, a VM sends a software generated interrupt (SGI) to the target vCPU by writing the `ICC_SGI1R_EL1` register. When the guest OS sends an SGI, the KVM needs to calculate the target multiprocessor affinity register (MPIDR) on the host machine based on the affinity information, and then traverse all vCPUs one by one to match MPIDR. The time complexity is O(n). When there are a large number of vCPUs, full traversal is required for each SGI delivery, which becomes a performance bottleneck.

The SGI injection affinity optimization feature pre-calculates a compressed mapping table (`kvm_mpidr_data`) from MPIDR values to vCPU indexes when a VM runs for the first time. This feature transforms SGI distribution path traversal (O(n)) to direct search (O(1)), significantly reducing the SGI delivery latency.

### Function Architecture

The SGI injection affinity optimization feature is implemented by the MPIDR mapping initialization module, fast search module, and SGI distribution path optimization module.

| Module| Function|
|--|--|
| MPIDR mapping initialization (<code>kvm_init_mpidr_data</code>)| When a VM runs for the first time, analyzes the MPIDR affinity of all vCPUs, extracts their differential bits to construct a compressed mapping table, and saves the table in the `kvm->arch.mpidr_data`.|
| Fast search (<code>kvm_mpidr_index</code>/<code>kvm_mpidr_to_vcpu</code>)| Directly converts the MPIDR affinity into vCPU indexes using the compressed mapping table, without the need for traversal.|
| SGI distribution path optimization (<code>vgic_v3_dispatch_sgi</code>)| Rewrites the SGI distribution logic by changing the original full traversal to direct search by the target bitmap, and extracts the `vgic_v3_queue_sgi` auxiliary function.|

The typical process is as follows:

1. When a VM runs for the first time, `kvm_init_mpidr_data` analyzes the MPIDR value corresponding to each vCPU and constructs a compressed mapping table.
2. The guest OS writes the `ICC_SGI1R_EL1` register to trigger SGI distribution.
3. The KVM calculates the target MPIDR value based on the affinity information and directly searches for the corresponding vCPU in the mapping table.
4. The `vgic_v3_queue_sgi` is called to deliver an interrupt to the target vCPU.

## Environment Requirements

Before enabling this feature, ensure that the hardware and software environments meet the requirements.

**Hardware Requirements**

| Item| Description|
|--|--|
| Processor| Kunpeng 950|

**Software Requirements**

| Item| Version or Description                   |
|--|--------------------------|
| OS | openEuler 24.03 LTS SP3|
| Kernel source code baseline| OLK-6.6 6.6.0-135.0.0 |

## Obtaining and Applying the SGI Optimization Patch

### Obtaining the Patch

1. To obtain the SGI optimization patch, visit:

   ```text
   https://gitcode.com/boostkit/cloud-virtual/tree/master/kernel/kernel-6.6.0
   ```

2. Obtain the following patch:

   `[kvm]arm64:vgic-v3:Optimize_affinity-based_SGI_injection.patch`

### Obtaining the Target Kernel Source Code

1. Clone the openEuler kernel source code and switch to the `OLK-6.6` branch. Ensure that the source code contains the tag `6.6.0-135.0.0`.

   ```bash
   git clone https://gitcode.com/openeuler/kernel.git -b OLK-6.6 --depth=1
   cd kernel
   git branch --show-current
   ```

2. The expected output is as follows:

   ```text
   OLK-6.6
   ```

### Applying the Patch

1. Before applying the patch, ensure that the source code directory is clean.

   ```bash
   git status --short
   ```

    If no command output is displayed, there are no uncommitted changes in the current workspace.

2. Run the following commands in the root directory of the kernel source code to apply the SGI optimization patch:

   ```bash
   git apply -p1 ~/sgi-patch/[kvm]arm64:vgic-v3:Optimize_affinity-based_SGI_injection.patch
   git log --oneline -n 1
   ```

   The expected command output is as follows:

   ```text
   <newest> KVM: arm64: vgic-v3: Optimize affinity-based SGI injection
   ```

## Compiling and Installing the Kernel

### Preparing for the Compilation

Before the compilation, ensure that the openEuler kernel RPM build dependencies have been installed and the build directory has sufficient space to store the source code, intermediate files, and RPM package. You can check the current source code status again.

```bash
git status --short
```

### Compiling the .config File

Use the default openEuler kernel configuration to generate the .config file required for compilation.

```bash
make openeuler_defconfig
```

### Compiling the RPM Package

Run the following command in the root directory of the kernel source code. The build time depends on the server configuration, kernel configuration, and number of concurrent threads. After the build is complete, the RPM package is usually generated in the parent directory of the source code directory.

```bash
make binrpm-pkg -j$(nproc)
```

### Installing the RPM Package

1. Install the new kernel package according to the actual name of the generated RPM file. The following command is only an example. Before running the command, ensure that the kernel RPM package is the current build product.

   ```bash
   sudo rpm -ivh <Kernel_RPM_name> --force
   ```

2. After the installation is complete, check the installed kernel.

   ```bash
   rpm -qa | grep '^kernel' | sort
   ```

3. Restart the system for the new kernel to take effect.

   ```bash
   sudo reboot
   ```

4. After the system is restarted, confirm that the kernel version in use has been switched to the newly installed version.

   ```bash
   uname -r
   ```

## Using the SGI Injection Affinity Optimization Feature

This feature automatically takes effect at the kernel level, and no additional configuration is required. After the kernel that contains this feature is installed and the OS is restarted, the optimized fast search logic is automatically used for the SGI distribution path of the KVM VM.

### Verifying Whether the Feature Takes Effect

1. Ensure that the current kernel version has been updated.

   ```bash
   uname -r
   ```

2. Start the VM.

   ```bash
   virsh start <vm name> --console
   ```

3. Trigger an SGI in the VM (for example, through `ipistat` or kernel scheduling activities) and check whether the SGI delivery latency is reduced.

4. You can use Ftrace or perf on the host machine to check whether the execution time of the `vgic_v3_dispatch_sgi` function is significantly shortened.

## Precautions

* This feature affects only the SGI distribution path on the KVM host machine, and is transparent to the guest OS.
* When the GICv4.1 hardware inter-processor interrupt virtualization (IPIv) is enabled, the guest OS SGIs are directly processed by hardware without passing through the KVM software path. In this case, this optimization does not take effect.
