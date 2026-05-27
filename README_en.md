# Project Introduction<a name="EN-US_TOPIC_0000002440951704"></a>

The Kunpeng cloud computing virtualization code repository is the result of **open-source virtualization software stack optimization** by Kunpeng. It stores patches related to **Kunpeng BoostKit for Virtualization**, which covers core virtualization and I/O software stacks such as **KVM**, **QEMU**, **libvirt**, **DPDK**, and **SPDK**.

-   The cross-generation live VM migration is a feature developed by Kunpeng and used together with QEMU/KVM. It introduces the vCPU feature read/write framework to ensure vCPU feature compatibility on servers of different generations. This feature is applicable to VM live migration between Kunpeng 920 and new Kunpeng 920 processor models.
-   L0 memory is a memory acceleration feature developed by Kunpeng. In scenarios where the last level cache (LLC) capacity is not sensitive, **some LLC capacity can be used as L0 memory** to implement high-performance access to hotspot data. Open vSwitch (OVS) is a high-quality virtual switch that supports multi-layer data forwarding. Its main architecture consists of the kernel-space datapath and user-space vswitchd and OVSDB. The data packets read from network ports are quickly matched against the flow table in datapath. If the matching fails, the packets are submitted to vswitchd for processing. The L0-accelerated kernel-space flow table matching solution offloads the kernel-space datapath forwarding path to L0 memory, accelerating flow table search and improving packet forwarding efficiency.
-   The DPDK queue selection optimization is a network acceleration feature developed by Kunpeng and used together with DPDK. It optimizes the DPDK queue selection algorithm to reduce the load pressure on a single CPU and the network latency. This feature is applicable to the OVS+DPDK VM networking scenarios.
-   SPDK interrupt aggregation is a storage performance acceleration feature developed by Kunpeng and used together with SPDK. The interrupt aggregation technology reduces the number of interrupt notifications from the backend to the frontend as well as VM entries/exits, thereby improving the overall system throughput performance. This feature is applicable to scenarios where VMs use NVMe drives managed by SPDK.

# Version Description<a name="EN-US_TOPIC_0000002441116972"></a>

<a name="table740710612324"></a>
<table><thead align="left"><tr id="row144075603214"><th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.1.4.1.1"><p id="p114078693216"><a name="p114078693216"></a><a name="p114078693216"></a>OS</p>
</th>
<th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.1.4.1.2"><p id="p84081465329"><a name="p84081465329"></a><a name="p84081465329"></a>Open-Source Software</p>
</th>
<th class="cellrowborder" valign="top" width="33.33333333333333%" id="mcps1.1.4.1.3"><p id="p19408196163219"><a name="p19408196163219"></a><a name="p19408196163219"></a>Feature</p>
</th>
</tr>
</thead>
<tbody><tr id="row64089653211"><td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.1 "><p id="p24084615327"><a name="p24084615327"></a><a name="p24084615327"></a><span>openEuler 24.03 LTS SP1</span> (<span>6.6.0-72.0.0</span>)</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.2 "><p id="p1040846123216"><a name="p1040846123216"></a><a name="p1040846123216"></a>QEMU-8.2.0</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.3 "><p id="p19408196173215"><a name="p19408196173215"></a><a name="p19408196173215"></a>Cross-generation VM live migration</p>
</td>
</tr>
<tr id="row44089619323"><td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.1 "><p id="p64089617323"><a name="p64089617323"></a><a name="p64089617323"></a><span>openEuler 22.03 LTS SP4</span></p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.2 "><p id="p540820673214"><a name="p540820673214"></a><a name="p540820673214"></a>openvswitch-2.12.4</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.3 "><p id="p10408369326"><a name="p10408369326"></a><a name="p10408369326"></a><span>L0-accelerated OVS kernel-space flow table matching</span></p>
</td>
</tr>
<tr id="row54088663218"><td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.1 "><p id="p114081164322"><a name="p114081164322"></a><a name="p114081164322"></a><span>openEuler 24.03 LTS SP1</span></p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.2 "><p id="p540815617322"><a name="p540815617322"></a><a name="p540815617322"></a>DPDK-24.11</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.3 "><p id="p1140886103218"><a name="p1140886103218"></a><a name="p1140886103218"></a><span>DPDK queue selection</span></p>
</td>
</tr>
<tr id="row127821831195814"><td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.1 "><p id="p1278214319581"><a name="p1278214319581"></a><a name="p1278214319581"></a><span>openEuler 24.03 LTS SP1</span></p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.2 "><p id="p1778293145813"><a name="p1778293145813"></a><a name="p1778293145813"></a>SPDK-24.05</p>
</td>
<td class="cellrowborder" valign="top" width="33.33333333333333%" headers="mcps1.1.4.1.3 "><p id="p278273115810"><a name="p278273115810"></a><a name="p278273115810"></a><span>SPDK interrupt aggregation</span></p>
</td>
</tr>
</tbody>
</table>

# Environment Deployment<a name="EN-US_TOPIC_0000002441276832"></a>

**Deploying the Cross-Generation VM Live Migration Feature<a name="section1772123143910"></a>**

1.  Obtain the kernel and QEMU code from the openEuler repository.
2.  Switch to the corresponding branch and use the script in the `tools` directory to apply the patch.
3.  For details, see [Kunpeng BoostKit 25.1.RC1 Kunpeng 920 Cross-Generation VM Live Migration Feature Guide](https://gitcode.com/boostkit/cloud-virtual/blob/master/docs/en/kunpeng_920_cross_generation_vm_live_migration_feature_guide.md).

**Deploying the L0-accelerated OVS Kernel-Space Flow Table Matching Feature<a name="section1073682415417"></a>**

1.  Pull the source code of version 5.10.0-216 from the repository.

    ```
    git clone https://gitee.com/openeuler/kernel.git -b 5.10.0-216.0.0 --depth=1
    ```

2.  Save the patch as a .patch file.
3.  Apply the patch.

    ```
    git am --reject <Patch_name>
    ```

4.  Compile the OVS module in the `kernel/net/openvswitch` directory.

    ```
    make CONFIG_XENO_DRIVERS_NET_DRV_IGB=m -C <Source_code_directory> M=pwd modules
    ```

**Deploying the DPDK Queue Selection Optimization Feature<a name="section1934464794214"></a>**

1.  Use `git` to pull the DPDK 24.11 source code.
2.  Apply the patch for DPDK queue selection.
3.  Compile and install DPDK 24.11.

**Deploying the SPDK Interrupt Aggregation Feature<a name="section1818515266438"></a>**

1.  Use `git` to pull the SPDK 24.05 source code.
2.  Apply the patch for SPDK interrupt aggregation.
3.  Compile and install SPDK 24.05.

# Quick Start<a name="EN-US_TOPIC_0000002474516797"></a>





## Cross-Generation Live Migration<a name="EN-US_TOPIC_0000002474636969"></a>

After the environment is deployed, start the VMs according to the deployment document, run the following command to perform a cross-generation migration test, and check whether the migration is complete and no error is reported.

```
virsh migrate --verbose --persistent --live --unsafe <VM_name> qemu+tcp://<IP_address_of_the_target_physical_machine>/system
```

## L0 Memory-based Acceleration<a name="EN-US_TOPIC_0000002441116976"></a>

1.  Load the L0 module.

    ```
    modprobe hisi_l3t
    modprobe hisi_l0
    ```

2.  Stop the OVS service that has been enabled.

    ```
    service openvswitch status
    service openvswitch stop
    ovs-ctl stop
    ```

3.  Load the compiled `openvswitch.ko`.

    ```
    insmod openvswitch.ko
    ```

4.  After the `openvswitch.ko` is loaded, start the OVS service to use the L0-accelerated kernel-space flow table matching.

## DPDK Queue Selection Optimization<a name="EN-US_TOPIC_0000002442189534"></a>

After the environment is deployed, perform iPerf traffic sending test on the VM, and run the following command on the physical machine to check whether the load of the PMD polling cores is evenly distributed.

```
ovs-appctl dpif-netdev/pmd-rxq-show -secs 5
```

![](figures/en-us_image_0000002475320297.png)

## SPDK Interrupt Aggregation<a name="EN-US_TOPIC_0000002475309653"></a>

After the environment is deployed, use the fio tool to test the SPDK NVMe drive performance on the VM with 4 vCPUs and 8 GB memory and compare the performance with that on the physical machine. Check whether the virtualization loss is less than 10%.

# Contribution Guide<a name="EN-US_TOPIC_0000002441276836"></a>

If you have any questions or want to provide feedback on feature requirements and bug reports, you can submit issues. For details, see the [contribution guideline](https://gitcode.com/boostkit/community/blob/master/docs/contributor/contributing.md).

# Disclaimer<a name="EN-US_TOPIC_0000002474516801"></a>

This code repository contributes to core virtualization open-source projects solely for extending the VM live migration function and improving network and storage performance. It strictly adheres to the coding style and methods, as well as security design of the native open-source software. Any vulnerability and security issues of the software shall be resolved by the corresponding upstream communities according to their response mechanisms. Please pay attention to the notifications and version updates released by the upstream communities. The Kunpeng computing community does not assume any responsibility for software vulnerabilities and security issues. The code for cross-generation live migration is used to demonstrate the usage and integration method and is for reference only. It shall not be used in production environments, and does not inherit or promise any security design and protection mechanism of upstream or downstream software. Users should evaluate risks and perform security hardening based on actual scenarios. Any security issues caused by the use of the repository code shall be borne by the user.

# License<a name="EN-US_TOPIC_0000002474636973"></a>

This project is licensed under Apache License 2.0. For details, see [LICENSE](https://gitcode.com/boostkit/cloud-virtual/blob/master/LICENSE).
The documents of this project are licensed under CC-BY 4.0. For details, see [LICENSE](https://gitcode.com/boostkit/cloud-virtual/blob/master/docs/LICENSE).
