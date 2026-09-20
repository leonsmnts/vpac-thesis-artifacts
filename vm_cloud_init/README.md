# VM Cloud-Init Configuration

Cloud-Init YAML configuration files for both 1vCPU and 2vCPU VMs.

## How to use

Swap `[SSHKEY]` for own public key, this enables `ssh` connectivity to VMs.

Can create the necessary cloud-init disk image, as used in the VM libvirt XML (cf. [./vm_libvirt_xml/](https://todo.add.url)) via:

```bash
$ cloud-localds [seed.img] [user-data-file.yaml]
```

Install via:

```bash
$ sudo apt install cloud-image-utils
```
