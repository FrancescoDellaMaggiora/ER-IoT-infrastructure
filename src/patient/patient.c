/*
ER-IOT-INFRASTRUCTURE
Patient node - application logic

  This code is derived from contiki's "mqtt_client" example.

  Its aim is to simulate a patient's clinical data and publish it.

  The topic format is: er/patient/<PATIENT_ID>/vitals.

  PATIENT_ID is learnt at build time:
    make TARGET=<target> PATIENT_ID=<number>

  MQTT CLIENT_ID has this format: patient-<PATIENT_ID> (e.g. patient-1)

  TODOs:
    TODO: Use the correct IPv6 address (project-conf.h)
    TODO: Write a better vitals initialization function (patient.c)

    //  SENSORS CAN'T RECEIVE MESSAGES YET:
    TODO: If needed, modify on_mqtt_incoming to handle received MQTT
          messages or commands (patient.c)
    TODO: If needed, modify construct_sub_topic() to create the correct
          topic for our use-case (patient.c)
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
#include "dev/button-hal.h"
#include "os/sys/log.h"

#include "patient.h"
#include "triage-model.h"
#include "mqtt-service.h"
#include "coap-engine.h"
#include "vitals-buffer.h"
#include "triage-report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "patient"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* Feature flag: subscription support.
 * SENSORS CAN'T RECEIVE MESSAGES YET: set to 1 once the node needs to
 * receive commands, then adapt construct_sub_topic() and
 * on_mqtt_incoming() below. */
#define PATIENT_SUBSCRIBE_ENABLED 0
/*---------------------------------------------------------------------------*/
/* Publish intervals.
 * The rate policy lives HERE (application), not in the MQTT service:
 * on_publish_slot() returns the delay until the next slot. */

//  Publish every 30 seconds
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)

//  ALESSANDRO: I wrote this code
//  In case of alert publish every second
#define ALERT_PUBLISH_INTERVAL      (1 * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topics.
 * Make sure they are large enough to hold the entire respective string
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];

//  ALESSANDRO: added one buffer per topic
static char vitals_topic[BUFFER_SIZE];
static char alert_topic[BUFFER_SIZE];

#if PATIENT_SUBSCRIBE_ENABLED
static char sub_topic[BUFFER_SIZE];
#endif
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT payload buffer.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
//  The sequence number is associated to a measurement cycle, so vital +
//  alert, e.g.:
//    measurement cycle #1 -> vitals #1
//    measurement cycle #2 -> vitals #2, alert #2
//  As of now, this number never gets transmitted anywhere
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code

//  This variable indicates which sensors are attached to the patient using
//  the SENSOR macros (in this case the heart rate, spo2 and temperature are
//  measured)
static uint8_t attached_sensors =
  SENSOR_HEART_RATE | SENSOR_SPO2 | SENSOR_TEMPERATURE |
  SENSOR_PRESSURE_SYSTOLIC | SENSOR_PRESSURE_DIASTOLIC |
  SENSOR_RESPIRATION_RATE;


//  This variabile indicates which alert conditions are currently active
//  using the ALERT macros
static uint8_t active_alerts = 0;

//  Data structure holding current patient vitals
static patient_vitals_t current_vitals;

//  Event triggered during a patient discharge request
process_event_t discharge_event;

//  Discharge resource
extern coap_resource_t res_discharge;

// Input for the AI model
static float model_input[VITALS_WINDOW * VITALS_FEATURES];

/*
 * The triage code currently in force on this node.
 */
static uint8_t current_triage_code = 0;   /* 0 = not known yet */

static long db_patient_id = 0;            /* 0 = bootstrap not done yet */

/*---------------------------------------------------------------------------*/
PROCESS(patient_process, "Patient node");
AUTOSTART_PROCESSES(&patient_process);
/*---------------------------------------------------------------------------*/
/* Topic / client id construction                                            */
/*---------------------------------------------------------------------------*/
//  Published topic construction
static int
construct_pub_topic(void)
{
  //  ALESSANDRO: I modified the original code to publish topics coherent
  //  with our use case

  int len;

  //  Fill vitals_topic
  len = snprintf(vitals_topic, BUFFER_SIZE, "%s/%d/%s", PUB_TOPIC_NAME,
                 PATIENT_ID, PATIENT_VITALS);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Vitals topic is too long (topic_len: %d, max_size: %d)\n",
             len, BUFFER_SIZE);
    return 0;
  }

  //  Fill alert_topic
  len = snprintf(alert_topic, BUFFER_SIZE, "%s/%d/%s", PUB_TOPIC_NAME,
                 PATIENT_ID, PATIENT_ALERT);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Alert topic is too long (topic_len: %d, max_size: %d)\n",
             len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
#if PATIENT_SUBSCRIBE_ENABLED
//  Subscribed topic construction
//  TODO: If needed, modify construct_sub_topic() to create the correct
//  topic for our use-case (patient.c).
//  The placeholder below follows the same naming scheme as the publish
//  topics (a per-patient command channel).
static int
construct_sub_topic(void)
{
  int len = snprintf(sub_topic, BUFFER_SIZE, "%s/%d/cmd", PUB_TOPIC_NAME,
                     PATIENT_ID);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
#endif /* PATIENT_SUBSCRIBE_ENABLED */
/*---------------------------------------------------------------------------*/
//  Construct the MQTT client ID (the broker uses this ID to differentiate
//  between clients)
static int
construct_client_id(void)
{
  //  ALESSANDRO: Original construction was modified and CLIENT_ID is now
  //  "patient-<number>"
  int len = snprintf(client_id, BUFFER_SIZE, "patient-%d", PATIENT_ID);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
/* Vitals simulation                                                         */
/*---------------------------------------------------------------------------*/

//  Initialize patient's vitals to pre-determined values
static void
patient_vitals_init(void)
{
  current_vitals.heart_rate = 72; 
  current_vitals.spo2 = 98;
  current_vitals.temperature = 36.0f;
  current_vitals.pressure_systolic = 115;
  current_vitals.pressure_diastolic = 61;
  current_vitals.respiration_rate = 14;

  active_alerts = 0;
}
/*---------------------------------------------------------------------------*/
/*
  This support function is used to randomly variate vitals.
  To do this, we use the same approch suggested in:

  Real-time prediction of trauma-induced coagulopathy using an inverted transformer
  (trauma-former): a methodological feasibility and simulation study based on the ADEMP
  framework 

  The formula is the following:

  value_next = current + pull_pct * (baseline - value) + noise
  
  Where:
  - current is the current value
  - pull_pct rappresent "how fast" the value will be return to the baseline
  - noise Random noise
*/
static float random_variation(
      float current,
      simulation_parameters_t par)
{
  float deviation = par.baseline - current;
  float pull = deviation * par.pull_pct / 100.0f;
 
  /* Uniform in [-noise_amp, +noise_amp]. Done in floating point: the
   * integer version could not represent noise_amp values below 1
   * (temperature moves by hundredths of a degree per reading). */
  float unit = (float)rand() / (float)RAND_MAX;          /* [0, 1] */
  float noise = (unit * 2.0f - 1.0f) * par.noise_amp;    /* [-amp, +amp] */
 
  return current + pull + noise;
}

/*---------------------------------------------------------------------------*/
//  This function reads sensor data and implements the alert
//  detection logic. Specifically what it does is to raise an alert (by
//  appropriately modifying active_alerts) if the corresponding values are
//  out of scale (MIN and MAX macros are used for this).
//
//  These fixed thresholds are the "Level 1" clinical safety net of the
//  project design: they are deterministic and independent from any other
//  mechanism. The publish rate consequence (1 s instead of 30 s) is
//  applied in on_publish_slot().
static void
patient_measurement_cycle(void)
{
  //  Read vitals
  if(attached_sensors & SENSOR_HEART_RATE) {

    current_vitals.heart_rate = (int16_t)random_variation(current_vitals.heart_rate, SIMULATION_VALUES[0])

    if(current_vitals.heart_rate < MIN_HEART_RATE ||
       current_vitals.heart_rate > MAX_HEART_RATE) {
      active_alerts |= ALERT_HEART_RATE;
    } else {
      active_alerts &= ~ALERT_HEART_RATE;
    }
  }

  if(attached_sensors & SENSOR_SPO2) {

    current_vitals.spo2 = (int8_t)random_variation(current_vitals.spo2, SIMULATION_VALUES[1]);
    if(current_vitals.spo2 > 100.0f) { current_vitals.spo2 = 100.0f; }
    if(current_vitals.spo2 < 0.0f)   { current_vitals.spo2 = 0.0f; }

    if(current_vitals.spo2 < MIN_SPO2 || current_vitals.spo2 > MAX_SPO2) {
      active_alerts |= ALERT_SPO2;
    } else {
      active_alerts &= ~ALERT_SPO2;
    }
  }

  if(attached_sensors & SENSOR_TEMPERATURE) {

    //  Varies slower than other parameters to be more realistic
    current_vitals.temperature = random_variation(current_vitals.temperature, SIMULATION_VALUES[2]);

    if(current_vitals.temperature < MIN_TEMPERATURE ||
       current_vitals.temperature > MAX_TEMPERATURE) {
      active_alerts |= ALERT_TEMPERATURE;
    } else {
      active_alerts &= ~ALERT_TEMPERATURE;
    }
  }

  if(attached_sensors & SENSOR_PRESSURE_SYSTOLIC) {

    current_vitals.pressure_systolic = (int16_t)random_variation(current_vitals.pressure_systolic, SIMULATION_VALUES[3]);

    if(current_vitals.pressure_systolic < MIN_PRESSURE_SYSTOLIC ||
       current_vitals.pressure_systolic > MAX_PRESSURE_SYSTOLIC) {
      active_alerts |= ALERT_PRESSURE_SYSTOLIC;
    } else {
      active_alerts &= ~ALERT_PRESSURE_SYSTOLIC;
    }
  }

  if(attached_sensors & SENSOR_PRESSURE_DIASTOLIC) {

    current_vitals.pressure_diastolic = (int16_t)random_variation(current_vitals.pressure_diastolic, SIMULATION_VALUES[4]);

    if(current_vitals.pressure_diastolic < MIN_PRESSURE_DIASTOLIC ||
       current_vitals.pressure_diastolic > MAX_PRESSURE_DIASTOLIC) {
      active_alerts |= ALERT_PRESSURE_DIASTOLIC;
    } else {
      active_alerts &= ~ALERT_PRESSURE_DIASTOLIC;
    }
  }

  if(attached_sensors & ALERT_RESPIRATION_RATE) {

    current_vitals.respiration_rate = (int16_t)random_variation(current_vitals.respiration_rate, SIMULATION_VALUES[5]);

    if(current_vitals.respiration_rate < MIN_RESPIRATION_RATE ||
       current_vitals.respiration_rate > MAX_RESPIRATION_RATE) {
      active_alerts |= ALERT_RESPIRATION_RATE;
    } else {
      active_alerts &= ~ALERT_RESPIRATION_RATE;
    }
  }

  /* Feed the freshly measured reading to the sliding window first, so
   * the inference below runs on a window that includes it. */
  vitals_buffer_push(&current_vitals);

  if(vitals_buffer_is_full() && vitals_buffer_export(model_input)) {
 
    /* 0 until the window fills up (VITALS_WINDOW measurement cycles
    * after boot), and on inference failure. */
    uint8_t predicted_code = triage_model_predict();
  
    if(predicted_code != 0 && predicted_code != current_triage_code) {
 
      LOG_INFO("Model changed triage code: %u -> %u\n",
             current_triage_code, predicted_code);
 
      current_triage_code = predicted_code;
  
      /* Then tell the Cloud. Returns immediately; the CoAP exchange
      * completes in the background. */
      triage_report_send(db_patient_id, predicted_code);
    } 
  }
}
/*---------------------------------------------------------------------------*/
/* SenML payload construction                                                */
/*---------------------------------------------------------------------------*/
//  ALESSANDRO: Functions to build each topic's payload. The payload will
//  depend on mounted sensors, active alerts, ...


typedef struct {
  char *buf;        /* current write position          */
  int remaining;    /* space left, including the '\0'  */
  bool first;       /* no record written yet -> no ',' */
  bool failed;      /* a snprintf overflowed           */
} senml_builder_t;

/* Internal: append formatted text, tracking overflow */
static void
senml_append(senml_builder_t *b, const char *fmt, ...)
{
  va_list ap;
  int len;

  if(b->failed) {
    return;
  }

  va_start(ap, fmt);
  len = vsnprintf(b->buf, b->remaining, fmt, ap);
  va_end(ap);

  if(len < 0 || len >= b->remaining) {
    b->failed = true;
    return;
  }

  b->buf += len;
  b->remaining -= len;
}

/* Open the SenML pack: '[' */
static void
senml_begin(senml_builder_t *b, char *buffer, int buffer_size)
{
  b->buf = buffer;
  b->remaining = buffer_size;
  b->first = true;
  b->failed = false;

  senml_append(b, "[");
}

/* Append one record with an integer value:
 * {"n":"<name>","u":"<unit>","v":<value>} */
static void
senml_add_int(senml_builder_t *b, const char *name, const char *unit,
              int value)
{
  if(!b->first) {
    //  A comma has to be added first
    senml_append(b, ",");
  }
  senml_append(b, "{\"n\":\"%s\",\"u\":\"%s\",\"v\":%d}", name, unit, value);
  b->first = false;
}

/* Append one record with a float value printed with one decimal.
 * NOTE for the real nRF52840 deployment: newlib-nano's printf does not
 * print floats by default (needs the '-u _printf_float' linker flag, or
 * a conversion to integer tenths). On Cooja this works as-is. */
static void
senml_add_float1(senml_builder_t *b, const char *name, const char *unit,
                 float value)
{
  if(!b->first) {
    //  A comma has to be added first
    senml_append(b, ",");
  }
  senml_append(b, "{\"n\":\"%s\",\"u\":\"%s\",\"v\":%.1f}", name, unit,
               value);
  b->first = false;
}

/* Close the SenML pack: ']'. Returns 1 on success, 0 on overflow. */
static int
senml_end(senml_builder_t *b, const char *caller)
{
  senml_append(b, "]");

  if(b->failed) {
    LOG_ERR("%s: payload buffer too short\n", caller);
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
build_payload_vitals(char *buffer, int buffer_size)
{
  senml_builder_t b;

  senml_begin(&b, buffer, buffer_size);

  //  Sensor attachment checks
  if(attached_sensors & SENSOR_HEART_RATE) {
    senml_add_int(&b, "heart-rate", "beat/min", current_vitals.heart_rate);
  }
  if(attached_sensors & SENSOR_SPO2) {
    senml_add_int(&b, "spo2", "/100", current_vitals.spo2);
  }
  if(attached_sensors & SENSOR_TEMPERATURE) {
    senml_add_float1(&b, "temperature", "Cel", current_vitals.temperature);
  }
  if(attached_sensors & SENSOR_PRESSURE_SYSTOLIC) {
    senml_add_int(&b, "sys-pressure", "mmHg",
                  current_vitals.pressure_systolic);
  }
  if(attached_sensors & SENSOR_PRESSURE_DIASTOLIC) {
    senml_add_int(&b, "dia-pressure", "mmHg",
                  current_vitals.pressure_diastolic);
  }
  if(attached_sensors & SENSOR_RESPIRATION_RATE) {
    senml_add_int(&b, "respiration_rate", "respiration/min",
                  current_vitals.respiration_rate);
  }

  return senml_end(&b, "build_payload_vitals()");
}
/*---------------------------------------------------------------------------*/
static int
build_payload_alert(char *buffer, int buffer_size)
{
  senml_builder_t b;

  senml_begin(&b, buffer, buffer_size);

  //  Current alert checks
  if(active_alerts & ALERT_HEART_RATE) {
    senml_add_int(&b, "heart-rate-alert", "beat/min",
                  current_vitals.heart_rate);
  }
  if(active_alerts & ALERT_SPO2) {
    senml_add_int(&b, "spo2-alert", "/100", current_vitals.spo2);
  }
  if(active_alerts & ALERT_TEMPERATURE) {
    senml_add_float1(&b, "temperature-alert", "Cel",
                     current_vitals.temperature);
  }
  if(active_alerts & ALERT_PRESSURE_SYSTOLIC) {
    senml_add_int(&b, "sys-pressure-alert", "mmHg",
                  current_vitals.pressure_systolic);
  }
  if(active_alerts & ALERT_PRESSURE_DIASTOLIC) {
    senml_add_int(&b, "dia-pressure-alert", "mmHg",
                  current_vitals.pressure_diastolic);
  }
  if(attached_sensors & ALERT_RESPIRATION_RATE) {
    senml_add_int(&b, "respiration_rate", "respiration/min",
                  current_vitals.respiration_rate);
  }

  return senml_end(&b, "build_payload_alert()");
}
/*---------------------------------------------------------------------------*/
/* Publishing                                                                */
/*---------------------------------------------------------------------------*/
//  ALESSANDRO: I modified this function in order to accept the topic to
//  publish as input (before it was publish(void)). Moreover, it uses custom
//  functions to produce the correct SenML payload.
static void
publish(uint8_t topic_id)
{
  char *topic;
  mqtt_qos_level_t qos;

  //  Payload construction

  //  If topic = "vitals"
  if(topic_id == TOPIC_MSG_VITALS) {

    topic = vitals_topic;
    qos = MQTT_QOS_LEVEL_0;

    if(!build_payload_vitals(app_buffer, APP_BUFFER_SIZE)) {
      LOG_ERR("Vitals payload construction fail\n");
      return;
    }
  }
  //  If topic = "alert"
  else if(topic_id == TOPIC_MSG_ALERT) {

    topic = alert_topic;
    qos = MQTT_QOS_LEVEL_1;

    if(!build_payload_alert(app_buffer, APP_BUFFER_SIZE)) {
      LOG_ERR("Alert payload construction fail\n");
      return;
    }
  } else {
    LOG_ERR("Unknown MQTT topic id: %u\n", topic_id);
    return;
  }

  mqtt_service_publish(topic, (uint8_t *)app_buffer, strlen(app_buffer), qos);

  LOG_DBG("Publish on '%s'!\n", topic);
}
/*---------------------------------------------------------------------------*/
/* MQTT service callbacks                                                    */
/*---------------------------------------------------------------------------*/
/* Called by the service once per (re)connection: the right place to
 * (re)establish subscriptions. */
static void
on_mqtt_connected(void)
{
#if PATIENT_SUBSCRIBE_ENABLED
  mqtt_service_subscribe(sub_topic);
#else
  LOG_DBG("Connected (no subscriptions)\n");
#endif
}
/*---------------------------------------------------------------------------*/
/* Called by the service on every publish slot (connection up and idle).
 * Runs one measurement cycle, publishes, and returns the interval until
 * the next slot: this is where the routine/alert rate policy lives. */
static clock_time_t
on_publish_slot(void)
{
  //  "patient_measurement_cycle()" checks sensor values
  LOG_DBG("Starting measurement cycle\n");
  patient_measurement_cycle();

  seq_nr_value++;

  //  Vitals are always published
  LOG_DBG("Publishing vitals\n");
  publish(TOPIC_MSG_VITALS);

  //  If alerts are detected they get published
  if(active_alerts != 0) {
    LOG_DBG("Alerts detected. Publishing alerts\n");
    publish(TOPIC_MSG_ALERT);
  }

  //vitals_buffer_push(&current_vitals);
  //  In case of alert publish every second instead of every 30 seconds
  return active_alerts ? ALERT_PUBLISH_INTERVAL : DEFAULT_PUBLISH_INTERVAL;
}
/*---------------------------------------------------------------------------*/
//  Broker publish handler (messages received on subscribed topics).
//  TODO: If needed, modify on_mqtt_incoming to handle received MQTT
//  messages or commands (patient.c).
static void
on_mqtt_incoming(const char *topic, uint16_t topic_len,
                 const uint8_t *payload, uint16_t payload_len)
{
  LOG_DBG("Incoming: topic='%s' (len=%u), payload_len=%u\n",
          topic, topic_len, payload_len);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(patient_process, ev, data)
{
  PROCESS_BEGIN();

  printf("Patient node process (PATIENT_ID=%d)\n", PATIENT_ID);
  vitals_buffer_init();
  triage_report_init();


  //  This event is needed by the discharge resource to interrupt all MQTT and COAP communication
  discharge_event = process_alloc_event();

  /* Build our identifiers. A failure here is fatal: identifiers are
   * static strings, if they don't fit the buffers there is nothing we
   * can do at runtime. */
  if(construct_client_id() == 0 || construct_pub_topic() == 0
#if PATIENT_SUBSCRIBE_ENABLED
     || construct_sub_topic() == 0
#endif
     ) {
    LOG_ERR("Fatal: identifier construction failed\n");
    PROCESS_EXIT();
  }

  //  Patient discharge resource
  //  CLOUD -> DEVICE
  coap_activate_resource(&res_discharge, "er/patient/discharge");

  /* Populate patient's vitals with default values */
  patient_vitals_init();

  /* Hand control of the MQTT transport to the service. From here on,
   * this process only reacts to buttons and forwards events. */
  mqtt_service_init(&patient_process, client_id,
                    on_mqtt_connected, on_publish_slot, on_mqtt_incoming);

  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    //  A discharge request triggers this event
    if(ev == discharge_event) {

        LOG_INFO("Patient discharged, stopping communications . . .\n");

        mqtt_service_stop();

        LOG_INFO("Patient node stopped. Waiting for shutdown.\n");

        PROCESS_EXIT();

    }
    
    /* Timers/polls belonging to the MQTT service */
    if(mqtt_service_handle_event(ev, data)) {
      continue;
    }

    

    if(ev == button_hal_release_event &&
       ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO) {
      /* For now, when you press a button you recover the MQTT connecton.
       *
       * TODO: this same button will implement the assistance request
       * once the CoAP side of the node is added; the two
       * behaviours will need to be distinguished (e.g. by press
       * duration or by connection state). */
      mqtt_service_recover();
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
