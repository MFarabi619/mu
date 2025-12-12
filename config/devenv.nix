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
    ./scripts.nix
  ];

  env = {
    GREET = "devenv";
    ELEVENLABS_API_KEY = "";
    ELEVENLABS_VOICE_ID = "";
  };

  # processes.dev.exec = "${lib.getExe pkgs.watchexec} -n -- ls -la";

  # services.postgres.enable = true;

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
