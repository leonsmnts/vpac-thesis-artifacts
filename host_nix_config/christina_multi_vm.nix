{ pkgs, ... }:
{
  imports = [
    ../modules/hardware/supermicro-x12spw-tf.nix
    ../modules/nfs/client.nix
    ../modules/ci.nix
    ../modules/vfio/iommu-intel.nix
    ../modules/dpdk.nix
  ];

  powerManagement.enable = true;
  powerManagement.cpuFreqGovernor = "performance";

  boot.kernelParams = [
    "nosmt"
    "isolcpus=3-11"
    "nohz_full=3-11"
    "rcu_nocbs=3-11"
    "irqaffinity=0-2"
    "kthread_cpus=0-2"
    "intel_idle.max_cstate=0"
    "idle=poll"
    "nowatchdog"
    "tsc=reliable"
    "skew_tick=1"
    "rcu_nocb_poll"
    "audit=0"
  ];
  
  systemd.settings.Manager = {
    CPUAffinity = "0-2";
  };
  
  systemd.user.extraConfig = ''
    [Manager]
    CPUAffinity=0-2
  '';

  virtualisation.libvirtd = {
    enable = true;
    qemu.package = pkgs.qemu_full;
  };

  environment.systemPackages = with pkgs; [
    libvirt
  ];


  boot.hugepages1GB.number = 48;
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
