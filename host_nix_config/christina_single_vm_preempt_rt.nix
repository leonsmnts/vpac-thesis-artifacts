{ pkgs, lib, ... }:
{
  imports = [
    ../modules/hardware/supermicro-x12spw-tf.nix
    ../modules/nfs/client.nix
    ../modules/ci.nix
    ../modules/vfio/iommu-intel.nix
    ../modules/dpdk.nix
  ];

  # START ENABLE PREEMPT_RT (also include "lib" at top for this)
  # do NOT set boot.kernelPackages here
  # chair infrastructure already deliberately chooses the newest ZFS-compatible kernel
  # in ./configurations.nix -> computeNodeModules -> srvos.nixosModules.mixins-latest-zfs-kernel
  #boot.kernelPackages = pkgs.linuxPackages_6_18;

  boot.kernelPatches = [
    {
      name  = "enable-preempt-rt";
      patch = null;

      structuredExtraConfig = with lib.kernel; {
        EXPERT = yes;
        PREEMPT_RT = yes;
        #PREEMPT_VOLUNTARY = lib.mkForce no;
        
        # i915 is unavailable with PREEMPT_RT on Linux 6.18.
        # NixOS common-config otherwise requests these unconditionally.
        DRM_I915_GVT = lib.mkForce (option no);
        DRM_I915_GVT_KVMGT = lib.mkForce (option no);
      };
    }
  ];
  
  boot.kernel.sysctl."kernel.sched_rt_runtime_us" = -1;

  # END PREEMPT_RT

  powerManagement.enable = true;
  powerManagement.cpuFreqGovernor = "performance";

  boot.kernelParams = [
    "nosmt"
    "isolcpus=8-11"
    "nohz_full=8-11"
    "rcu_nocbs=8-11"
    "irqaffinity=0-7"
    "kthread_cpus=0-7"
    "intel_idle.max_cstate=0"
    "idle=poll"
    "nowatchdog"
    "tsc=reliable"
    "skew_tick=1"
    "rcu_nocb_poll"
    "audit=0"
  ];
  
  systemd.settings.Manager = {
    CPUAffinity = "0-7";
  };
  
  systemd.user.extraConfig = ''
    [Manager]
    CPUAffinity=0-7
  '';

  virtualisation.libvirtd = {
    enable = true;
    qemu.package = pkgs.qemu_full;
  };

  environment.systemPackages = with pkgs; [
    libvirt
  ];


  boot.hugepages1GB.number = 8;
  boot.hugepages2MB.number = let
    gb = 30;
  in gb * 1024 / 2;

  boot.initrd.availableKernelModules = [ "nvme" ];

  networking.hostName = "christina";

  simd.arch = "icelake-server";

  system.stateVersion = "21.11";

  networking.doctor-bridge.enable = true;
  # The mac is missing in ./modules/hosts.nix and therefore doctor-bridge does not work.
  # If it is defined in hosts.nix, this config can be removed from here.
  networking.doctorwho.currentHost.mac = "00:1b:21:c3:85:e0";

  ####
  #### From here for the low latency bridge connection to VMs
  ####

  systemd.network.netdevs."04-lowlat-bridge" = {
    netdevConfig = {
      Name = "lowlat-bridge";
      Kind = "bridge";
    };

    extraConfig = ''
      [Bridge]
      STP=false
      ForwardDelaySec=0
      MulticastSnooping=false
    '';
  };

  systemd.network.networks."04-lowlat-eno1" = {
    matchConfig.Name = "eno1";

    networkConfig = {
      Bridge = "lowlat-bridge";
      DHCP = "no";
      LinkLocalAddressing = "no";
      IPv6AcceptRA = false;
    };

    linkConfig.RequiredForOnline = "enslaved";
  };

  systemd.network.networks."04-lowlat-bridge" = {
    matchConfig.Name = "lowlat-bridge";
    
    networkConfig = {
      DHCP = "no";
      LinkLocalAddressing = "no";
      IPv6AcceptRA = false;
    };

    linkConfig.RequiredForOnline = "no";
  };

  # Prevent networkd from treating lltap* as normal Eth device and assigning IPs
  systemd.network.networks."04-lowlat-libtap" = {
    matchConfig.Name = "lltap*";

    linkConfig.Unmanaged = true;
  };

}
