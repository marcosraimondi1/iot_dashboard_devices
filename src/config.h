/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define ZEPHYR_ADDR		CONFIG_NET_CONFIG_MY_IPV4_ADDR
#define SERVER_ADDR		CONFIG_NET_CONFIG_PEER_IPV4_ADDR

#define SERVER_PORT		1883

#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		500

#define APP_CONNECT_TRIES	10

#define APP_MQTT_BUFFER_SIZE	128

#define MQTT_CLIENTID		"frdm_k64f_zephyr"

#define MAX_RECV_BUF_LEN 1024

#define HTTP_PORT 3001

#define SERVER_ENDPOINT "/api/getdevicecredentials"

#define HEADERS "Content-type: application/x-www-form-urlencoded\r\n"

#define REQ_PAYLOAD "dId=1&password=pass"

#endif
