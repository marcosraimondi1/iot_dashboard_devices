# IOT DASHBOARD DEVICES
This repository holds necessary firmware for running IoT devices connecting to the IoT dashboard running on a server:
- [IoT Dashboard Web Application](https://github.com/marcosraimondi1/iot_dashboard)
- [IoT Dashboard Services](https://github.com/marcosraimondi1/iot_dashboard_services) 

This particular example implements a **Zephyr RTOS** application with the **FRDM K64F** board. The firmware should be aplicable for other boards with a network interface and Zephyr support.
For non Zephyr applications, similar steps should be taken.

## Hardware
- [FRDM K64F](https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/frdm-development-platform-for-kinetis-k64-k63-and-k24-mcus:FRDM-K64F)


## Configuration and more
Before the device can connect succesfully, both the backend API and the dashboard services need to be running.
1. First the device connects to the network, IP address of the board is configurable through `src/config.h`.
2. Once the board is successfully connected, it will try to get the device MQTT credentials for connecting to the broker. The device makes a POST request to the server API `/api/getdevicecredentials`, in order to do this you need to cofigure correctly the *REQ_PAYLOAD* macro in `src/config.h` with the correct device ID and password, which you can obtain from the IoT Dashboard.
3. Finally the device will connect to the MQTT broker with the correct credentials and start publishing and subscribing to its allowed topics (also retrieved from the post request).
> [!NOTE]  
> You can simulate POST requests and broker connections using the commands provided in [`cmds.sh`](./cmds.sh) file.

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
5. If connecting the ethernet to your PC which runs the project locally, run the following commands (`config_linux.sh`) to setup the interface IP address:
```sh
IFACE=enp3s0 # eth interface where the board is connected
IPV4_ADDR="192.0.2.2/24"
IPV4_ROUTE="192.0.2.0/24"

sudo ip address add $IPV4_ADDR dev $IFACE
sudo ip route add $IPV4_ROUTE dev $IFACE
sudo ip link set dev $IFACE up
```



