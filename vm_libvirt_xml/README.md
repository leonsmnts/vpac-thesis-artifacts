# VM Libvirt XML Configuration

Libvirt XML configuration files for network (used in multi-VM) and both 1vCPU and 2vCPU VMs.

## How to use

**For single-VM and multi-VM:**

1. For initial provisioning boot, create a `cloud-init disk image`, e.g. from the [provided vm_cloud_init yaml files in this repository](https://todo.add.url) and uncomment the disk device.

2. Replace the template path of the VM image to the actual `qcow2` VM image.

**Additionally only for multi-VM:**

3. Adjust VM Name, set `[X]` to VM id (e.g. `0`)

4. Change the pCPU pinning of the RT vCPU thread.

5. Potentially adjust NUMA tune.

6. Adjust both MAC addresses, set `[X]` to VM id (e.g. `0`)
    - *(lowlat-bridge)* Last byte denotes the `VM_id` in measurements
    - *(default network)* Integrates with fixed IP-address configuration for `ssh` Jump Host.
