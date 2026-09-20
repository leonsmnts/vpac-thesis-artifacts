{
  imports = [
    ../modules/hardware/supermicro-x12spw-tf.nix
    ../modules/nfs/client.nix
    ../modules/dpdk.nix
    ../modules/vfio/iommu-intel.nix
  ];

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
  ];
  
  systemd.settings.Manager = {
    CPUAffinity = "0-7";
  };
  
  systemd.user.extraConfig = ''
    [Manager]
    CPUAffinity=0-7
  '';


  boot.hugepages1GB.number = 8;
  boot.hugepages2MB.number =
    let
      gb = 100;
    in
    gb * 1024 / 2;

  networking.hostName = "river";

  simd.arch = "icelake-server";

  system.stateVersion = "21.11";

  ####
  #### Disable IP config of eno2 - stop DHCP and other noise
  ####

  systemd.network.networks."04-river-eno1" = {
    matchConfig.Name = "eno1";

    networkConfig = {
      DHCP = "no";
      LinkLocalAddressing = "no";
      IPv6AcceptRA = false;
    };

    linkConfig.RequiredForOnline = "no";
  };

}
