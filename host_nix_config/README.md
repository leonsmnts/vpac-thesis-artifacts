# Host .nix Configuration Files

Nix-Configuration files for both physical hosts `River` and `Christina`.

## How to use

The chairs systems run on the Nix configuration specified [in this linked repository](https://github.com/TUM-DSE/doctor-cluster-config). 

Replacing the associated `./hosts/[host].nix` files on the `host` system yields the appropriate configuration.

Use the `River` and `Christina` configurations included in this artifact repository as `river.nix` and `christina.nix` instead of the chairs files in `./hosts/`.

## Included Configurations

### River

- [`river.nix`](river.nix)

### Christina

- [`christina_single_vm.nix`](christina_single_vm.nix)
  - Configuration used in the single-VM experiments
- [`christina_multi_vm.nix`](christina_multi_vm.nix)
  - Configuration used in the multi-VM experiments
  - Differences to single-VM:
    - CPU pool sizes for housekeeping and RT
    - amount of 1 GiB hugepages.
- [`christina_single_vm_preempt_rt.nix`](christina_single_vm_preempt_rt.nix)
  - Unused configuration, equivalent to single VM, however enables a `PREEMPT_RT` kernel. Potentially useful for further investigation.
