#!/bin/bash
IFACE=enp3s0 # lan interface
IPV4_ADDR="192.0.2.2/24"
IPV4_ROUTE="192.0.2.0/24"

sudo ip address add $IPV4_ADDR dev $IFACE
sudo ip route add $IPV4_ROUTE dev $IFACE
sudo ip link set dev $IFACE up
