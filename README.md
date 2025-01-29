# IOT DASHBOARD DEVICES
This repository holds necessary firmware for running IoT devices connecting to the IoT dashboard running on a server.

This particular example implements a **Zephyr RTOS** application with the **FRDM K64F** board. The firmware should be aplicable for other boards with a network interface and Zephyr support.
For non Zephyr applications, similar steps should be taken.

## Hardware
- [FRDM K64F](https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/frdm-development-platform-for-kinetis-k64-k63-and-k24-mcus:FRDM-K64F)

## Build and Run
1. Follow [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to prepare the environment.
2. On `~/zephyrproject/` clone this repository.
3. Connect your board and flash it with `west`:
```sh
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject/iot_dashboard_devices
west build -b frdm_k64f
west flash
```
> [!NOTE]  
> You need to have LinkServer tools installed, check [LinkServer Debug Host Tools](https://docs.zephyrproject.org/latest/develop/flash_debug/host-tools.html#linkserver-debug-host-tools).
4. You can monitor the serial output with a program like `minicom`:
```sh
minicom -D /dev/ttyACM0
```

