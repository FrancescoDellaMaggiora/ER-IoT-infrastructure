/*
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * mqtt-service.h - MQTT connection service
 *
 * This module owns EVERYTHING related to the MQTT transport:
 *   - registration to the Contiki-NG MQTT library
 *   - the connection state machine (connect / reconnect with backoff /
 *     error handling)
 *   - network connectivity checks (global address + default route)
 *   - status LED feedback
 *
 * The application (patient.c) does NOT talk to the mqtt library directly:
 * it only provides its client id and three callbacks, and publishes
 * through mqtt_service_publish().
 *
 * Derived from Contiki-NG's "mqtt-client" example. we use plain MQTT
 * 3.1.1 against a local Mosquitto broker: none of that machinery
 * is needed.
 */
/*---------------------------------------------------------------------------*/
#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "mqtt.h"

#include <stdbool.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
/* MQTT broker address: kept overridable from project-conf.h. */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_SERVICE_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_SERVICE_BROKER_IP_ADDR "fd00:1::1"
#endif

/* Unsecure MQTT default TCP port */
#define MQTT_SERVICE_BROKER_PORT 1883

/* Keep-alive (seconds). The Contiki-NG MQTT library sends PINGREQ by
 * itself, so this does NOT need to track the publish interval: it only
 * has to be long enough not to flood the network with pings. */
#define MQTT_SERVICE_KEEP_ALIVE_SECONDS 60
/*---------------------------------------------------------------------------*/
/*
 * Application callbacks.
 *
 * on_connected: called ONCE every time a connection (or reconnection)
 *   becomes operational. The typical use is subscribing to topics.
 *
 * on_publish_slot: called every time the service is connected, idle
 *   and the publish timer expired.
 *   The application performs its measurement cycle and it publishes
 *   here, then RETURNS THE INTERVAL until the next slot. This is how
 *   the app - not the service - decides the publish rate (e.g. the
 *   1 s alert rate vs the 30 s routine rate).
 *
 * on_incoming: called for every message received on a subscribed
 *   topic. May be NULL if the node does not subscribe to anything.
 */
typedef void (*mqtt_service_connected_cb_t)(void);
typedef clock_time_t (*mqtt_service_publish_slot_cb_t)(void);
typedef void (*mqtt_service_incoming_cb_t)(const char *topic,
                                           uint16_t topic_len,
                                           const uint8_t *payload,
                                           uint16_t payload_len);
/*---------------------------------------------------------------------------*/
/**
 * Initialize the service and kickstart the connection state machine.
 *
 * Must be called from the application process (PROCESS_BEGIN section):
 * the internal timers are bound to the calling process, and 'app_process'
 * is also handed to the MQTT library for its notifications.
 *
 * 'client_id' is NOT copied: the buffer must stay valid forever
 * (a static buffer in the application, as in patient.c).
 */
void mqtt_service_init(struct process *app_process,
                       char *client_id,
                       mqtt_service_connected_cb_t on_connected,
                       mqtt_service_publish_slot_cb_t on_publish_slot,
                       mqtt_service_incoming_cb_t on_incoming);

/**
 * Event dispatcher. Call it from the application process for EVERY
 * event received (PROCESS_YIELD loop). Returns true if the event
 * belonged to the service (timers, polls) and was consumed.
 */
bool mqtt_service_handle_event(process_event_t ev, process_data_t data);

/**
 * Publish 'payload' on 'topic' with the given QoS.
 * Thin wrapper around mqtt_publish(); returns its status code.
 */
mqtt_status_t mqtt_service_publish(char *topic, uint8_t *payload,
                                   uint16_t payload_len,
                                   mqtt_qos_level_t qos);

/**
 * Subscribe to 'topic' (QoS 0). Call it from the on_connected callback
 * so the subscription is re-established after every reconnection.
 */
mqtt_status_t mqtt_service_subscribe(char *topic);

/**
 * True when the connection is up and no packet is in flight,
 * i.e. a publish attempt will not be refused by the library.
 */
bool mqtt_service_ready(void);

/**
 * Manual recovery hook: if the service gave up (error state after too
 * many reconnect attempts), restart the connection attempts.
 */
void mqtt_service_recover(void);

//  This function is used to handle the discharge resource. Its aim is to stop all MQTT activity.
void mqtt_service_stop();
/*---------------------------------------------------------------------------*/
#endif /* MQTT_SERVICE_H_ */
/*---------------------------------------------------------------------------*/
