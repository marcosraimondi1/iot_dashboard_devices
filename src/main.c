/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unistd.h"
#include <stdint.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_publisher_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/random/random.h>

#include <errno.h>
#include <string.h>

#include "config.h"

#define APP_BMEM
#define APP_DMEM

/* Buffers for MQTT client. */
static APP_BMEM uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
static APP_BMEM uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];

/* The mqtt client struct */
static APP_BMEM struct mqtt_client client_ctx;

/* MQTT Broker details. */
static APP_BMEM struct sockaddr_storage broker;

static APP_BMEM struct pollfd fds[1];
static APP_BMEM int nfds;

static APP_BMEM bool connected;

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
      LOG_ERR("poll error: %d", errno);
    }
  }

  return ret;
}

void mqtt_evt_handler(struct mqtt_client *const client,
                      const struct mqtt_evt *evt) {
  int err;

  switch (evt->type) {
  case MQTT_EVT_CONNACK:
    if (evt->result != 0) {
      LOG_ERR("MQTT connect failed %d", evt->result);
      break;
    }

    connected = true;
    LOG_INF("MQTT client connected!");

    break;

  case MQTT_EVT_DISCONNECT:
    LOG_INF("MQTT client disconnected %d", evt->result);

    connected = false;
    clear_fds();

    break;

  case MQTT_EVT_PUBACK:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBACK error %d", evt->result);
      break;
    }

    LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

    break;

  case MQTT_EVT_PUBREC:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBREC error %d", evt->result);
      break;
    }

    LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

    const struct mqtt_pubrel_param rel_param = {
        .message_id = evt->param.pubrec.message_id};

    err = mqtt_publish_qos2_release(client, &rel_param);
    if (err != 0) {
      LOG_ERR("Failed to send MQTT PUBREL: %d", err);
    }

    break;

  case MQTT_EVT_PUBCOMP:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBCOMP error %d", evt->result);
      break;
    }

    LOG_INF("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);

    break;

  case MQTT_EVT_PINGRESP:
    LOG_INF("PINGRESP packet");
    break;

  default:
    break;
  }
}

static char *get_mqtt_payload(enum mqtt_qos qos) {
  static APP_DMEM char payload[] = "{ \"value\": 1 }";
  static int flag = 0;
  if (flag) {
    payload[11] = '0';
  } else {
    payload[11] = '1';
  }

  flag = (flag + 1) % 2;

  sleep(1);

  return payload;
}

static char *get_mqtt_topic(void) {
  return "679103d95d9261fdf4d81397/1/goRrYVqZw2/sdata";
}

static int publish(struct mqtt_client *client, enum mqtt_qos qos) {
  struct mqtt_publish_param param;

  param.message.topic.qos = qos;
  param.message.topic.topic.utf8 = (uint8_t *)get_mqtt_topic();
  param.message.topic.topic.size = strlen(param.message.topic.topic.utf8);
  param.message.payload.data = get_mqtt_payload(qos);
  param.message.payload.len = strlen(param.message.payload.data);
  param.message_id = sys_rand16_get();
  param.dup_flag = 0U;
  param.retain_flag = 0U;

  return mqtt_publish(client, &param);
}

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

static void broker_init(void) {
  struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

  broker4->sin_family = AF_INET;
  broker4->sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
  LOG_INF("attempting to connect to server: %s\n", SERVER_ADDR);
}

static struct mqtt_utf8 username = {.utf8 = (uint8_t *)"user",
                                    .size = sizeof("user") - 1};
static struct mqtt_utf8 password = {.utf8 = (uint8_t *)"pass",
                                    .size = sizeof("pass") - 1};

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

#define CONFIG_NET_SAMPLE_APP_MAX_ITERATIONS 10
static int publisher(void) {
  int i, rc, r = 0;

  LOG_INF("attempting to connect: ");
  rc = try_to_connect(&client_ctx);
  PRINT_RESULT("try_to_connect", rc);
  SUCCESS_OR_EXIT(rc);

  i = 0;
  while (i++ < CONFIG_NET_SAMPLE_APP_MAX_ITERATIONS && connected) {
    r = -1;

    rc = mqtt_ping(&client_ctx);
    PRINT_RESULT("mqtt_ping", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_0_AT_MOST_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_1_AT_LEAST_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_2_EXACTLY_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    r = 0;
  }

  rc = mqtt_disconnect(&client_ctx);
  PRINT_RESULT("mqtt_disconnect", rc);

  LOG_INF("Bye!");

  return r;
}

#define CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS 0

static int start_app(void) {
  int r = 0, i = 0;

  while (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS ||
         i++ < CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
    r = publisher();

    if (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
      k_sleep(K_MSEC(5000));
    }
  }

  return r;
}

int main(void) {
  exit(start_app());
  return 0;
}
