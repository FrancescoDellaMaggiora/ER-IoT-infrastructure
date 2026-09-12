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
/*
 * ER-IOT-INFRASTRUCTURE
 * mqtt-service.c - MQTT connection service 
 *
 * See mqtt-service.h for the module overview.
 *
 * This file contains the connection state machine of Contiki-NG's
 * "mqtt-client" example, extracted so that patient.c only holds
 * application logic. Compared to the original example, the following
 * was REMOVED on purpose:
 *   - all MQTTv5 code paths (we run MQTT 3.1.1);
 *   - the IBM Watson mode and the "org id / quickstart" machinery
 *     (authentication can be re-enabled with MQTT_SERVICE_WITH_AUTH);
 *   - the extension mechanism (unused);
 *   - the RSSI/echo-request diagnostics (measured but never consumed).
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "mqtt.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "os/sys/log.h"

#include "mqtt-service.h"

#include <string.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-service"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* Optional username/password authentication (cleartext!).
 * Off by default: our broker (local Mosquitto) accepts anonymous
 * clients. Enable from project-conf.h if the broker requires it. */
#ifdef MQTT_SERVICE_CONF_WITH_AUTH
#define MQTT_SERVICE_WITH_AUTH MQTT_SERVICE_CONF_WITH_AUTH
#else
#define MQTT_SERVICE_WITH_AUTH 0
#endif

#if MQTT_SERVICE_WITH_AUTH
#ifndef MQTT_SERVICE_CONF_USERNAME
#error "MQTT_SERVICE_WITH_AUTH requires MQTT_SERVICE_CONF_USERNAME"
#endif
#ifndef MQTT_SERVICE_CONF_AUTH_TOKEN
#error "MQTT_SERVICE_WITH_AUTH requires MQTT_SERVICE_CONF_AUTH_TOKEN"
#endif
#endif
/*---------------------------------------------------------------------------*/
/* Status LED: on/blinking during the connection phases, one short blink
 * per publish slot. Overridable from project-conf.h. 
 */
#ifdef MQTT_CLIENT_CONF_STATUS_LED
#define STATUS_LED MQTT_CLIENT_CONF_STATUS_LED
#else
#define STATUS_LED LEDS_GREEN
#endif
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32

/* State machine "idle" tick, used while waiting for something to
 * happen (e.g. to connect or to disconnect) */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)

/* A timeout used when waiting for a network connection */
#define NET_CONNECT_PERIODIC       (CLOCK_SECOND >> 2)

/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)
/* Each time we get a publish slot */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/* When we have no connectivity yet */
#define NO_NET_LED_DURATION        (NET_CONNECT_PERIODIC >> 1)

/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)
/*
 * Number of broker reconnection attempts.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
/*---------------------------------------------------------------------------*/
/* Internal state machine states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
/* MQTT connection: owned entirely by this module. The application
 * never touches it. */
static struct mqtt_connection conn;

/* The application process: the MQTT library needs it for its
 * notifications, and we poll it on disconnections. */
static struct process *app_process;

/* Client id, provided (and kept alive) by the application */
static char *client_id;

//  This variable is used to stop the service in case of a discharge request
static bool service_stopped;

/* Application callbacks (see mqtt-service.h) */
static mqtt_service_connected_cb_t connected_cb;
static mqtt_service_publish_slot_cb_t publish_slot_cb;
static mqtt_service_incoming_cb_t incoming_cb;

/* Timers: the state machine tick / publish scheduler ... */
static struct etimer fsm_timer;
/* ... the "connection is stable" grace period ... */
static struct timer connection_life;
/* ... and the LED feedback one-shot */
static struct ctimer led_timer;

static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
static void state_machine(void);
/*---------------------------------------------------------------------------*/
static void
led_off_callback(void *d)
{
  leds_off(STATUS_LED);
}
/*---------------------------------------------------------------------------*/
/* True when we have a global address and a default route: only then it
 * makes sense to try to open a TCP connection towards the broker. */
static bool
have_connectivity(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
     uip_ds6_defrt_choose() == NULL) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
/*
 * Handles MQTT events coming from the Contiki-NG MQTT library
 * (CONNECTED, DISCONNECTED, PUBLISH, SUBACK, ...).
 *
 * Note: received publishes are forwarded verbatim to the application
 * through the on_incoming callback: no topic parsing happens here,
 * because topics are an application concern.
 */

extern clock_time_t last_RTT;
extern clock_time_t start_RTT;
extern uint16_t seq_nr_alert;

static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  //  In case of a discharge request this function returns immediately
  if(service_stopped) {
    return;
  }

  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_INFO("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR: {
    LOG_INFO("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    /* Wake the application process up so the state machine runs and
     * schedules the reconnection backoff */
    process_poll(app_process);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    struct mqtt_message *msg_ptr = data;

    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      LOG_INFO("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n",
              msg_ptr->topic, msg_ptr->payload_chunk_length);
    }

    if(incoming_cb != NULL) {
      incoming_cb(msg_ptr->topic, strlen(msg_ptr->topic),
                  msg_ptr->payload_chunk, msg_ptr->payload_chunk_length);
    }
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
          LOG_DBG("Application failed to subscribe to topic (ret code %x)\n",
                  suback_event->return_code);
        }
    #endif
  }
  case MQTT_EVENT_UNSUBACK: {
    LOG_DBG("Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    last_RTT = clock_time() - start_RTT;
    LOG_INFO("Publishing complete, sequence_number = %u, RTT = %li, CLOCK_SECONDS = %i.\n",seq_nr_alert, last_RTT, CLOCK_SECOND);
    break;
  }
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, MQTT_SERVICE_BROKER_IP_ADDR, MQTT_SERVICE_BROKER_PORT,
               MQTT_SERVICE_KEEP_ALIVE_SECONDS,
               MQTT_CLEAN_SESSION_ON);

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
/*
 * The MQTT connection state machine.
 *
 * It is the same one of the original example, with one structural
 * difference: the "what do we publish" part has been replaced by the
 * mesure_and_publish application callback, which also RETURNS the delay
 * until the next slot. The rate policy (routine vs alert interval)
 * therefore lives in patient.c, not here.
 */
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    /* If we have just been initialized, register the MQTT connection */
    mqtt_register(&conn, app_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

#if MQTT_SERVICE_WITH_AUTH
    mqtt_set_username_password(&conn, MQTT_SERVICE_CONF_USERNAME,
                               MQTT_SERVICE_CONF_AUTH_TOKEN);
#endif

    /* _register() will set auto_reconnect. We don't want that: the
     * backoff logic below handles reconnections explicitly. */
    conn.auto_reconnect = 0;
    connect_attempt = 1;

    state = STATE_REGISTERED;
    LOG_DBG("Init MQTT version %d\n", MQTT_PROTOCOL_VERSION);
    /* Continue */
  case STATE_REGISTERED:
    if(have_connectivity()) {
      /* Registered and with a global IP + default route. Connect */
      LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
      connect_to_broker();
    } else {
      leds_on(STATUS_LED);
      ctimer_set(&led_timer, NO_NET_LED_DURATION, led_off_callback, NULL);
    }
    etimer_set(&fsm_timer, NET_CONNECT_PERIODIC);
    return;
  case STATE_CONNECTING:
    leds_on(STATUS_LED);
    ctimer_set(&led_timer, CONNECTING_LED_DURATION, led_off_callback, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;
  case STATE_CONNECTED:
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
      /* Connection operational and no packet in flight */

      if(state == STATE_CONNECTED) {
        /* First slot after (re)connection: let the application
         * (re)subscribe or do any per-connection setup */
        if(connected_cb != NULL) {
          connected_cb();
        }
        state = STATE_PUBLISHING;
      } else {
        clock_time_t next_slot;

        leds_on(STATUS_LED);
        ctimer_set(&led_timer, PUBLISH_LED_ON_DURATION,
                   led_off_callback, NULL);

        /* Hand the slot to the application: it measures, publishes,
         * and tells us when it wants the next slot */
        next_slot = publish_slot_cb();

        etimer_set(&fsm_timer, next_slot);
        /* Return here so we don't end up rescheduling the timer */
        return;
      }

      /* We were in STATE_CONNECTED: schedule the first publish slot
       * as soon as possible */
      etimer_set(&fsm_timer, 0);
      return;
    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or there
       * might simply be some network delay. In both cases, we refuse to
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
      /* Disconnect and backoff: exponential up to a cap */
      clock_time_t interval;

      mqtt_disconnect(&conn);
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt,
              (unsigned long)interval);

      etimer_set(&fsm_timer, interval);

      state = STATE_REGISTERED;
      return;
    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case STATE_ERROR:
  default:
    leds_on(STATUS_LED);
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only
     * things that can bring us out are mqtt_service_recover() (wired to
     * the button in patient.c) or a reboot.
     */
    LOG_ERR("Error/default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&fsm_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/
void
mqtt_service_init(struct process *process, char *id,
                  mqtt_service_connected_cb_t on_connected,
                  mqtt_service_publish_slot_cb_t on_publish_slot,
                  mqtt_service_incoming_cb_t on_incoming)
{
  app_process = process;
  client_id = id;
  connected_cb = on_connected;
  publish_slot_cb = on_publish_slot;
  incoming_cb = on_incoming;

  //  Variable used in case of a discharge request
  service_stopped = false;

  state = STATE_INIT;

  /* Kickstart the state machine as soon as the process starts its loop.
   * NOTE: etimer_set() binds the timer to the CALLING process, which
   * is why this function must be called from the application process. */
  etimer_set(&fsm_timer, 0);
}
/*---------------------------------------------------------------------------*/
bool
mqtt_service_handle_event(process_event_t ev, process_data_t data)
{
  //  In case of a discharge request this function returns immediately
  if(service_stopped) {
    return true;
  }

  /* Our periodic timer, or a poll requested on disconnection */
  if((ev == PROCESS_EVENT_TIMER && data == &fsm_timer) ||
     ev == PROCESS_EVENT_POLL) {
    state_machine();
    return true;
  }

  return false;
}
/*---------------------------------------------------------------------------*/
mqtt_status_t
mqtt_service_publish(char *topic, uint8_t *payload, uint16_t payload_len,
                     mqtt_qos_level_t qos)
{
  return mqtt_publish(&conn, NULL, topic, payload, payload_len,
                      qos, MQTT_RETAIN_OFF);
}
/*---------------------------------------------------------------------------*/
mqtt_status_t
mqtt_service_subscribe(char *topic)
{
  mqtt_status_t status;

  status = mqtt_subscribe(&conn, NULL, topic, MQTT_QOS_LEVEL_0);

  LOG_DBG("Subscribing to '%s'\n", topic);
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    LOG_ERR("Tried to subscribe but command queue was full!\n");
  }

  return status;
}
/*---------------------------------------------------------------------------*/
bool
mqtt_service_ready(void)
{
  return mqtt_ready(&conn) && conn.out_buffer_sent;
}
/*---------------------------------------------------------------------------*/
void
mqtt_service_recover(void)
{
  if(state == STATE_ERROR) {
    LOG_DBG("Manual recovery requested\n");
    connect_attempt = 1;
    state = STATE_REGISTERED;
    state_machine();
  }
}

//  This function is used to handle the discharge resource. Its aim is to stop all MQTT activity.
void mqtt_service_stop() {

    LOG_INFO("Ending MQTT service\n");

    //  Service needs to be stopped
    service_stopped = true;

    //  Stop all timers
    etimer_stop(&fsm_timer);
    ctimer_stop(&led_timer);

    //  Disconnect from the broker
    if(mqtt_ready(&conn)) {
        mqtt_disconnect(&conn);
    }

}

/*---------------------------------------------------------------------------*/
