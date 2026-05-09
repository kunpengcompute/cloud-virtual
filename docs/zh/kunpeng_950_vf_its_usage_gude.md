# VF设备选择ITS 用户指南

## 介绍

### 简介

本文介绍如何在鲲鹏950服务器中部署和使用VF设备选择ITS新特性。

在支持多个ITS（Interrupt Translation Service，中断翻译服务）的服务器平台上，物理PCI设备PF（Physical Function）下创建的虚拟PCI设备VF（Virtual Function）默认通常沿用IORT/PCI的设备中断域解析路径。该默认路径迫使VF设备只能选择与PF设备相同的ITS，当PF设备所使用的ITS压力过大时，会导致VF设备性能受到影响。

VF设备选择ITS新特性提供PF级sysfs接口，允许用户为某个PF配置一组VF可使用的ITS设备（通过指定ITS raw index）。内核在接受配置前会根据PF当前实际使用的ITS设备、平台拓扑规则和芯片能力白名单进行校验。只有全部条件满足时，配置才会被保存并在后续VF枚举时生效。

当前需要注意的是，该特性并不是通用ITS重映射能力，而是受平台约束的VF ITS选择能力。配置项必须与当前平台IORT ITS注册顺序、PF当前ITS位置以及芯片型号匹配。

### 功能架构

VF ITS新特性由PCI IOV层、ACPI IORT策略层、GIC ITS驱动和PCI probe路径协同完成。

| 模块 | 功能 |
|--|--|
| PCI IOV层 | 提供PF设备上的`sriov_vf_its_indices` sysfs节点，解析并保存VF ITS raw index配置。 |
| ACPI IORT策略层 | 解析PF当前ITS，校验candidate raw index是否满足平台拓扑和芯片能力约束，并在VF运行时解析目标MSI domain。 |
| GIC ITS驱动 | 提供ITS芯片身份读取能力，用于判断当前服务器芯片是否在支持范围内。 |
| PCI probe路径 | 在VF枚举时设置MSI domain；当PF已显式配置VF ITS时，避免配置错误被bridge默认MSI domain fallback静默掩盖。 |

典型流程如下：

1. 用户在PF设备路径下写入`sriov_vf_its_indices`。
2. PCI IOV层解析raw index数组。
3. IORT策略层以PF当前实际使用的ITS为锚点执行校验。
4. 校验通过后，PF保存该raw index数组。
5. 启用VF后，VF按自身`vf_id`从PF保存的数组中选择raw index。
6. IORT根据raw index解析目标PCI MSI domain，并交给VF使用。

### 对外接口

VF ITS新特性使用PF设备路径下的sysfs节点：

```text
/sys/bus/pci/devices/<PF_BDF>/sriov_vf_its_indices
/sys/bus/pci/devices/<PF_BDF>/sriov_numvfs
```

其中：

- `sriov_vf_its_indices`用于写入VF可使用的ITS raw index数组，属于本特性新增接口。取值范围为0到7。
- `sriov_numvfs`用于指定该PF创建多少VF设备，数值为0时关闭该PF下的VF，该接口在VF ITS特性开发前已经存在。取值范围不能超过PF设备驱动设定的最大VF数量。

`sriov_vf_its_indices`写入的是IORT ITS列表中的raw index，不是ITS base address，也不是`translation_id`。

## 环境要求


启用本特性之前，请确认软硬件环境满足要求。

**硬件要求**

| 项目 | 说明 |
|--|--|
| 处理器 | 鲲鹏950处理器 |
| PCIe设备 | NVME盘或PCIE网卡 |

**软件要求**

| 项目 | 版本或说明 |
|--|--|
| OS | `openEuler 2403 SP3` |
| 内核源码基线 | `OLK-6.6 6.6.0-133.0.0`  |
| libvirt | `9.1.0（yum源）`  |
| qemu | `8.2.0（yum源）`  |
| bios版本 | `10.79`  |


## 获取并合入VF ITS补丁

### 获取补丁

VF ITS补丁获取链接如下：

```text
https://gitcode.com/boostkit/cloud-virtual/tree/master/kernel/kernel-6.6.0/vf-its-patch
```

需要获取以下两个补丁文件，并保持应用顺序不变：

1. `0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch`
2. `0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch`


### 获取目标内核源码

克隆openEuler内核源码并切换到`OLK-6.6`分支，确保源码包含tag 6.6.0-133.0.0：

```bash
git clone https://gitcode.com/openeuler/kernel.git -b OLK-6.6 --depth=1
cd kernel
git branch --show-current
```

期望输出：

```text
OLK-6.6
```


### 合入补丁
1. 合入补丁前，建议确认源码目录干净：

   ```bash
   git status --short
   ```

    若命令没有输出，表示当前工作区没有未提交修改。

2. 在内核源码根目录执行以下命令，按邮件补丁流合入VF ITS补丁：

   ```bash
   git am --reject ~/vf-its-patch/0001-PCI-ACPI-Support-ITS-selection-for-PCI-VF-devices.patch
   git am --reject ~/vf-its-patch/0002-PCI-Fix-kabi-broken-for-SR-IOV-exported-symbols.patch
   git log --oneline -n 2
   ```

   期望最新两条提交类似如下：

   ```text
   <newest> PCI: Fix kabi broken for SR-IOV exported symbols
   <older>  PCI/ACPI: Support ITS selection for PCI VF devices
   ```


## 编译并安装内核

### 编译前准备

开始编译前，请确保系统已安装openEuler内核RPM构建依赖，且构建目录有足够空间保存源码、中间文件和RPM包。

可再次确认当前源码状态：

```bash
git status --short
```

### 编译RPM

在内核源码根目录执行：

```bash
make binrpm-pkg -j$(nproc)
```

构建时间与服务器配置、内核配置和并发数有关。构建完成后，RPM包通常生成在源码目录的上一级目录。

### 安装RPM

根据实际生成的RPM文件名安装新内核包。以下命令仅为示例，请在执行前确认内核RPM包是本次构建产物：

```bash
sudo rpm -ivh <内核rpm名称> --force
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

## 使用VF ITS新特性

### 确认PF设备路径

首先确认目标PF的BDF。以`0000:03:00.0`为示例，实际操作时请替换为目标设备的BDF。

```bash
export PF_BDF=0000:03:00.0
cd /sys/bus/pci/devices/${PF_BDF}
pwd
```

期望路径类似：

```text
/sys/bus/pci/devices/0000:03:00.0
```

确认PF支持SR-IOV且已暴露VF ITS相关节点。

```bash
ls sriov_totalvfs sriov_numvfs sriov_vf_its_indices
cat sriov_totalvfs
cat sriov_numvfs
```

注意：

- `sriov_vf_its_indices`只出现在PF设备路径下，不出现在VF路径下。
- 修改`sriov_vf_its_indices`前，必须先保证`sriov_numvfs`为`0`。

### 配置前关闭VF

如果PF当前已经启用了VF，请先关闭该PF下的全部VF。

```bash
echo 0 | sudo tee sriov_numvfs
cat sriov_numvfs
```

期望输出：

```text
0
```

当前实现只允许在`sriov_numvfs == 0`时修改`sriov_vf_its_indices`。如果VF尚未关闭，写入会返回忙。

### 清空旧配置

如果需要清空PF上已经保存的VF ITS raw index配置，可使用以下任一方式。

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

- `cat sriov_vf_its_indices`通常只会输出一个空行。
- VF会恢复到默认IORT/PCI解析路径，不再受PF显式ITS配置控制。

### 写入ITS raw index数组

写入`sriov_vf_its_indices`时，请使用当前平台IORT ITS列表中的raw index。该值不是ITS base address，也不是`translation_id`。

例如，将PF下的VF配置为在raw index `6`和`7`之间循环分配。

```bash
echo 6,7 | sudo tee sriov_vf_its_indices
cat sriov_vf_its_indices
```

期望输出：

```text
6,7
```

VF到ITS raw index的映射规则为：

```text
raw_index = pf->vf_its_indices[vf_id % nr_indices]
```

当`sriov_vf_its_indices=6,7`时，映射关系如下：

| VF | raw index |
|--|--|
| VF0 | 6 |
| VF1 | 7 |
| VF2 | 6 |
| VF3 | 7 |

写入成功仅表示该raw index数组已经通过当前PF ITS、平台拓扑和芯片白名单约束校验。不同PF或不同平台上的合法raw index可能不同。

### 启用VF

写入raw index后，再启用VF。以创建4个VF为例。

```bash
export VF_COUNT=4
echo ${VF_COUNT} | sudo tee sriov_numvfs
cat sriov_numvfs
```

期望输出：

```text
4
```

查看PF下生成的VF软链接。

```bash
ls -l virtfn*
```

也可以通过`lspci`查看PF与VF设备。

```bash
lspci -nn | grep -E "${PF_BDF#0000:}|Virtual Function"
```

### 查看运行日志

启用VF后，可通过`dmesg`查看VF ITS解析日志。

```bash
dmesg | grep -E 'VF ITS|VF uses ITS raw index|sriov_vf_its_indices|ITS raw index'
```

当VF成功解析到目标ITS时，常见日志形态类似：

```text
VF uses ITS raw index <n> base <base_addr>
```

## 当前平台raw index对照表

当前平台raw index与ITS base address的对应关系如下所示。

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

该表只适用于当前平台和当前IORT ITS注册顺序，不应作为跨平台稳定ABI使用。

结合当前平台规则：

- PF在`index0`或`index4`对应的A槽位时，VF只能选择同socket的A。
- PF在B、C、D时，VF只能选择同socket的B、C、D。
- 不允许跨socket选择ITS。
- 若PF或candidate ITS不在平台静态表中，当前策略会保守退化为只允许candidate与PF当前ITS完全相同。

## 验证与排障

### 最小验证步骤

建议按以下顺序检查VF ITS配置是否生效。

1. 检查PF节点是否存在。

```bash
ls /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
```

2. 检查当前配置是否已写入。

```bash
cat /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
cat /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
```

3. 检查VF是否已生成。

```bash
ls -l /sys/bus/pci/devices/${PF_BDF}/virtfn*
```

4. 检查运行日志。

```bash
dmesg | grep -E 'VF uses ITS raw index|failed to resolve VF ITS raw index|VF ITS selection'
```

### `sriov_numvfs != 0`时写配置失败的解决方法

**现象**

向`sriov_vf_its_indices`写入时返回忙。

**原因**

当前实现要求修改`sriov_vf_its_indices`时必须满足`sriov_numvfs == 0`。

**处理方法**

先关闭该PF下的VF，再重新写入配置：

```bash
echo 0 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_numvfs
echo 6,7 | sudo tee /sys/bus/pci/devices/${PF_BDF}/sriov_vf_its_indices
```

### raw index非法的解决方法

**现象**

向`sriov_vf_its_indices`写入不存在的raw index后，sysfs写入失败。

常见日志如下：

```text
requested VF ITS raw index <n> is invalid
```

**原因**

写入的raw index超出当前IORT ITS列表范围，或者该raw index对应的ITS无法解析出合法的PCI MSI domain。

**处理方法**

对照当前平台raw index对照表重新选择合法raw index，并确认目标ITS已在当前平台上注册且可解析。

### candidate ITS跨socket或违反A/B/C/D规则的解决方法

**现象**

写入raw index时失败。

常见日志如下：

```text
VF ITS raw index <n> base <addr> is not allowed for PF ITS base <addr>
```

**原因**

目标ITS与PF当前ITS不在同一个socket，或candidate ITS违反当前平台A/B/C/D槽位规则。

**处理方法**

先确认PF当前实际使用的ITS，再按当前平台规则重新选择candidate raw index。

- PF在A时，只能选择同socket的A。
- PF在B、C、D时，只能选择同socket的B、C、D。
- 不允许跨socket选择。

### PF当前ITS芯片不满足`GITS_IIDR == 0x00070736U`的解决方法

**现象**

`sriov_vf_its_indices`写入失败。

常见日志如下：

```text
VF ITS selection is not supported on ITS IIDR 0x<value>
```

或：

```text
VF ITS selection is unavailable for PF ITS base <addr>: unable to identify ITS chip
```

**原因**

当前服务器芯片不在白名单内，或者无法读取PF当前ITS的芯片身份。

**处理方法**

确认当前服务器芯片符合该特性的受支持范围，并确认PF当前ITS已正确初始化，且可以获取到`GITS_IIDR`。

### VF运行时无法获得目标MSI domain的解决方法

**现象**

PF配置已经写入，但VF启用后驱动初始化或MSI/MSI-X相关路径报错。

**原因**

PF已显式配置`sriov_vf_its_indices`时，VF运行时必须解析到合法的device-specific MSI domain。如果目标raw index无法解析为合法PCI MSI domain，内核不会再回退到host bridge默认MSI domain。

**处理方法**

检查配置值、当前平台raw index对照表和`dmesg`日志，确认每个candidate raw index都能解析到合法的PCI MSI domain。


## 缩略语

| 缩略语 | 英文全称 | 中文说明 |
|--|--|--|
| BDF | Bus:Device.Function | PCI设备的总线、设备和功能号。 |
| IORT | I/O Remapping Table | ACPI中描述I/O重映射和中断拓扑关系的表。 |
| ITS | Interrupt Translation Service | GICv3中用于MSI/MSI-X中断转换的组件。 |
| MSI | Message Signaled Interrupts | 消息信号中断。 |
| MSI-X | Message Signaled Interrupts eXtended | MSI的扩展形式。 |
| PF | Physical Function | SR-IOV中的物理功能。 |
| SR-IOV | Single Root I/O Virtualization | 单根I/O虚拟化技术。 |
| VF | Virtual Function | SR-IOV中由PF创建的虚拟功能。 |
