{
  pkgs,
  lib,
  config,
  inputs,
  ...
}:

{
  imports = [
    ./languages
    ./packages.nix
  ];

  env = {
    GREET = "devenv";
  };

  # processes.dev.exec = "${lib.getExe pkgs.watchexec} -n -- ls -la";

  # services.postgres.enable = true;

  scripts = {
    list = {
      exec = ''
        probe-rs list
        comchan --list-ports
      '';
    };

    clean = {
      exec = "git clean -fdX";
    };

    kernel = {
      description = " 🎉 Fire up the Microvisor Kernel";
      exec = "devenv up";
    };
  };

  enterShell = ''
    hello         # Run scripts directly
    git --version # Use packages
  '';

  # tasks = {
  #   "myproj:setup".exec = "mytool build";
  #   "devenv:enterShell".after = [ "myproj:setup" ];
  # };

  enterTest = ''
    echo "Running tests"
    git --version | grep --color=auto "${pkgs.git.version}"
  '';
}
