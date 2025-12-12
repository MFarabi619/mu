  {
    scripts = {
    list = {
      exec = ''
        probe-rs list
        pio device list
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
}
