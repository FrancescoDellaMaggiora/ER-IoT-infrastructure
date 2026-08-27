/*
ER-IOT-INFRASTRUCTURE 
MQTT client

  This code is derived from contiki's "mqtt_client" example.

  Its aim is to simulate a patient's clinical data and publish it.

  The topic format is: er/patient/<PATIENT_ID>/vitals.

  PATIENT_ID is learnt at build time:
    make TARGET=<target> PATIENT_ID=<number>

  MQTT CLIENT_ID has this format: patient-<PATIENT_ID> (e.g. patient-1)
    

  TODOs can be found in the code by pasting them in the search function CTRL+F) inside the specified file:
    TODO: Use the correct IPv6 address (project_conf.h)
    TODO: Write a better vitals initialization function (patient.c)

    //  SENSORS CAN'T RECEIVE MESSAGES YET:
    TODO: If needed, modify pub_handler to handle received MQTT messages or commands (patient.c)
    TODO: If needed, modify construct_sub_topic() to create the correct topic for our use-case (patient.c)

*/


/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/

#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "mqtt-prop.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "patient.h"

#include <string.h>
#include <strings.h>
#include <stdarg.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-client"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* Controls whether the example will work in IBM Watson IoT platform mode */
#ifdef MQTT_CLIENT_CONF_WITH_IBM_WATSON
#define MQTT_CLIENT_WITH_IBM_WATSON MQTT_CLIENT_CONF_WITH_IBM_WATSON
#else
#define MQTT_CLIENT_WITH_IBM_WATSON 0
#endif
/*---------------------------------------------------------------------------*/
/* MQTT broker address. Ignored in Watson mode */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_CLIENT_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_CLIENT_BROKER_IP_ADDR " fd00:1::1"
#endif
/*---------------------------------------------------------------------------*/
/*
 * MQTT Org ID.
 *
 * If it equals "quickstart", the client will connect without authentication.
 * In all other cases, the client will connect with authentication mode.
 *
 * In Watson mode, the username will be "use-token-auth". In non-Watson mode
 * the username will be MQTT_CLIENT_USERNAME.
 *
 * In all cases, the password will be MQTT_CLIENT_AUTH_TOKEN.
 */
#ifdef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_ORG_ID MQTT_CLIENT_CONF_ORG_ID
#else
#define MQTT_CLIENT_ORG_ID "quickstart"
#endif
/*---------------------------------------------------------------------------*/
/* MQTT token */
#ifdef MQTT_CLIENT_CONF_AUTH_TOKEN
#define MQTT_CLIENT_AUTH_TOKEN MQTT_CLIENT_CONF_AUTH_TOKEN
#else
#define MQTT_CLIENT_AUTH_TOKEN "AUTHTOKEN"
#endif
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_IBM_WATSON
/* With IBM Watson support */
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define MQTT_CLIENT_USERNAME "use-token-auth"

#else /* MQTT_CLIENT_WITH_IBM_WATSON */
/* Without IBM Watson support. To be used with other brokers, e.g. Mosquitto */
static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

#ifdef MQTT_CLIENT_CONF_USERNAME
#define MQTT_CLIENT_USERNAME MQTT_CLIENT_CONF_USERNAME
#else
#define MQTT_CLIENT_USERNAME "use-token-auth"
#endif

#endif /* MQTT_CLIENT_WITH_IBM_WATSON */
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_STATUS_LED
#define MQTT_CLIENT_STATUS_LED MQTT_CLIENT_CONF_STATUS_LED
#else
#define MQTT_CLIENT_STATUS_LED LEDS_GREEN
#endif
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_WITH_EXTENSIONS
#define MQTT_CLIENT_WITH_EXTENSIONS MQTT_CLIENT_CONF_WITH_EXTENSIONS
#else
#define MQTT_CLIENT_WITH_EXTENSIONS 0
#endif
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//  PATIENT_ID (default is 0) used to construct the MQTT CLIENT_ID

#ifndef PATIENT_ID
#define PATIENT_ID 0
#endif

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//  These macros are used, together with PATIENT_ID, to construct topic names.
//
//  - PUB_TOPIC_NAME is the topic namespace
//  - PATIENT_ID identifies the patient
//  - The last field is the data type. Many types can be defined, for example:
//
//  PUB_TOPIC_NAME/PATIENT_ID/PATIENT_VITALS = "er/patient/1/vitals"

#define PUB_TOPIC_NAME "er/patient"

#define PATIENT_VITALS "vitals"
#define PATIENT_ALERT "alert"

//  These IDs identify the topics
//  They are used, for example, in the publish(uint8_t topic_id) function to differentiate between topics and to build the correct payload
#define TOPIC_MSG_VITALS 0
#define TOPIC_MSG_ALERT 1

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//
//  These macros are bitmasks used to define what sensors are attached to the patient (using the "attached_sensors" variable).
//
//  TODO: decide what units of measurement to use:
//  The used units are listed as well as they are needed to build the SenML JSON payload.
//  "beat/min" and "Cel" are compliant with RFC 8428, whereas "/100" is a secondary unit belonging to the extension in RFC 8798 (not always valid).
//  Moreover, "mmHg" is not a valid unit of measurement and it may need to be converted to "Pa" (1 mmHg is circa 133.322 Pa)

//      SENSOR_TYPE                          UNIT OF MEASUREMENT (RFC 8428, RFC 8798)
#define SENSOR_HEART_RATE         (1 << 0)  //  beat/min
#define SENSOR_SPO2               (1 << 1)  //  /100
#define SENSOR_TEMPERATURE        (1 << 2)  //  Cel
#define SENSOR_PRESSURE_SYSTOLIC  (1 << 3)  //  mmHg
#define SENSOR_PRESSURE_DIASTOLIC (1 << 4)  //  mmHg

//  These macros are bitmasks used to identify current alert parameters (using the  "active_alerts" variable).

#define ALERT_HEART_RATE          (1 << 0)  
#define ALERT_SPO2                (1 << 1)  
#define ALERT_TEMPERATURE         (1 << 2) 
#define ALERT_PRESSURE_SYSTOLIC   (1 << 3)  
#define ALERT_PRESSURE_DIASTOLIC  (1 << 4)  

//  These macros define thresholds for each measured parameter.  

#define MIN_HEART_RATE          60
#define MAX_HEART_RATE          100

#define MIN_SPO2                95
#define MAX_SPO2                100

#define MIN_TEMPERATURE         36.0f
#define MAX_TEMPERATURE         37.5f

#define MIN_PRESSURE_SYSTOLIC   90
#define MAX_PRESSURE_SYSTOLIC   140

#define MIN_PRESSURE_DIASTOLIC  60
#define MAX_PRESSURE_DIASTOLIC  90
  
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  ALESSANDRO: I wrote this code

/*
 * Data structure declaration to store patient vitals
 */
typedef struct {

  uint16_t heart_rate;
  uint8_t spo2;
  float temperature;
  uint16_t pressure_systolic;
  uint16_t pressure_diastolic;

} patient_vitals_t;

/*---------------------------------------------------------------------------*/



/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

/*---------------------------------------------------------------------------*/
/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */

//  Device type (used to construct CLIENT_ID)
//  ALESSANDRO: This is not true anymore, CLIENT_ID is now "patient-<PATIENT_ID>". DEFAULT_TYPE_ID now goes unused but still added to "conf"
#define DEFAULT_TYPE_ID             "mqtt-client"

//  Event type (used to construct the topic that the client publishes)
#define DEFAULT_EVENT_TYPE_ID       "status"

//  Subscribe (used to construct the topic to which the client subscribes)
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"

//  Unsecure MQTT default TCP port
#define DEFAULT_BROKER_PORT         1883

//  Publish every 30 seconds 
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)

//  ALESSANDRO: I wrote this code
//  In case of alert publish every second
#define ALERT_PUBLISH_INTERVAL      (1 * CLOCK_SECOND)

#define DEFAULT_KEEP_ALIVE_TIMER    60
#define DEFAULT_RSSI_MEAS_INTERVAL  (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
//  No sensor was defined
#define MQTT_CLIENT_SENSOR_NONE     (void *)0xFFFFFFFF
/*---------------------------------------------------------------------------*/
/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN   20
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_client_process);
AUTOSTART_PROCESSES(&mqtt_client_process);
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  char org_id[CONFIG_ORG_ID_LEN];
  char type_id[CONFIG_TYPE_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  int def_rt_ping_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * d:quickstart:status:EUI64 is 32 bytes long
 * iot-2/evt/status/fmt/json is 25 bytes
 * We also need space for the null termination
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
//static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];

//  ALESSANDRO: added one buffer per topic
static char vitals_topic[BUFFER_SIZE];
static char alert_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
//static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_EXTENSIONS
extern const mqtt_client_extension_t *mqtt_client_extensions[];
extern const uint8_t mqtt_client_extension_count;
#else
static const mqtt_client_extension_t *mqtt_client_extensions[] = { NULL };
static const uint8_t mqtt_client_extension_count = 0;
#endif
/*---------------------------------------------------------------------------*/
/* MQTTv5 */
#if MQTT_5
static uint8_t PUB_TOPIC_ALIAS;

struct mqtt_prop_list *publish_props;

/* Control whether or not to perform authentication (MQTTv5) */
#define MQTT_5_AUTH_EN 0
#if MQTT_5_AUTH_EN
struct mqtt_prop_list *auth_props;
#endif
#endif



/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code

//  This variable indicates which sensors are attached to the patient using the SENSOR macros (in this case the heart rate, spo2 and temperature are measured)
static uint8_t attached_sensors = SENSOR_HEART_RATE | SENSOR_SPO2 | SENSOR_TEMPERATURE;

//  This variabile indicates which alert conditions are currently active using the ALERT macros
static uint8_t active_alerts = 0;

//  Data structure holding current patient vitals
static patient_vitals_t current_vitals;

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
PROCESS(mqtt_client_process, "MQTT Client");
/*---------------------------------------------------------------------------*/
static bool
have_connectivity(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
     uip_ds6_defrt_choose() == NULL) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------
static int ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}*/
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
                   uint16_t datalen)
{
  if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
    def_rt_rssi = (int16_t)uipbuf_get_attr(UIPBUF_ATTR_RSSI);
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(MQTT_CLIENT_STATUS_LED);
}
/*---------------------------------------------------------------------------*/
//  Broker publish handler
//  TODO: If needed, modify pub_handler to handle received MQTT messages or commands (patient.c)
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{
  LOG_DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u, chunk='%s'\n", topic,
          topic_len, chunk_len, chunk);

  /* If we don't like the length, ignore */
  if(topic_len != 23 || chunk_len != 1) {
    LOG_ERR("Incorrect topic or chunk len. Ignored\n");
    return;
  }

  /* If the format != json, ignore */
  if(strncmp(&topic[topic_len - 4], "json", 4) != 0) {
    LOG_ERR("Incorrect format\n");
  }

  if(strncmp(&topic[10], "leds", 4) == 0) {
    LOG_DBG("Received MQTT SUB\n");
    if(chunk[0] == '1') {
      leds_on(LEDS_RED);
    } else if(chunk[0] == '0') {
      leds_off(LEDS_RED);
    }
    return;
  }
}
/*---------------------------------------------------------------------------*/
//  Handles MQTT events (CONNECTED, DISCONNECTED, PUBLISH, ...)
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_DBG("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR: {
    LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&mqtt_client_process);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      LOG_DBG("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n", msg_ptr->topic, msg_ptr->payload_chunk_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic),
                msg_ptr->payload_chunk, msg_ptr->payload_chunk_length);
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
    break;
  }
  case MQTT_EVENT_SUBACK: {
#if MQTT_31
    LOG_DBG("Application is subscribed to topic successfully\n");
#else
    struct mqtt_suback_event *suback_event = (struct mqtt_suback_event *)data;

    if(suback_event->success) {
      LOG_DBG("Application is subscribed to topic successfully\n");
    } else {
      LOG_DBG("Application failed to subscribe to topic (ret code %x)\n", suback_event->return_code);
    }
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
#endif
    break;
  }
  case MQTT_EVENT_UNSUBACK: {
    LOG_DBG("Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    LOG_DBG("Publishing complete.\n");
    break;
  }
#if MQTT_5_AUTH_EN
  case MQTT_EVENT_AUTH: {
    LOG_DBG("Continuing auth.\n");
    struct mqtt_prop_auth_event *auth_event = (struct mqtt_prop_auth_event *)data;
    break;
  }
#endif
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
//  Published topic construction
static int
construct_pub_topic(void)
{
  //  ALESSANDRO: I modified the original code to publish topics coherent with our use case

  int len;

  //  Fill vitals_topic
  len = snprintf(vitals_topic, BUFFER_SIZE, "%s/%d/%s", PUB_TOPIC_NAME, PATIENT_ID, PATIENT_VITALS);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Vitals topic is too long (topic_len: %d, max_size: %d)\n", len, BUFFER_SIZE);
    return 0;
  }

  //  Fill alert_topic
  len = snprintf(alert_topic, BUFFER_SIZE, "%s/%d/%s", PUB_TOPIC_NAME, PATIENT_ID, PATIENT_ALERT);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Alert topic is too long (topic_len: %d, max_size: %d)\n", len, BUFFER_SIZE);
    return 0;
  }

#if MQTT_5
  PUB_TOPIC_ALIAS = 1;
#endif

  return 1;
}
/*---------------------------------------------------------------------------*/
//  Subscribed topic construction
//  TODO: If needed, modify construct_sub_topic() to create the correct topic for our use-case (patient.c)
static int
construct_sub_topic(void)
{
  int len = snprintf(sub_topic, BUFFER_SIZE, "iot-2/cmd/%s/fmt/json",
                     conf.cmd_type);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
//  Construct the MQTT client ID (the broker uses this ID to differentiate between clients)
static int
construct_client_id(void)
{
  //  ALESSANDRO: Original construction was modified and CLIENT_ID is now "patient-<number>"
  int len = snprintf(client_id, BUFFER_SIZE, "patient-%d",
                     PATIENT_ID);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_config(void)
{
  if(construct_client_id() == 0) {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_sub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_pub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value = 0;

  state = STATE_INIT;

  /*
   * Schedule next timer event ASAP
   *
   * If we entered an error state then we won't do anything when it fires.
   *
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

#if MQTT_5
  LIST_STRUCT_INIT(&(conn.will), properties);

  mqtt_props_init();
#endif

  return;
}
/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//  Initialize patient's vitals to pre-determined values (this is temporary)
//  TODO: Write a better vitals initialization function (patient.c)
static void
patient_vitals_init(void)
{
  current_vitals.heart_rate = 70;
  current_vitals.spo2 = 98;
  current_vitals.temperature = 36.5f;
  current_vitals.pressure_systolic = 120;
  current_vitals.pressure_diastolic = 80;

  active_alerts = 0;
}

static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
  memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN,
         strlen(MQTT_CLIENT_AUTH_TOKEN));
  memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
         strlen(DEFAULT_EVENT_TYPE_ID));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

  //  ALESSANDRO: I wrote this code
  /* Populate patient's vitals with default values */
  patient_vitals_init();

  return 1;
}
/*---------------------------------------------------------------------------*
static void subscribe(void)
{
  // Publish MQTT topic in IBM quickstart format 
  mqtt_status_t status;

#if MQTT_5
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0,
                          MQTT_NL_OFF, MQTT_RAP_OFF, MQTT_RET_H_SEND_ALL,
                          MQTT_PROP_LIST_NONE);
#else
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);
#endif

  LOG_DBG("Subscribing!\n");
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    LOG_ERR("Tried to subscribe but command queue was full!\n");
  }
}


*/
/*---------------------------------------------------------------------------*/
//  ALESSANDRO: this support function is use to randomly variate vitals.

static int
random_variation(void)
{
  return (rand() % 5) - 2;
}

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  ALESSANDRO: Functions to build each topic's payload. The payload will depend on mounted sensors, active alerts, ...

static int
build_payload_vitals(char *buffer, int buffer_size) {

  int len = 0;
  int remaining = buffer_size;

  //  Used to handle commas
  bool first = true;

  len = snprintf(buffer, remaining, "[");

  if(len < 0 || len >= remaining) {
    LOG_ERR("build_payload_vitals(): Buffer too short to begin. Have %d, need %d + \\0\n", remaining, len);
    return 0;
  }

  remaining -= len;
  buffer += len;

  //  Sensor attachment checks

  if(attached_sensors & SENSOR_HEART_RATE) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_vitals(): Buffer too short for heart rate data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"heart-rate\",\"u\":\"beat/min\",\"v\":%d}", current_vitals.heart_rate);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_vitals(): Buffer too short for heart rate data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;
  }

  if(attached_sensors & SENSOR_SPO2) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_vitals(): Buffer too short for spo2 data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"spo2\",\"u\":\"/100\",\"v\":%d}", current_vitals.spo2);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_vitals(): Buffer too short for spo2 data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;
  }

  if(attached_sensors & SENSOR_TEMPERATURE) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_vitals(): Buffer too short for temperature data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }
    
    len = snprintf(buffer, remaining, "{\"n\":\"temperature\",\"u\":\"Cel\",\"v\":%.1f}", current_vitals.temperature);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_vitals(): Buffer too short for temperature data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;
  }

  if(attached_sensors & SENSOR_PRESSURE_SYSTOLIC) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_vitals(): Buffer too short for systolic pressure data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }
    
    len = snprintf(buffer, remaining, "{\"n\":\"sys-pressure\",\"u\":\"mmHg\",\"v\":%d}", current_vitals.pressure_systolic);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_vitals(): Buffer too short for systolic pressure data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;
  }

  if(attached_sensors & SENSOR_PRESSURE_DIASTOLIC) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_vitals(): Buffer too short for diastolic pressure data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }
    
    len = snprintf(buffer, remaining, "{\"n\":\"dia-pressure\",\"u\":\"mmHg\",\"v\":%d}", current_vitals.pressure_diastolic);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_vitals(): Buffer too short for diastolic pressure data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;
  }

  len = snprintf(buffer, remaining, "]");

  if(len < 0 || len >= remaining) {
    LOG_ERR("build_payload_vitals(): Buffer too short to end. Have %d, need %d + \\0\n", remaining, len);
    return 0;
  }

  return 1;
}

static int 
build_payload_alert(char *buffer, int buffer_size) {

  int len = 0;
  int remaining = buffer_size;

  //  Used to handle commas
  bool first = true;

  len = snprintf(buffer, remaining, "[");

  if(len < 0 || len >= remaining) {
    LOG_ERR("build_payload_alert(): Buffer too short to begin. Have %d, need %d + \\0\n", remaining, len);
    return 0;
  }

  remaining -= len;
  buffer += len;

  //  Current alert checks
  if(active_alerts & ALERT_HEART_RATE) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_alert(): Buffer too short for heart rate data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"heart-rate-alert\",\"u\":\"beat/min\",\"v\":%d}", current_vitals.heart_rate);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_alert(): Buffer too short for heart rate data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;

  }

  if(active_alerts & ALERT_SPO2) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_alert(): Buffer too short for spo2 data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"spo2-alert\",\"u\":\"/100\",\"v\":%d}", current_vitals.spo2);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_alert(): Buffer too short for spo2 data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;

  }

  if(active_alerts & ALERT_TEMPERATURE) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_alert(): Buffer too short for temperature data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"temperature-alert\",\"u\":\"Cel\",\"v\":%.1f}", current_vitals.temperature);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_alert(): Buffer too short for temperature data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;

  }

  if(active_alerts & ALERT_PRESSURE_SYSTOLIC) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_alert(): Buffer too short for systolic pressure data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"sys-pressure-alert\",\"u\":\"mmHg\",\"v\":%d}", current_vitals.pressure_systolic);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_alert(): Buffer too short for systolic pressure data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;

  }

  if(active_alerts & ALERT_PRESSURE_DIASTOLIC) {

    if(!first) {
      //  A comma has to be added first
      len = snprintf(buffer, remaining, ",");

      if(len < 0 || len >= remaining) {
        LOG_ERR("build_payload_alert(): Buffer too short for diastolic pressure data. Have %d, need %d + \\0\n", remaining, len);
        return 0;
      }

      remaining -= len;
      buffer += len;
    }

    len = snprintf(buffer, remaining, "{\"n\":\"dia-pressure-alert\",\"u\":\"mmHg\",\"v\":%d}", current_vitals.pressure_diastolic);

    if(len < 0 || len >= remaining) {
      LOG_ERR("build_payload_alert(): Buffer too short for diastolic pressure data. Have %d, need %d + \\0\n", remaining, len);
      return 0;
    }

    remaining -= len;
    buffer += len;
    first = false;

  }

  len = snprintf(buffer, remaining, "]");

  if(len < 0 || len >= remaining) {
    LOG_ERR("build_payload_alert(): Buffer too short to end. Have %d, need %d + \\0\n", remaining, len);
    return 0;
  }

  return 1;

}

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  ALESSANDRO: Note that mqtt_publish uses MQTT_QOS_LEVEL_0 which sends at most 1 time, no reliability. Modify to make it reliable.
//  
//  ALESSANDRO: I modified this function in order to accept the topic to publish as input (before it was publish(void)).
//              Moreover, it now uses custom functions to produce the correct SenML payload.
static void
publish(uint8_t topic_id)
{
  /* Publish MQTT topic in IBM quickstart format */

#if MQTT_5
  static uint8_t prop_err = 1;
#endif

  //const char *topic;
  char *topic;

  //  Payload construction

  //  If topic = "vitals"
  if(topic_id == TOPIC_MSG_VITALS) {

    topic = vitals_topic;

    if(!build_payload_vitals(app_buffer, APP_BUFFER_SIZE)) {
      LOG_ERR("Vitals payload construction fail\n");
      return;
    }

  }

  //  If topic = "alert"
  else if(topic_id == TOPIC_MSG_ALERT) {
    
    topic = alert_topic;

    if(!build_payload_alert(app_buffer, APP_BUFFER_SIZE)) {
      LOG_ERR("Alert payload construction fail\n");
      return;
    }

  }

  else {
    LOG_ERR("Unknown MQTT topic id: %u\n", topic_id);
    return;
  }

//  ALESSANDRO: this optimization makes it so a topic gets sent only once and then gets replaced by a number (smaller message). This doesn't work anymore as it is becuase 
//              "vitals" and "alert" would become the same alias. However, we're not using MQTT_5 so this shouldn't be an issue, all MQTT_5 gets ignored.
#if MQTT_5
  /* Only send full topic name with the first PUBLISH
   * Afterwards, only use topic alias
   */
  if(seq_nr_value == 1) {
    mqtt_publish(&conn, NULL, topic, (uint8_t *)app_buffer,
                 strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF,
                 PUB_TOPIC_ALIAS, MQTT_TOPIC_ALIAS_OFF,
                 publish_props);

    prop_err = mqtt_prop_register(&publish_props,
                                  NULL,
                                  MQTT_FHDR_MSG_TYPE_PUBLISH,
                                  MQTT_VHDR_PROP_TOPIC_ALIAS,
                                  PUB_TOPIC_ALIAS);
  } else {
    mqtt_publish(&conn, NULL, topic, (uint8_t *)app_buffer,
                 strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF,
                 PUB_TOPIC_ALIAS, (mqtt_topic_alias_en_t) !prop_err,
                 publish_props);
  }
#else
  mqtt_publish(&conn, NULL, topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
#endif

  LOG_DBG("Publish!\n");
}



/*---------------------------------------------------------------------------*/
//  ALESSANDRO: This function reads sensor data and implements the MQTT control logic.
//  Specifically what it does is to raise an alert (by appropiately modifying active_alerts) 
//  if the corresponding values are out of scale (MIN and MAX macros are used for this)

static void
patient_measurement_cycle() {

  //  Read vitals
  if(attached_sensors & SENSOR_HEART_RATE) {
    
    current_vitals.heart_rate += random_variation();

    if(current_vitals.heart_rate < MIN_HEART_RATE || current_vitals.heart_rate > MAX_HEART_RATE) {
      active_alerts |= ALERT_HEART_RATE;
    }
    else {
      active_alerts &= ~ALERT_HEART_RATE;
    }

  }

  if(attached_sensors & SENSOR_SPO2) {

    current_vitals.spo2 += random_variation();

    if(current_vitals.spo2 < MIN_SPO2 || current_vitals.spo2 > MAX_SPO2) {
      active_alerts |= ALERT_SPO2;
    }
    else {
      active_alerts &= ~ALERT_SPO2;
    }

  }

  if(attached_sensors & SENSOR_TEMPERATURE) {

    //  Varies slower than other parameters to be more realistic
    current_vitals.temperature += random_variation() * 0.1f;

    if(current_vitals.temperature < MIN_TEMPERATURE || current_vitals.temperature > MAX_TEMPERATURE) {
      active_alerts |= ALERT_TEMPERATURE;
    }
    else {
      active_alerts &= ~ALERT_TEMPERATURE;
    }

  }

  if(attached_sensors & SENSOR_PRESSURE_SYSTOLIC) {

    current_vitals.pressure_systolic += random_variation();

    if(current_vitals.pressure_systolic < MIN_PRESSURE_SYSTOLIC || current_vitals.pressure_systolic > MAX_PRESSURE_SYSTOLIC) {
      active_alerts |= ALERT_PRESSURE_SYSTOLIC;
    }
    else {
      active_alerts &= ~ALERT_PRESSURE_SYSTOLIC;
    }

  }

  if(attached_sensors & SENSOR_PRESSURE_DIASTOLIC) {

    current_vitals.pressure_diastolic += random_variation();

    if(current_vitals.pressure_diastolic < MIN_PRESSURE_DIASTOLIC || current_vitals.pressure_diastolic > MAX_PRESSURE_DIASTOLIC) {
      active_alerts |= ALERT_PRESSURE_DIASTOLIC;
    }
    else {
      active_alerts &= ~ALERT_PRESSURE_DIASTOLIC;
    }
  }

  //  In case of alert publish every second instead of every 30 seconds
  if(active_alerts)
      conf.pub_interval = ALERT_PUBLISH_INTERVAL;
  else
      conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;

}


/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               (conf.pub_interval * 3) / CLOCK_SECOND,
#if MQTT_5
               MQTT_CLEAN_SESSION_ON,
               MQTT_PROP_LIST_NONE);
#else
               MQTT_CLEAN_SESSION_ON);
#endif

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
#if MQTT_5_AUTH_EN
static void
send_auth(struct mqtt_prop_auth_event *auth_info, mqtt_auth_type_t auth_type)
{
  mqtt_prop_clear_prop_list(&auth_props);

  if(auth_info->auth_method.length) {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_METHOD,
                             auth_info->auth_method.string);
  }

  if(auth_info->auth_data.len) {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_DATA,
                             auth_info->auth_data.data,
                             auth_info->auth_data.len);
  }

  /* Connect to MQTT server */
  mqtt_auth(&conn, auth_type, auth_props);

  if(state != STATE_CONNECTING) {
    LOG_DBG("MQTT reauthenticating\n");
  }
}
#endif
/*---------------------------------------------------------------------------*/
static void
ping_parent(void)
{
  if(have_connectivity()) {
    uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
                   ECHO_REQ_PAYLOAD_LEN);
  } else {
    LOG_WARN("ping_parent() is called while we don't have connectivity\n");
  }
}
/*---------------------------------------------------------------------------*/
//  MQTT FSM (STATE_INIT, STATE_REGISTERED, STATE_CONNECTING, STATE_CONNECTED, STATE_PUBLISHING, STATE_DISCONNECTED, STATE_NEWCONFIG, STATE_CONFIG_ERROR, STATE_ERROR)
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    /*
     * If we are not using the quickstart service (thus we are an IBM
     * registered device), we need to provide user name and password
     */
    if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) != 0) {
      if(strlen(conf.auth_token) == 0) {
        LOG_ERR("User name set, but empty auth token\n");
        state = STATE_ERROR;
        break;
      } else {
        mqtt_set_username_password(&conn, MQTT_CLIENT_USERNAME,
                                   conf.auth_token);
      }
    }

    /* _register() will set auto_reconnect. We don't want that. */
    conn.auto_reconnect = 0;
    connect_attempt = 1;

#if MQTT_5
    mqtt_prop_create_list(&publish_props);

    /* this will be sent with every publish packet */
    (void)mqtt_prop_register(&publish_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_PUBLISH,
                             MQTT_VHDR_PROP_USER_PROP,
                             "Contiki", "v4.5+");

    mqtt_prop_print_list(publish_props, MQTT_VHDR_PROP_ANY);
#endif

    state = STATE_REGISTERED;
    LOG_DBG("Init MQTT version %d\n", MQTT_PROTOCOL_VERSION);
    /* Continue */
  case STATE_REGISTERED:
    if(have_connectivity()) {
      /* Registered and with a public IP. Connect */
      LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
      ping_parent();
      connect_to_broker();
    } else {
      leds_on(MQTT_CLIENT_STATUS_LED);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;
  case STATE_CONNECTING:
    leds_on(MQTT_CLIENT_STATUS_LED);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;
  case STATE_CONNECTED:
    /* Don't subscribe unless we are a registered device */
    if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) == 0) {
      LOG_DBG("Using 'quickstart': Skipping subscribe\n");
      state = STATE_PUBLISHING;
    }
    /* Continue */
  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      /* Connected. Publish */
      if(state == STATE_CONNECTED) {

        //  TODO: the next line of code will need to be uncommented if sensors will receive messages
        //subscribe();

        state = STATE_PUBLISHING;
      } else {
        leds_on(MQTT_CLIENT_STATUS_LED);
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);

        //  ALESSANDRO: I wrote this code
        //  "patient_measurement_cycle()" checks sensor values
        LOG_DBG("Starting measurement cycle\n");
        patient_measurement_cycle();

        //  The sequence number is associated to a measurement cycle, so vital + alert, e.g.:
        //  measurement cycle #1 -> vitals #1
        //  measurement cycle #2 -> vitals #2, alert #2
        //  As of now, this number never gets transmitted anywhere
        seq_nr_value++;

        //  Vitals are always published
        LOG_DBG("Publishing vitals\n");
        publish(TOPIC_MSG_VITALS);

        //  If alerts are detected they get published
        if(active_alerts != 0) {
          LOG_DBG("Alerts detected. Publishing alerts\n");
          publish(TOPIC_MSG_ALERT);
        } 

      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);
      /* Return here so we don't end up rescheduling the timer */
      return;
    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
    }
    break;
  case STATE_DISCONNECTED:
    LOG_DBG("Disconnected\n");
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      /* Disconnect and backoff */
      clock_time_t interval;
#if MQTT_5
      mqtt_disconnect(&conn, MQTT_PROP_LIST_NONE);
#else
      mqtt_disconnect(&conn);
#endif
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt,
              (unsigned long)interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    LOG_ERR("Bad configuration.\n");
    return;
  case STATE_ERROR:
  default:
    leds_on(MQTT_CLIENT_STATUS_LED);
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only thing
     * that can bring us out is a new config event
     */
    LOG_ERR("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
static void
init_extensions(void)
{
  int i;

  for(i = 0; i < mqtt_client_extension_count; i++) {
    if(mqtt_client_extensions[i]->init) {
      mqtt_client_extensions[i]->init();
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{

  PROCESS_BEGIN();

  printf("MQTT Client Process\n");

  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  init_extensions();

  update_config();

  def_rt_rssi = 0x8000000;
  uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
                                    echo_reply_handler);
  etimer_set(&echo_request_timer, conf.def_rt_ping_interval);

  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    if(ev == button_hal_release_event &&
       ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO) {
      if(state == STATE_ERROR) {
        connect_attempt = 1;
        state = STATE_REGISTERED;
      }
    }

    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       ev == PROCESS_EVENT_POLL ||
       (ev == button_hal_release_event &&
        ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO)) {
      state_machine();
    }

    if(ev == PROCESS_EVENT_TIMER && data == &echo_request_timer) {
      ping_parent();
      etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
