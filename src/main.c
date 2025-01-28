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

#include <zephyr/data/json.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/random/random.h>

static uint8_t http_recv_buf[MAX_RECV_BUF_LEN];

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

/* MQTT Broker details. */
static struct sockaddr_storage broker;

static struct pollfd fds[1];
static int nfds;

static bool connected;

/* GPIO */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define MAX_VARIABLES 5
#define MAX_STRING_LEN 100 // Maximum length for string fields

/* DEVICE DATA */
enum VarType { SENSOR, ACTUATOR };

typedef struct {
  char variable[MAX_STRING_LEN];
  char variableFullName[MAX_STRING_LEN];
  enum VarType variableType;
  int variableSendFreq;
} Variable;

typedef struct {
  char username[MAX_STRING_LEN];
  char password[MAX_STRING_LEN];
  char topic[MAX_STRING_LEN];
  Variable variables[MAX_VARIABLES];
  int variableCount;
} DeviceData;

static DeviceData device_data = {0};

// Helper function to extract a string value
void extractString(const char *json, const char *key, char *outValue) {

  char *start = strstr(json, key);
  if (start) {
    start += strlen(key) + strlen(":\"");
    char *end = strchr(start, '\"');
    if (end) {
      strncpy(outValue, start, end - start);
      outValue[end - start] = '\0';
    }
  }
}

// Helper function to extract an integer value
int extractInt(const char *json, const char *key) {
  char *start = strstr(json, key);
  if (start) {
    start = strchr(start, ':') + 1;
    return atoi(start);
  }
  return 0; // Default value if not found
}

// Function to parse the JSON string
void parseJSON(const char *json, DeviceData *data) {
  extractString(json, "\"username\"", data->username);
  extractString(json, "\"password\"", data->password);
  extractString(json, "\"topic\"", data->topic);

  // Parse variables array
  char *variablesStart = strstr(json, "\"variables\"");
  if (variablesStart) {
    variablesStart =
        strchr(variablesStart, '[') + 1; // Move to the first array element
    char *variablesEnd = strchr(variablesStart, ']');
    char buffer[1024];
    int varIndex = 0;

    char type[100];
    while (variablesStart < variablesEnd && varIndex < MAX_VARIABLES) {
      char *variableEnd = strchr(variablesStart, '}') + 1;
      strncpy(buffer, variablesStart, variableEnd - variablesStart);
      buffer[variableEnd - variablesStart] = '\0';

      // Parse fields in the variable
      extractString(buffer, "\"variable\"", data->variables[varIndex].variable);
      extractString(buffer, "\"variableFullName\"",
                    data->variables[varIndex].variableFullName);
      extractString(buffer, "\"variableType\"", type);

      if (strcmp(type, "input") == 0) {
        data->variables[varIndex].variableType = SENSOR;
      } else {
        data->variables[varIndex].variableType = ACTUATOR;
      }

      data->variables[varIndex].variableSendFreq =
          extractInt(buffer, "\"variableSendFreq\"");

      // Move to the next variable
      variablesStart = variableEnd;
      varIndex++;
    }
    data->variableCount = varIndex;
  }
}

// Function to display the parsed data
void printDeviceData(const DeviceData *data) {
  printf("Username: %s\n", data->username);
  printf("Password: %s\n", data->password);
  printf("Topic: %s\n", data->topic);

  for (int i = 0; i < data->variableCount; i++) {
    printf("  Variable %d:\n", i + 1);
    printf("    Variable: %s\n", data->variables[i].variable);
    printf("    Variable Full Name: %s\n", data->variables[i].variableFullName);

    if (data->variables[i].variableType == SENSOR)
      printf("    Variable Type: SENSOR\n");
    else
      printf("    Variable Type: ACTUATOR\n");

    if (data->variables[i].variableSendFreq > 0) {
      printf("    Variable Send Freq: %d\n",
             data->variables[i].variableSendFreq);
    }
  }
}

// ---------------------------------------------

static int setup_socket(const char *server, int port, int *sock,
                        struct sockaddr *addr, socklen_t addr_len) {
  int ret = 0;

  memset(addr, 0, addr_len);

  net_sin(addr)->sin_family = AF_INET;
  net_sin(addr)->sin_port = htons(port);
  inet_pton(AF_INET, server, &net_sin(addr)->sin_addr);
  *sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (*sock < 0) {
    printf("Failed to create IPv4 HTTP socket (%d)", -errno);
  }

  return ret;
}

void parse_response(char *data, size_t len) {
  printf("Parsing....\n");
  parseJSON(data, &device_data);
  printDeviceData(&device_data);
}

static void response_cb(struct http_response *rsp,
                        enum http_final_call final_data, void *user_data) {
  if (final_data == HTTP_DATA_MORE) {
    printf("Partial data received (%zd bytes)\n", rsp->data_len);
  } else if (final_data == HTTP_DATA_FINAL) {
    printf("All the data received (%zd bytes)\n", rsp->data_len);
  }

  printf("Response status %s\n", rsp->http_status);
  if (rsp->http_status_code == 200) {
    printf("MQTT Credentials obtained!\n");
    parse_response(http_recv_buf, rsp->data_len);
  }
}

static int connect_socket(const char *server, int port, int *sock,
                          struct sockaddr *addr, socklen_t addr_len) {
  int ret;

  ret = setup_socket(server, port, sock, addr, addr_len);
  if (ret < 0 || *sock < 0) {
    return -1;
  }

  ret = connect(*sock, addr, addr_len);
  if (ret < 0) {
    printf("Cannot connect to IPv4 remote (%d)", -errno);
    close(*sock);
    *sock = -1;
    ret = -errno;
  }

  return ret;
}

static int get_credentials(void) {
  struct sockaddr_in addr;
  int sock = -1;
  int32_t timeout = 5000;
  int ret = 0;
  int port = HTTP_PORT;

  connect_socket(SERVER_ADDR, port, &sock, (struct sockaddr *)&addr,
                 sizeof(addr));

  if (sock < 0) {
    printf("Cannot create HTTP connection.");
    return -ECONNABORTED;
  }

  struct http_request req;
  const char *headers[] = {HEADERS, NULL};

  memset(&req, 0, sizeof(req));

  req.method = HTTP_POST;
  req.url = SERVER_ENDPOINT;
  req.host = SERVER_ADDR;
  req.protocol = "HTTP/1.1";
  req.payload = REQ_PAYLOAD;
  req.payload_len = strlen(req.payload);
  req.response = response_cb;
  req.recv_buf = http_recv_buf;
  req.recv_buf_len = sizeof(http_recv_buf);
  req.header_fields = headers;

  ret = http_client_req(sock, &req, timeout, "IPv4 POST");

  close(sock);
  return ret;
}
// ---------------------------------------------
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
  static struct mqtt_utf8 username;
  static struct mqtt_utf8 password;

  mqtt_client_init(client);

  broker_init();

  username.utf8 = device_data.username;
  username.size = strlen(username.utf8);
  password.utf8 = device_data.password;
  password.size = strlen(password.utf8);

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

void subscribe() {
  struct mqtt_subscription_list list;
  struct mqtt_topic topics[1];
  const int ntopics = 1;
  char topic[100];
  strcpy(topic, device_data.topic);
  strcat(topic, "+/actdata");

  topics[0].qos = MQTT_QOS_0_AT_MOST_ONCE;
  topics[0].topic.utf8 = topic;
  topics[0].topic.size = strlen(topic);

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

  // http
  k_sleep(K_MSEC(5000));
  printf("Getting credentials ....\n");
  get_credentials();

  k_sleep(K_MSEC(5000));

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
