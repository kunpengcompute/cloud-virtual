# VF ITS Selection User Guide

## Introduction

### Overview

This document describes how to deploy and use the VF ITS selection feature on Kunpeng 950 servers.

On a server platform that supports multiple interrupt translation services (ITSs), virtual functions (VFs) created under a physical function (PF) of a physical peripheral component interconnect (PCI) device inherit the I/O Remapping Table (IORT)/PCI device interrupt domain parsing path by default. This default path forces VFs to select the same ITS as the PF. When the ITS used by the PF is overloaded, the VF performance is affected.

The VF ITS selection feature provides a PF-level sysfs interface, which allows users to configure a group of ITSs that can be used by VFs for a PF (by specifying the ITS raw indexes). Before accepting the configuration, the kernel verifies the configuration based on the ITS currently used by the PF, platform topology rules, and the chip capability trustlist. The configuration is saved and takes effect during subsequent VF enumeration only when all conditions are met.

It should be noted that this feature provides the VF ITS selection capability restricted by the platform, instead of the universal ITS remapping capability. The configuration items must match the current-platform IORT ITS registration sequence, the current ITS used by the PF, and the chip model.

### Function Architecture

This feature is implemented by the PCI I/O virtualization (IOV) layer, Advanced Configuration and Power Interface (ACPI) IORT policy layer, generic interrupt controller (GIC) ITS driver, and PCI probe path.

| Module| Function|
|--|--|
| PCI IOV layer| Provides the `sriov_vf_its_indices` sysfs node on the PF to parse and save the VF ITS raw index configuration.|
| ACPI IORT policy layer| Parses the current ITS used by the PF, verifies whether the candidate raw index meets the platform topology and chip capability constraints, and parses out the target MSI domain during VF running.|
| GIC ITS driver| Provides the ITS chip identity reading capability to determine whether the current server chip is supported.|
| PCI probe path| Sets the MSI domain during VF enumeration, and avoids the configuration error being masked by the default MSI domain fallback of the bridge when the VF ITS has been explicitly configured for the PF.|

The typical process is as follows:

1. A user writes `sriov_vf_its_indices` to the PF path.
2. The PCI IOV layer parses the raw index array.
3. The IORT policy layer performs verification using the ITS currently used by the PF as the anchor.
4. After the verification is passed, the PF saves the raw index array.
5. After VFs are enabled, each VF selects a raw index from the array saved by the PF based on its `vf_id`.
6. The IORT parses the target PCI MSI domain based on the raw index and sends it to the VF.

### External Interfaces

The VF ITS selection feature uses the sysfs node in the PF path.

```text
/sys/bus/pci/devices/<PF_BDF>/sriov_vf_its_indices
/sys/bus/pci/devices/<PF_BDF>/sriov_numvfs
```

In the preceding command:

- `sriov_vf_its_indices` is a new interface for this feature, which is written with the ITS raw index array that can be used by VFs. The raw index value ranges from 0 to 7.
- `sriov_numvfs` specifies the number of VFs to be created by a PF. If the value is `0`, the VFs under the PF are disabled. This interface has been available before the VF ITS selection feature is developed. The value cannot exceed the maximum number of VFs set by the PF driver.

The value written to `sriov_vf_its_indices` is the raw index in the IORT ITS list, instead of the ITS base address or `translation_id`.

## Environment Requirements

Before enabling this feature, ensure that the hardware and software environments meet the requirements.

**Hardware Requirements**

| Item| Description|
|--|--|
| Processor| Kunpeng 950|
| PCIe device| NVMe drive or PCIe NIC|

**Software Requirements**

| Item| Version or Description|
|--|--|
| OS | openEuler 24.03 SP3|
| Kernel source code baseline| OLK-6.6 6.6.0-133.0.0 |
| libvirt | 9.1.0 (Yum repository) |
| QEMU| 8.2.0 (Yum repository) |
| BIOS| 10.79 |

## Obtaining and Applying Related Patches

### Obtaining the Patches

To obtain the related patches, visit:

```text
https://gitcode.com/boostkit/cloud-virtual/tree/master/kernel/kernel-6.6.0/VF_ITS
```

You need to obtain the following two patch files and ensure that their application sequence remains unchanged:

1. `0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch`
2. `0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch`

### Obtaining the Target Kernel Source Code

Clone the openEuler kernel source code and switch to the `OLK-6.6` branch. Ensure that the source code contains the tag `6.6.0-133.0.0`.

```bash
git clone https://gitcode.com/openeuler/kernel.git -b OLK-6.6 --depth=1
cd kernel
git branch --show-current
```

The expected output is as follows:

```text
OLK-6.6
```

### Applying the Patches

1. Before applying the patches, ensure that the source code directory is clean.

   ```bash
   git status --short
   ```

    If no command output is displayed, there are no uncommitted changes in the current workspace.

2. In the root directory of the kernel source code, run the following commands to apply the VF ITS patches based on the email patch format:

   ```bash
   git am --reject ~/vf-its-patch/0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch
   git am --reject ~/vf-its-patch/0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch
   git log --oneline -n 2
   ```

   The expected command output is as follows:

   ```text
   <newest> PCI: Fix kabi broken for SR-IOV exported symbols
   <older>  PCI/ACPI: Support ITS selection for PCI VF devices
   ```

## Compiling and Installing the Kernel

### Preparing for the Compilation

Before starting the compilation, ensure that the dependencies for building the openEuler kernel RPM package have been installed and the build directory has sufficient space to store the source code, intermediate files, and RPM package.

You can check the current source code status again.

```bash
git status --short
```

### Compiling the RPM Package

Run the following command in the root directory of the kernel source code:

```bash
make binrpm-pkg -j$(nproc)
```

The build time depends on the server configuration, kernel configuration, and number of concurrent threads. After the build is complete, the RPM package is usually generated in the parent directory of the source code directory.

### Installing the RPM Package

Install the new kernel package according to the actual name of the generated RPM file. The following command is only an example. Before running the command, ensure that the kernel RPM package is the current build product.

```bash
sudo rpm -ivh <Kernel_RPM_name> --force
```

After the installation is complete, restart the system for the new kernel to take effect.

```bash
sudo reboot
```

After the system is restarted, confirm that the kernel version in use has been switched to the newly installed version.

```bash
uname -r
rpm -qa | grep '^kernel' | sort
```

## Using the VF ITS Selection Feature

### Checking the PF Path

First, confirm the Bus, Device, and Function (BDF) of the target PF. The following uses `0000:03:00.0` as an example. Replace it with the BDF of the target device in actual operations.

```bash
export PF_BDF=0000:03:00.0
cd /sys/bus/pci/devices/${PF_BDF}
pwd
```

The expected path is similar to the following:

```text
/sys/bus/pci/devices/0000:03:00.0
```

Ensure that the PF supports SR-IOV and related VF ITS nodes are exposed.

```bash
ls sriov_totalvfs sriov_numvfs sriov_vf_its_indices
cat sriov_totalvfs
cat sriov_numvfs
```

Notes:

- `sriov_vf_its_indices` is displayed only in the PF path, not in the VF path.
- Before modifying `sriov_vf_its_indices`, ensure that `sriov_numvfs` is set to `0`.

### Disabling VFs Before Configuration

If VFs have been enabled for a PF, disable all VFs for the PF.

```bash
echo 0 | sudo tee sriov_numvfs
cat sriov_numvfs
```

The expected output is as follows:

```text
0
```

Currently, `sriov_vf_its_indices` can be modified only when `sriov_numvfs` is `0`. If VFs are not disabled, the write operation fails.

### Clearing the Old Configuration

To clear the VF ITS raw index configuration saved on the PF, use either of the following methods:

```bash
echo -1 | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

Or

```bash
echo | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

After the clearing:

- `cat sriov_vf_its_indices` usually outputs only one blank line.
- The default IORT/PCI parsing path is restored for VFs and the VFs are no longer controlled by the explicit ITS configuration for the PF.

### Writing the ITS Raw Index Array

When writing `sriov_vf_its_indices`, use the raw index in the IORT ITS list of the current platform, rather than the ITS base address or `translation_id`.

The following example configures VFs under a PF to be allocated cyclically with raw indexes `6` and `7`.

```bash
echo 6,7 | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

The expected output is as follows:

```text
6,7
```

The mapping rule from VFs to ITS raw indexes is as follows:

```text
raw_index = pf->vf_its_indices[vf_id % nr_indices]
```

When `sriov_vf_its_indices` is `6,7`, the mapping is as follows:

| VF | Raw Index|
|--|--|
| VF 0| 6 |
| VF 1| 7 |
| VF 2| 6 |
| VF 3| 7 |

A successful write only indicates that the raw index array has passed the verification based on the current ITS used by the PF, platform topology, and chip trustlist. The valid raw indexes may vary depending on the PF or platform.

### Enabling VFs

After writing raw indexes into the array, enable VFs. The following uses the creation of four VFs as an example.

```bash
export VF_COUNT=4
echo ${VF_COUNT} | sudo tee sriov_numvfs
cat sriov_numvfs
```

The expected output is as follows:

```text
4
```

View the VF symbolic links generated under the PF.

```bash
ls -l virtfn*
```

You can also use `lspci` to view the PF and VFs.

```bash
lspci -nn | grep -E "${PF_BDF#0000:}|Virtual Function"
```

**Precautions**

For details about how to create NVMe drive VFs, see the VF creation guide of the NVMe drive driver.

### Viewing Run Logs

After VFs are enabled, you can use `dmesg` to view the VF ITS parsing logs.

```bash
dmesg | grep -E 'VF ITS|VF uses ITS raw index|sriov_vf_its_indices|ITS raw index'
```

When a VF successfully parses the target ITS, the common log output format is similar to the following:

```text
VF uses ITS raw index <n> base <base_addr>
```

## Raw Index Mapping Table of the Current Platform

The following table lists the mapping between raw indexes and ITS base addresses on the current platform.

```text
socket 1 ITS A: index0: 0x7010000000
socket 1 ITS B: index1: 0x6888000000
socket 1 ITS C: index2: 0x4588000000
socket 1 ITS D: index3: 0x4488000000
socket 0 ITS A: index4: 0x3010000000
socket 0 ITS B: index5: 0x2888000000
socket 0 ITS C: index6: 0x0588000000
socket 0 ITS D: index7: 0x0488000000
```

This table is applicable only to the current platform and the current IORT ITS registration sequence. It should not be used as a cross-platform stable application binary interface (ABI).

Based on the current platform rules:

- If the PF is in the slot of ITS A corresponding to `index0` or `index4`, VFs can only select ITS A of the same socket.
- If the PF is in the slot of ITS B, ITS C, or ITS D, VFs can only select ITS B, ITS C, or ITS D of the same socket.
- Cross-socket ITS selection is not allowed.
- If the PF or candidate ITS is not in the static table of the platform, the current policy will be conservatively degraded to allow only the ITS corresponding to the candidate raw index to be the same as the current ITS of the PF.

## Verification and Troubleshooting

### Minimum Verification Procedure

You are advised to check whether the VF ITS configuration takes effect in the following sequence:

1. Check whether the PF node exists.

      ```bash
      ls /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
      ```

2. Check whether the current configuration has been written.

      ```bash
      cat /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
      cat /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
      ```

3. Check whether VFs have been generated.

      ```bash
      ls -l /sys/bus/pci/devices/${PF_BDF}/virtfn*
      ```

4. Check run logs.

      ```bash
      dmesg | grep -E 'VF uses ITS raw index|failed to resolve VF ITS raw index|VF ITS selection'
      ```

### Failed to Write Configuration When `sriov_numvfs != 0`

**Symptom**

A failure message is returned when data is written to `sriov_vf_its_indices`.

**Causes**

The current implementation requires `sriov_numvfs == 0` when `sriov_vf_its_indices` is modified.

**Solution**

Disable VFs under the PF and then write the configuration again.

```bash
echo 0 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
echo 6,7 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
```

### Invalid Raw Index

**Symptom**

After a raw index that does not exist is written to `sriov_vf_its_indices`, the sysfs fails to be written.

The common log output is as follows:

```text
requested VF ITS raw index <n> is invalid
```

**Causes**

The written raw index is out of the current IORT ITS list, or the ITS corresponding to the raw index cannot be parsed into a valid PCI MSI domain.

**Solution**

Select a valid raw index according to the raw index table of the current platform, and ensure that the target ITS has been registered on the current platform and can be parsed.

### Cross-Socket Candidate ITS Selection or Violation of the A/B/C/D Rules

**Symptom**

The raw index fails to be written into the array.

The common log output is as follows:

```text
VF ITS raw index <n> base <addr> is not allowed for PF ITS base <addr>
```

**Causes**

The target ITS and the current ITS of the PF are not in the same socket, or the candidate ITS violates the current platform's A/B/C/D slot rules.

**Solution**

Determine the current ITS used by the PF, and then select the candidate raw index again based on the current platform rules.

- If the PF is in the slot of ITS A, only ITS A of the same socket can be selected.
- If the PF is in the slot of ITS B, ITS C, or ITS D, only ITS B, ITS C, or ITS D of the same socket can be selected.
- Cross-socket selection is not allowed.

### The Current ITS Chip of the PF Does Not Meet the `GITS_IIDR == 0x00070736U` Requirement

**Symptom**

`sriov_vf_its_indices` fails to be written.

The common log output is as follows:

```text
VF ITS selection is not supported on ITS IIDR 0x<value>
```

Or

```text
VF ITS selection is unavailable for PF ITS base <addr>: unable to identify ITS chip
```

**Causes**

The current server chip is not in the trustlist, or the chip identity of the current ITS used by the PF cannot be read.

**Solution**

Ensure that the current server chip is supported by this feature, the current ITS used by the PF has been correctly initialized, and `GITS_IIDR` can be obtained.

### Failed to Obtain the Target MSI Domain During VF Running

**Symptom**

The PF configuration has been written, but an error related to driver initialization or the MSI/MSI-X path is reported after VFs are enabled.

**Causes**

If the PF has been explicitly configured with `sriov_vf_its_indices`, the target raw index must be parsed into a valid PCI MSI domain during VF running. Otherwise, the kernel will not roll back to the default MSI domain of the host bridge.

**Solution**

Check the configuration values, the raw index mapping table of the current platform, and `dmesg` logs to ensure that each candidate raw index can be parsed into a valid PCI MSI domain.

## Acronyms and Abbreviations

| Acronym/Abbreviation| Full Spelling|
|--|--|
| BDF | Bus, Device, and Function |
| IORT | I/O Remapping Table |
| ITS | interrupt translation service|
| MSI | message-signaled interrupt|
| MSI-X | extended message-signaled interrupt|
| PF | physical function|
| SR-IOV | single root I/O virtualization|
| VF | virtual function|
