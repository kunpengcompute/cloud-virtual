# VF ITS 新特性使用文档

## 1. 文档说明

本文档说明如何在 `OLK-6.6` 基线上获取 VF ITS 特性补丁、合入补丁、编译并安装内核，以及如何在系统中使能和验证该特性。


## 2. 特性概览

VF ITS 特性的目标是让 PF 通过 sysfs 为其 VF 指定可使用的 ITS raw index 集合，但该配置只有在以下约束同时满足时才会生效：

- 约束 1：VF ITS 选择始终受 PF 当前实际使用的 ITS 约束。
- 约束 2：PF 在 A 时，VF 只能选择同 socket 的 A。
- 约束 3：PF 在 B、C、D 时，VF 只能选择同 socket 的 B、C、D。
- 约束 4：不允许跨 socket 选择 ITS。
- 约束 5：当前仅支持 `GITS_IIDR == 0x00070736U` 的 ITS 芯片。

当前对外接口如下：

- `/sys/bus/pci/devices/<PF_BDF>/sriov_vf_its_indices`
- `/sys/bus/pci/devices/<PF_BDF>/sriov_numvfs`

其中：

- `sriov_vf_its_indices` 用于写入 ITS raw index 数组。
- `sriov_numvfs` 用于启用或关闭 VF。

## 3. 获取补丁

补丁来源页：

- `https://gitcode.com/boostkit/cloud-virtual/tree/master/kernel/kernel-6.6.0/vf-its-patch`

需要获取以下两个补丁文件，并保持应用顺序不变：

1. `0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch`
2. `0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch`

建议将补丁下载到单独目录，例如 `~/vf-its-patch/`：

```bash
mkdir -p ~/vf-its-patch
cd ~/vf-its-patch
ls -1
```

期望看到：

```text
0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch
0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch
```

## 4. 获取目标仓库

克隆目标内核仓库：

```bash
git clone https://gitcode.com/openeuler/kernel.git -b OLK-6.6 --depth=1
cd kernel
git branch --show-current
```

期望输出：

```text
OLK-6.6
```

## 5. 合入补丁

按邮件补丁流使用 `git am` 合入补丁，顺序必须先 `0001` 再 `0002`：

```bash
git am --reject ~/vf-its-patch/0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch
git am --reject ~/vf-its-patch/0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch
git log --oneline -n 2
```

期望最新两条提交为：

```text
<newest> PCI: Fix kabi broken for SR-IOV exported symbols
<older>  PCI/ACPI: Support ITS selection for PCI VF devices
```


## 6. 编译内核并安装 RPM

### 6.1 前置准备

开始编译前，请确保满足以下条件：

- 系统已安装 openEuler `OLK-6.6` 内核 RPM 构建所需依赖。
- 构建目录有足够磁盘空间保存源码、中间文件和 RPM 包。
- 当前用户具备安装 RPM 和重启系统的权限。

建议先确认当前源码目录干净，避免把无关改动带入构建：

```bash
git status --short
```

### 6.2 编译 RPM

在内核源码根目录执行：

```bash
make binrpm-pkg -j$(nproc)
```

### 6.3 安装 RPM

使用 `rpm -ivh --force` 安装新内核包。请根据实际生成的 RPM 文件名替换下面命令中的文件名：

```bash
sudo rpm -ivh ../kernel-*.rpm ../kernel-core-*.rpm ../kernel-modules-*.rpm --force
```

安装完成后，重启系统使新内核生效：

```bash
sudo reboot
```

系统重启后，确认当前运行的内核版本已经切换到新安装版本：

```bash
uname -r
rpm -qa | grep '^kernel' | sort
```

## 7. 使能 VF ITS 新特性

### 7.1 确认 PF 设备路径

首先确认目标 PF 的 BDF，例如 `0000:03:00.0`，并进入对应 sysfs 目录：

```bash
export PF_BDF=0000:03:00.0
cd /sys/bus/pci/devices/${PF_BDF}
pwd
```

期望路径类似：

```text
/sys/bus/pci/devices/0000:03:00.0
```

确认 PF 支持 SR-IOV 且已暴露相关节点：

```bash
ls sriov_totalvfs sriov_numvfs sriov_vf_its_indices
cat sriov_totalvfs
cat sriov_numvfs
```

注意：

- `sriov_vf_its_indices` 只出现在 PF 设备路径下，不出现在 VF 路径下。
- 修改 `sriov_vf_its_indices` 之前，必须先保证 `sriov_numvfs` 为 `0`。

### 7.2 配置前先关闭 VF

如果 PF 当前已经启用了 VF，先关闭全部 VF：

```bash
echo 0 | sudo tee sriov_numvfs
cat sriov_numvfs
```

期望输出：

```text
0
```

当前实现中，`sriov_vf_its_indices` 只允许在 `sriov_numvfs == 0` 时修改。若 VF 尚未关闭，写入会返回忙。

### 7.3 清空旧配置

如果需要清空 PF 上已经保存的 ITS raw index 配置，可使用以下任一方式：

```bash
echo -1 | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

或：

```bash
echo | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

清空后：

- `cat sriov_vf_its_indices` 通常只会输出一个空行。
- VF 会恢复到默认 IORT/PCI 解析路径，不再受 PF 显式 ITS 配置控制。

### 7.4 写入 ITS raw index 数组

当前接口写入的是 raw index，不是 ITS base address，也不是 `translation_id`。

例如，将 PF 下的 VF 配置为在 raw index `6` 和 `7` 之间循环分配：

```bash
echo 6,7 | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

期望输出：

```text
6,7
```

当前 VF 到 ITS 的映射规则为：

```text
raw_index = pf->vf_its_indices[vf_id % nr_indices]
```

例如，当 `sriov_vf_its_indices=6,7` 时：

- `VF0 -> 6`
- `VF1 -> 7`
- `VF2 -> 6`
- `VF3 -> 7`

### 7.5 使能 VF

写入 raw index 后，再启用 VF：

```bash
export VF_COUNT=4
echo ${VF_COUNT} | sudo tee sriov_numvfs
cat sriov_numvfs
```

期望输出：

```text
4
```

启用后查看 PF 与 VF 设备：

```bash
lspci -nn | grep -E "${PF_BDF#0000:}|Virtual Function"
```

也可以直接查看该 PF 下生成的 VF 软链接：

```bash
ls -l virtfn*
```

### 7.6 查看运行日志

启用 VF 后，可通过 `dmesg` 查看 VF ITS 解析日志：

```bash
dmesg | grep -E 'VF ITS|VF uses ITS raw index|sriov_vf_its_indices|ITS raw index'
```

当 VF 成功解析到目标 ITS 时，常见日志形态类似：

```text
VF uses ITS raw index <n> base <base_addr>
```

## 8. 当前平台 raw index 对照表

当前平台 raw index 与 ITS base address 的对应关系如下：

```text
index0: 0x7010000000
index1: 0x6888000000
index2: 0x4588000000
index3: 0x4488000000
index4: 0x3010000000
index5: 0x2888000000
index6: 0x0588000000
index7: 0x0488000000
```

说明：

- 该表适用于当前平台和当前 IORT ITS 注册顺序。
- raw index 是 `iort_msi_chip_list` 当前顺序上的原始索引。
- 不应把该表直接当作跨平台稳定 ABI 使用。

结合当前平台规则：

- PF 在 `index0` 或 `index4` 所对应的 A 槽位时，VF 只能选同 socket 的 A。
- PF 在 B、C、D 时，VF 只能在同 socket 的 B、C、D 内选择。
- 任何跨 socket 选择都不允许。

## 9. 机制说明

### 9.1 `sriov_vf_its_indices` 的语义

`sriov_vf_its_indices` 是 PF 设备上的 sysfs 节点，用于保存一个 raw index 数组。该数组并不是“无条件生效”的目标 ITS 列表，而是一个需要经过平台策略校验的候选集合。

只有当以下检查全部通过时，配置才会被接受：

- PF 当前 ITS 可以被正确解析。
- candidate raw index 可以解析到合法的 ITS 和 PCI MSI domain。
- candidate ITS 满足 PF 当前 ITS 对应的 socket 和 A/B/C/D 拓扑规则。
- PF 当前 ITS 芯片满足 `IIDR == 0x00070736U`。

### 9.2 PF 当前 ITS 约束

当前实现并不是仅根据用户写入的 raw index 决定 VF 的目标 ITS。内核会先解析 PF 当前已经绑定的 MSI domain，再反查 PF 当前实际使用的 ITS，把它作为整套策略判断的锚点。

这意味着：

- 同一个 raw index 在不同 PF 上不一定合法。
- 配置是否允许，取决于 PF 当前落在哪个 ITS 上。

### 9.3 运行时行为

PF 配置写入成功后，VF 在运行时会根据自己的 `vf_id` 从 PF 保存的数组中取 raw index，并将该 raw index 解析为目标 PCI MSI domain。

如果 PF 没有配置 `sriov_vf_its_indices`，系统保持原有默认行为。

如果 PF 已经配置了 `sriov_vf_its_indices`，但 VF 在运行时无法解析到合法的 device-specific MSI domain，则：

- 内核不会再回退到 host bridge 默认 MSI domain。
- VF 设备对象不一定会直接消失。
- 更准确地说，VF 将拿不到合法的 device-specific MSI domain，后续依赖 MSI 或 MSI-X 的驱动路径会继续暴露错误。

## 10. 验证与排障

### 10.1 最小验证步骤

建议按以下顺序检查：

1. 检查 PF 节点是否存在：

```bash
ls /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
```

2. 检查当前配置是否已写入：

```bash
cat /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
cat /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
```

3. 检查 VF 是否已生成：

```bash
ls -l /sys/bus/pci/devices/${PF_BDF}/virtfn*
```

4. 检查运行日志：

```bash
dmesg | grep -E 'VF uses ITS raw index|failed to resolve VF ITS raw index|VF ITS selection'
```

### 10.2 `sriov_numvfs != 0` 时写配置失败

现象：

- 向 `sriov_vf_its_indices` 写入时返回忙。

原因：

- 当前实现要求修改 `sriov_vf_its_indices` 时必须满足 `sriov_numvfs == 0`。

处理方法：

```bash
echo 0 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
echo 6,7 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
```

### 10.3 raw index 非法

现象：

- 向 `sriov_vf_its_indices` 写入不存在的 raw index 后，sysfs 写入失败。

常见日志：

```text
requested VF ITS raw index <n> is invalid
```

原因：

- 写入的 raw index 超出当前 IORT ITS 列表范围。
- 或者该 raw index 对应的 ITS 无法解析出合法的 PCI MSI domain。

处理方法：

- 对照本文“当前平台 raw index 对照表”重新选择合法 raw index。
- 确认目标 ITS 已在当前平台上注册并可解析。

### 10.4 candidate ITS 跨 socket 或违反 A/B/C/D 规则

现象：

- 写入 raw index 时失败。

常见日志：

```text
VF ITS raw index <n> base <addr> is not allowed for PF ITS base <addr>
```

原因：

- 目标 ITS 与 PF 当前 ITS 不在同一个 socket。
- 或者 PF 当前位于 A，而你选择了同 socket 的 B、C、D。
- 或者 PF 当前位于 B、C、D，而你选择了不满足规则的 ITS。

处理方法：

- 先确认 PF 当前实际使用的 ITS。
- 按本文的平台限制重新选择 candidate raw index。

### 10.5 PF 当前 ITS 芯片不满足 `IIDR == 0x00070736U`

现象：

- `sriov_vf_its_indices` 写入失败。

常见日志：

```text
VF ITS selection is not supported on ITS IIDR 0x<value>
```

或：

```text
VF ITS selection is unavailable for PF ITS base <addr>: unable to identify ITS chip
```

原因：

- 当前 PF 绑定的 ITS 芯片不在白名单内。
- 或者无法读取 PF 当前 ITS 的芯片身份。

处理方法：

- 确认当前平台和 ITS 芯片符合该特性的受支持范围。
- 确认 PF 当前 ITS 已正确初始化，且可以获取到 `GITS_IIDR`。


