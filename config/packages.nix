{
  pkgs,
  ...
}:
{
  packages =
    with pkgs;
    [
      binsider

      probe-rs-tools

      ninja
      ccache
      dfu-util

      binaryen

      platformio

      openocd
    ];
}
