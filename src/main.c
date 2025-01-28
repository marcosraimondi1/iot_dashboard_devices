/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/random/random.h>

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")
#define PRINT_RESULT(func, rc) printf("%s: %d <%s>\n", (func), rc, RC_STR(rc))
#define SUCCESS_OR_EXIT(rc)                                                    \
  {                                                                            \
    if (rc != 0) {                                                             \
      return 1;                                                                \
    }                                                                          \
  }
#define SUCCESS_OR_BREAK(rc)                                                   \
  {                                                                            \
    if (rc != 0) {                                                             \
      break;                                                                   \
    }                                                                          \
  }

/* Buffers for MQTT client. */
static uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];

/* The mqtt client struct */
static struct mqtt_client client_ctx;
static struct mqtt_utf8 username = {.utf8 = (uint8_t *)MQTT_USERNAME,
                                    .size = sizeof(MQTT_USERNAME) - 1};
static struct mqtt_utf8 password = {.utf8 = (uint8_t *)MQTT_PASSWORD,
                                    .size = sizeof(MQTT_PASSWORD) - 1};

/* MQTT Broker details. */
static struct sockaddr_storage broker;

static struct pollfd fds[1];
static int nfds;

static bool connected;

/* GPIO */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void prepare_fds(struct mqtt_client *client) {
  if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
    fds[0].fd = client->transport.tcp.sock;
  }

  fds[0].events = POLLIN;
  nfds = 1;
}

static void clear_fds(void) { nfds = 0; }

static int wait(int timeout) {
  int ret = 0;

  if (nfds > 0) {
    ret = poll(fds, nfds, timeout);
    if (ret < 0) {
      printf("poll error: %d\n", errno);
    }
  }

  return ret;
}

void process_message(struct mqtt_publish_message msg) {
  static uint8_t data[APP_MQTT_BUFFER_SIZE];
  mqtt_read_publish_payload(&client_ctx, data, msg.payload.len);
  printf("Received: %s from %s\n", data, msg.topic.topic.utf8);
}

void mqtt_evt_handler(struct mqtt_client *const client,
                      const struct mqtt_evt *evt) {
  int err;

  switch (evt->type) {
  case MQTT_EVT_PUBLISH:
    process_message(evt->param.publish.message);

    break;

  case MQTT_EVT_CONNACK:
    if (evt->result != 0) {
      printf("MQTT connect failed %d\n", evt->result);
      break;
    }

    connected = true;
    printf("MQTT client connected!\n");

    break;

  case MQTT_EVT_DISCONNECT:
    printf("MQTT client disconnected %d\n", evt->result);

    connected = false;
    clear_fds();

    break;

  case MQTT_EVT_PUBACK:
    if (evt->result != 0) {
      printf("MQTT PUBACK error %d\n", evt->result);
      break;
    }

    break;

  case MQTT_EVT_PUBREC:
    if (evt->result != 0) {
      printf("MQTT PUBREC error %d\n", evt->result);
      break;
    }

    const struct mqtt_pubrel_param rel_param = {
        .message_id = evt->param.pubrec.message_id};

    err = mqtt_publish_qos2_release(client, &rel_param);
    if (err != 0) {
      printf("Failed to send MQTT PUBREL: %d\n", err);
    }

    break;

  case MQTT_EVT_PUBCOMP:
    if (evt->result != 0) {
      printf("MQTT PUBCOMP error %d\n", evt->result);
      break;
    }
    break;

  case MQTT_EVT_PINGRESP:
    printf("PINGRESP packet\n");
    break;

  default:
    break;
  }
}

static char *get_mqtt_payload() {
  static char payload[] = "{ \"value\": 1 }";
  static int flag = 0;

  if (flag) {
    gpio_pin_set_dt(&led, 0);
    payload[11] = '0';
  } else {
    gpio_pin_set_dt(&led, 1);
    payload[11] = '1';
  }

  flag = (flag + 1) % 2;

  return payload;
}

static int publish(struct mqtt_client *client, struct mqtt_topic topic,
                   struct mqtt_binstr payload) {
  struct mqtt_publish_param param;

  param.message.topic = topic;
  param.message.payload = payload;
  param.message_id = sys_rand16_get();
  param.dup_flag = 0U;
  param.retain_flag = 0U;

  return mqtt_publish(client, &param);
}

static void broker_init(void) {
  struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

  broker4->sin_family = AF_INET;
  broker4->sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
  printf("attempting to connect to server: %s\n", SERVER_ADDR);
}

static void client_init(struct mqtt_client *client) {
  mqtt_client_init(client);

  broker_init();

  /* MQTT client configuration */
  client->broker = &broker;
  client->evt_cb = mqtt_evt_handler;
  client->client_id.utf8 = (uint8_t *)MQTT_CLIENTID;
  client->client_id.size = strlen(MQTT_CLIENTID);
  client->password = &password;
  client->user_name = &username;
  client->protocol_version = MQTT_VERSION_3_1_1;

  /* MQTT buffers configuration */
  client->rx_buf = rx_buffer;
  client->rx_buf_size = sizeof(rx_buffer);
  client->tx_buf = tx_buffer;
  client->tx_buf_size = sizeof(tx_buffer);

  client->transport.type = MQTT_TRANSPORT_NON_SECURE;
}

/* In this routine we block until the connected variable is 1 */
static int try_to_connect(struct mqtt_client *client) {
  int rc, i = 0;

  while (i++ < APP_CONNECT_TRIES && !connected) {

    client_init(client);

    rc = mqtt_connect(client);
    if (rc != 0) {
      PRINT_RESULT("mqtt_connect", rc);
      k_sleep(K_MSEC(APP_SLEEP_MSECS));
      continue;
    }

    prepare_fds(client);

    if (wait(APP_CONNECT_TIMEOUT_MS)) {
      mqtt_input(client);
    }

    if (!connected) {
      mqtt_abort(client);
    }
  }

  if (connected) {
    return 0;
  }

  return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout) {
  int64_t remaining = timeout;
  int64_t start_time = k_uptime_get();
  int rc;

  while (remaining > 0 && connected) {
    if (wait(remaining)) {
      rc = mqtt_input(client);
      if (rc != 0) {
        PRINT_RESULT("mqtt_input", rc);
        return rc;
      }
    }

    rc = mqtt_live(client);
    if (rc != 0 && rc != -EAGAIN) {
      PRINT_RESULT("mqtt_live", rc);
      return rc;
    } else if (rc == 0) {
      rc = mqtt_input(client);
      if (rc != 0) {
        PRINT_RESULT("mqtt_input", rc);
        return rc;
      }
    }

    remaining = timeout + start_time - k_uptime_get();
  }

  return 0;
}

static struct mqtt_subscription_list list;
static struct mqtt_topic topics[1];
const static int ntopics = 1;

void subscribe() {
  topics[0].qos = MQTT_QOS_0_AT_MOST_ONCE;
  topics[0].topic.utf8 = "679103d95d9261fdf4d81397/1/+/actdata";
  topics[0].topic.size = strlen(topics[0].topic.utf8);

  list.list = topics;
  list.list_count = ntopics;
  list.message_id = sys_rand16_get();

  int r = mqtt_subscribe(&client_ctx, &list);
  if (r < 0)
    printf("mqtt_subscribe failed: %d", r);
  process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
}

void setup() {
  // gpio
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  gpio_pin_set_dt(&led, 0);

  // mqtt
  while (!connected) {
    printf("attempting to connect: \n");
    try_to_connect(&client_ctx);
    k_sleep(K_MSEC(APP_SLEEP_MSECS));
  }

  k_sleep(K_MSEC(5000));
  subscribe();
}

static struct mqtt_topic topic;
static struct mqtt_binstr payload;

void loop() {
  topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
  topic.topic.utf8 = "679103d95d9261fdf4d81397/1/goRrYVqZw2/sdata";
  topic.topic.size = strlen(topic.topic.utf8);
  payload.data = get_mqtt_payload();
  payload.len = strlen(payload.data);

  publish(&client_ctx, topic, payload);
  process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
}

int main(void) {
  setup();
  while (1) {
    loop();
  }
}
