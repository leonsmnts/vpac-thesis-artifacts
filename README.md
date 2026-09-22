# vpac-thesis-artifacts

Supplementary source code and configuration artifacts for the Bachelor's Thesis "Scheduling for Deterministic Virtualization of Virtualized Protection and Control Workloads on COTS Hardware"

## Citation

If you use the material in this repository in academic work, please cite the
associated Bachelor's Thesis as the primary reference:

> Leon Simoniants, *Scheduling for Deterministic Virtualization of Virtualized Protection and Control Workloads on COTS Hardware*, Bachelor's Thesis, Technical University of Munich (TUM), 2026.

```bibtex
@thesis{simoniants2026vpac,
  author      = {Leon Simoniants},
  title       = {Scheduling for Deterministic Virtualization of Virtualized Protection and Control Workloads on COTS Hardware},
  type        = {Bachelor's Thesis},
  institution = {Technical University of Munich (TUM)},
  year        = {2026}
}
```

When referring specifically to the source code, configuration files, or other
artifacts contained in this repository, the repository itself may additionally
be cited.

Citation metadata is provided in [`CITATION.cff`](CITATION.cff).

## This repo contains...

All of the directories have their own `README.md` files further detailing contents.

- [`host_nix_config/`](host_nix_config/) contains the host-specific `*.nix` configuration files for the experimental systems `River` and `Christina`.

- [`vm_libvirt_xml/`](vm_libvirt_xml/) contains the `libvirt` XML configuration files for evaluated VMs and network.

- [`vm_cloud_init/`](vm_cloud_init/) contains the `cloud-init` initial provisioning YAML configurations.

- [`src/`](src/) contains the implemented reference protection workload as C-code, alongside the measurement related applications.

## Two branches

- `main` The main branch contains the source code used throughout the single-VM experiments.

- `multivm` The multivm branch only differs from `main` in the [`src/`](src/) directory, and contains the source code which is used throughout the multi-VM experiments. The only file that changes is the [`sv_hot_receiver.c`](src/net/sv_hot_receiver.c)
