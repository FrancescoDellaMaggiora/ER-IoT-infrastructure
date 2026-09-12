/*
ER-IOT-INFRASTRUCTURE
Patient node - application logic

  This code is derived from contiki's "mqtt_client" example.

  Its aim is to simulate a patient's clinical data and publish it.

  MQTT topics are: 
  /er/patient/<PATIENT_ID>/registration
  /er/patient/<PATIENT_ID>/vitals
  /er/patient/<PATIENT_ID>/alert

  The device publishes on the registration topic everytime it connects (in function on_mqtt_connected()).
  The payload published on the registration topic is the following JSON file (it only writes the attached sensors):

    {
      "DEVICE_ID": <number>,
      "PATIENT_ID": <number>,
      "SENSORS": [
        "heart-rate",
        "spo2",
        "temperature",
        "sys-pressure",
        "dia-pressure",
        "respiration-rate"
      ]
    }

  PATIENT_ID is learnt at build time:
    make TARGET=<target> PATIENT_ID=<number>

  MQTT CLIENT_ID has this format: patient-<PATIENT_ID> (e.g. patient-1)
*/

#include "contiki.h"
#include "contiki-net.h"
#include "dev/button-hal.h"
#include "os/sys/log.h"

#include "patient.h"
#include "triage_model.h"
#include "adaptive.h"
#include "mqtt-service.h"
#include "coap-engine.h"
#include "vitals-buffer.h"
#include "triage-report.h"
#include "senml.h"

#include "coap-blocking-api.h"
#include "coap-log.h"
#include "sys/node-id.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

/*---------------------------------------------------------------------------*/
#define LOG_MODULE "patient"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif

/*---------------------------------------------------------------------------*/
/* Compile-time configuration                                                */
/*---------------------------------------------------------------------------*/
/* Feature flag: subscription support. */
#define PATIENT_SUBSCRIBE_ENABLED 0

/* 
 * Vitals publish intervals.
 * The rate policy lives HERE (application), not in the MQTT service:
 * on_publish_slot() returns the delay until the next slot.
 * Publish every 30 seconds
 */
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)

/*
 * Alerts publish interval
 * Publish alerts every second
 */
#define ALERT_PUBLISH_INTERVAL      (1 * CLOCK_SECOND)

/* 
 * Checked on its own timer, not from on_publish_slot(): that slot only
 * fires when MQTT is idle and ready, so it becomes rarer exactly when
 * the gateway is failing, the detector would starve precisely when it
 * is needed.
 */
#define GATEWAY_CHECK_INTERVAL (5 * CLOCK_SECOND)

/* 
 * While draining the backlog, come back quickly instead of waiting the
 * full routine interval: the MQTT out queue holds one message at a time,
 * so the buffer can only be emptied one entry per slot.
 */
#define RETENTION_FLUSH_INTERVAL (2 * CLOCK_SECOND)

/*
 * URI to ask for nurse assistance
 */
#define ASSISTANCE_URI "/er/patient/assistance"

#define OBS_STATUS_URI "/er/patient/assistance/status"

/*---------------------------------------------------------------------------*/
/* Global state                                                              */
/*---------------------------------------------------------------------------*/
/*
 * Buffer for the registration uri
 */
#define URI_SIZE 64
static char REGISTRATION_URI[URI_SIZE];

/*
 * Buffers for Client ID and Topics.
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];

static char vitals_topic[BUFFER_SIZE];
static char alert_topic[BUFFER_SIZE];
static char registration_topic[BUFFER_SIZE];

#if PATIENT_SUBSCRIBE_ENABLED
static char sub_topic[BUFFER_SIZE];
#endif

/*
 * Age of the reading being published, in seconds.
 *
 * Zero for a live reading; set by publish_buffered() to the age of a
 * replayed one. The payload builders read it the same way they read
 * current_vitals, a global rather than a parameter, because publish()
 * sits between them and threading it through would mean changing every
 * signature for a value that is zero in the normal case.
 */
static clock_time_t publish_taken_at = 0;

/*
 * The MQTT payload buffers.
 * We will need to increase if we start publishing more data.
 * We need separate buffers because otherwise one buffer would be completed overrided.
 */
#define APP_BUFFER_SIZE 512
static char vitals_app_buffer[APP_BUFFER_SIZE];
static char alert_app_buffer[APP_BUFFER_SIZE];
static char registration_app_buffer[APP_BUFFER_SIZE];

/*
 * The measurement cycle runs on its own timer, independent of MQTT.
 */
static struct etimer measure_timer;

static struct etimer gateway_timer;
static struct etimer et;
static int flagRegistration = 0;

/*
 * Contiki-NG's MQTT out queue holds ONE message at a time: a second
 * mqtt_publish() in the same slot is refused with
 * MQTT_STATUS_OUT_QUEUE_FULL, regardless of payload size (a 46 B alert
 * was refused just like a 180 B one). So the alert is deferred to the
 * next slot rather than sent back-to-back with the vitals.
 */
static bool alert_pending = false;

/*  
 * Sequence numbers
 */
uint16_t seq_nr_value = 0;
uint16_t seq_nr_alert = 0;

/*
 *  This variable indicates which sensors are attached to the patient using
 *  the SENSOR macros (in this case the all the vitals are measured)
 */
static uint8_t attached_sensors =
  SENSOR_HEART_RATE | SENSOR_SPO2 | SENSOR_TEMPERATURE |
  SENSOR_PRESSURE_SYSTOLIC | SENSOR_PRESSURE_DIASTOLIC |
  SENSOR_RESPIRATION_RATE;


/*  
 *  This variabile indicates which alert conditions are currently active
 *  using the ALERT macros
*/
static uint8_t active_alerts = 0;

/*
 *  Data structure holding current patient vitals
 */
static patient_vitals_t current_vitals;

/*
 *  Event triggered during a patient discharge request
 */
process_event_t discharge_event;

/*
 * Event triggered during an assistance request
 */
static process_event_t stop_observation_event;

/*
 * Device and patient information
 */
device_data_t patient_info;

/*
 *  Discharge resource
 */
extern coap_resource_t res_discharge;
extern coap_resource_t res_triage;
static coap_observee_t *request_status;

/*
 * Cloud Application CoAP endpoint
 */
static coap_endpoint_t server_addr;

/*
 * Nurse CoAP endpoint
 */
static coap_endpoint_t nurse_addr;

/*
 * Variable to measure RTT
 */
clock_time_t start_RTT;

/*
 * Variable to save last RTT
 */
clock_time_t last_RTT = 0;

/*
 * Simulation Parameters
 * 
 * If the macro URGENT is set to 1, vitals quickly converge to red code zone.
 * Otherwhise, whenever the marco URGENT is set to 0 vitals quickly converge to a white code.
 */

#if URGENT
const simulation_parameters_t SIMULATION_VALUES[SIM_PARAM_COUNT] = {
  /* baseline, noise_amp, pull_pct */
  {  72.0f,      3.3f,      1.5f },   /* SIM_HR   */
  {  98.0f,      4.5f,      2.5f },   /* SIM_SPO2 */
  {  36.0f,      4.3f,      2.0f },   /* SIM_TEMP */
  { 115.0f,      6.9f,      2.0f },   /* SIM_SBP  */
  {  61.0f,      6.3f,      2.0f },   /* SIM_DBP  */
  {  14.0f,      2.5f,      2.0f },   /* SIM_RR   */
};
# else
const simulation_parameters_t SIMULATION_VALUES[SIM_PARAM_COUNT] = {
  /* baseline, noise_amp, pull_pct */
  {  72.0f,      3.3f,      15.0f },   /* SIM_HR   */
  {  98.0f,      0.5f,      25.0f },   /* SIM_SPO2 */
  {  36.0f,      0.3f,      20.0f },   /* SIM_TEMP */
  { 115.0f,      0.9f,      20.0f },   /* SIM_SBP  */
  {  61.0f,      0.3f,      20.0f },   /* SIM_DBP  */
  {  14.0f,      0.5f,      20.0f },   /* SIM_RR   */
};
#endif

/*
 * Simulation state kept in floating point.
 *
 * The published vitals are integers, but the AR(1) process MUST NOT be
 * iterated on them: casting to int truncates toward zero, so positive
 * noise is discarded while negative noise is amplified. That is a
 * systematic -0.5/step drift, not noise, the mean reversion then
 * settles at whatever deviation makes the pull cancel it.
 * Keeping the state as float and rounding
 * only for publication removes the bias entirely.
 */
static float sim_hr, sim_spo2, sim_sbp, sim_dbp, sim_rr;

 /*
  * Confirmation counter: a classifier sitting near a decision boundary
  * flips between adjacent codes.
  * Each flip costs a CoAP PUT to the cloud plus one from
  * the cloud to the nurse, so a code is committed only after the model
  * has predicted it consistently.
  */
static uint8_t candidate_code = 0;
static uint8_t candidate_streak = 0;

/*---------------------------------------------------------------------------*/
PROCESS(patient_process, "Patient node");
AUTOSTART_PROCESSES(&patient_process);

/*---------------------------------------------------------------------------*/
/* Forward declarations                                                      */
/*---------------------------------------------------------------------------*/
static bool publish(uint8_t topic_id);

/*---------------------------------------------------------------------------*/
/* Topic / client id construction                                            */
/*---------------------------------------------------------------------------*/
//  Published topic construction
static int construct_pub_topic(void)
{

  int len;

  //  Fill vitals_topic
  len = snprintf(vitals_topic, BUFFER_SIZE, "%s/%ld/%s", PUB_TOPIC_NAME,
                 patient_info.patient_id, PATIENT_VITALS);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Vitals topic is too long (topic_len: %d, max_size: %d)\n",
             len, BUFFER_SIZE);
    return 0;
  }

  //  Fill alert_topic
  len = snprintf(alert_topic, BUFFER_SIZE, "%s/%ld/%s", PUB_TOPIC_NAME,
                 patient_info.patient_id, PATIENT_ALERT);

  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Alert topic is too long (topic_len: %d, max_size: %d)\n",
             len, BUFFER_SIZE);
    return 0;
  }

  //  Fill registration topic
  len = snprintf(registration_topic, BUFFER_SIZE, "%s/%ld/%s", PUB_TOPIC_NAME,
               patient_info.patient_id, PATIENT_REGISTRATION);

  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Registration topic is too long\n");
    return 0;
  }

  return 1;
}

/*---------------------------------------------------------------------------*/
#if PATIENT_SUBSCRIBE_ENABLED

  /*
  * Subscribed topic construction
  * The placeholder below follows the same naming scheme as the publish
  * topics (a per-patient command channel).
  */
  static int construct_sub_topic(void) {

    int len = snprintf(sub_topic, BUFFER_SIZE, "%s/%d/cmd", PUB_TOPIC_NAME,
                      patient_info.patient_id);

    /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
    if(len < 0 || len >= BUFFER_SIZE) {
      LOG_INFO("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
      return 0;
    }

    return 1;
  }
#endif /* PATIENT_SUBSCRIBE_ENABLED */

/*---------------------------------------------------------------------------*/

/*
 *  Construct the MQTT client ID (the broker uses this ID to differentiate
 *  between clients)
 */
static int  construct_client_id(void) {
  
  int len = snprintf(client_id, BUFFER_SIZE, "patient-%li", patient_info.patient_id);

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

/*
 *  Initialize patient's vitals to pre-determined values
 */
static void patient_vitals_init(void)
{

  sim_hr   = 72.0f;
  sim_spo2 = 98.0f;
  sim_sbp  = 115.0f;
  sim_dbp  = 61.0f;
  sim_rr   = 14.0f;

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

  << Real-time prediction of trauma-induced coagulopathy using an inverted transformer
  (trauma-former): a methodological feasibility and simulation study based on the ADEMP
  framework >>

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

/*
 * Advances one simulated vital: iterate in float, clamp to a physically
 * possible range, and round (not truncate) for the published integer.
 * The clamp is a safety net against the uint16_t wraparound that
 * produced values like 65535, not a clinical threshold.
 */
static uint16_t sim_advance(float *state, simulation_parameters_t par, float lo, float hi)
{
  *state = random_variation(*state, par);

  if(*state < lo) { *state = lo; }
  if(*state > hi) { *state = hi; }

  return (uint16_t)roundf(*state);
}

/*---------------------------------------------------------------------------*/
/*
 *  This function reads sensor data and implements the alert
 *  detection logic. Specifically what it does is to raise an alert (by
 *  appropriately modifying active_alerts) if the corresponding values are
 *  out of scale (MIN and MAX macros are used for this).
 *
 *  These fixed thresholds are the "Level 1" clinical safety net of the
 *  project design: they are deterministic and independent from any other
 *  mechanism. The publish rate variation (1s instead of 30s) is
 *  applied in on_publish_slot().
*/
static void patient_measurement_cycle(void)
{
  //  Read vitals
  if(attached_sensors & SENSOR_HEART_RATE) {

    current_vitals.heart_rate = sim_advance(&sim_hr, SIMULATION_VALUES[SIM_HR], 20.0f, 250.0f);

    if(current_vitals.heart_rate < MIN_HEART_RATE ||
       current_vitals.heart_rate > MAX_HEART_RATE) {
      active_alerts |= ALERT_HEART_RATE;
    } else {
      active_alerts &= ~ALERT_HEART_RATE;
    }
  }

  if(attached_sensors & SENSOR_SPO2) {

    current_vitals.spo2 = (uint8_t)sim_advance(&sim_spo2, SIMULATION_VALUES[SIM_SPO2], 50.0f, 100.0f);

    if(current_vitals.spo2 < MIN_SPO2 || current_vitals.spo2 > MAX_SPO2) {
      active_alerts |= ALERT_SPO2;
    } else {
      active_alerts &= ~ALERT_SPO2;
    }
  }

  if(attached_sensors & SENSOR_TEMPERATURE) {

    current_vitals.temperature = random_variation(current_vitals.temperature, SIMULATION_VALUES[2]);

    if(current_vitals.temperature < MIN_TEMPERATURE ||
       current_vitals.temperature > MAX_TEMPERATURE) {
      active_alerts |= ALERT_TEMPERATURE;
    } else {
      active_alerts &= ~ALERT_TEMPERATURE;
    }
  }

  if(attached_sensors & SENSOR_PRESSURE_SYSTOLIC) {

    current_vitals.pressure_systolic = sim_advance(&sim_sbp, SIMULATION_VALUES[SIM_SBP], 40.0f, 250.0f);

    if(current_vitals.pressure_systolic < MIN_PRESSURE_SYSTOLIC ||
       current_vitals.pressure_systolic > MAX_PRESSURE_SYSTOLIC) {
      active_alerts |= ALERT_PRESSURE_SYSTOLIC;
    } else {
      active_alerts &= ~ALERT_PRESSURE_SYSTOLIC;
    }
  }

  if(attached_sensors & SENSOR_PRESSURE_DIASTOLIC) {

    current_vitals.pressure_diastolic = sim_advance(&sim_dbp, SIMULATION_VALUES[SIM_DBP], 20.0f, 150.0f);

    if(current_vitals.pressure_diastolic < MIN_PRESSURE_DIASTOLIC ||
       current_vitals.pressure_diastolic > MAX_PRESSURE_DIASTOLIC) {
      active_alerts |= ALERT_PRESSURE_DIASTOLIC;
    } else {
      active_alerts &= ~ALERT_PRESSURE_DIASTOLIC;
    }
  }

  if(attached_sensors & SENSOR_RESPIRATION_RATE) {

    current_vitals.respiration_rate = sim_advance(&sim_rr, SIMULATION_VALUES[SIM_RR], 4.0f, 60.0f);

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

  /*
   * 0 until the window fills up (VITALS_WINDOW measurement cycles
   * after boot), and on inference failure.
   */
  uint8_t predicted_code = triage_model_predict();

  if(predicted_code == 0) {
    // no prediction yet
  } else if ( predicted_code != patient_info.triage_code) {
    
    if (predicted_code == candidate_code) {
      candidate_streak++;
    } else {
      candidate_code = predicted_code;
      candidate_streak = 1;
    }

    if(candidate_streak >= 3) {

      LOG_INFO("Model changed triage code: %u -> %u\n",
              patient_info.triage_code, candidate_code);

      patient_info.triage_code = candidate_code;
      triage_report_send(patient_info.patient_id, candidate_code);
    }
  } 
}


/*---------------------------------------------------------------------------*/
/* Payload construction                                                      */
/*---------------------------------------------------------------------------*/
//  Functions to build each topic's payload. The payload will 
//  depend on mounted sensors, active alerts, ...

/*---------------------------------------------------------------------------*/
static int build_payload_vitals(char *buffer, int buffer_size)
{
  senml_builder_t b;

  senml_begin(&b, buffer, buffer_size);
  senml_add_age(&b, publish_taken_at);

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
static int build_payload_alert(char *buffer, int buffer_size)
{
  senml_builder_t b;

  senml_begin(&b, buffer, buffer_size);
  senml_add_age(&b, publish_taken_at);

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
  if(active_alerts & ALERT_RESPIRATION_RATE) {
    senml_add_int(&b, "respiration-rate-alert", "respiration/min",
                  current_vitals.respiration_rate);
  }

  return senml_end(&b, "build_payload_alert()");
}

static int build_payload_registration(char *buffer, int buffer_size)
{
  int len;
  int first_sensor = 1;

  len = snprintf(buffer, buffer_size,
                 "{"
                 "\"DEVICE_ID\":%d,"
                 "\"SENSORS\":[",
                 DEVICE_ID);

  if(len < 0 || len >= buffer_size) {
    return 0;
  }

  #define ADD_SENSOR(flag, name)                                      \
    do {                                                              \
      if(attached_sensors & (flag)) {                                 \
        int n = snprintf(buffer + len, buffer_size - len,             \
                        "%s\"%s\"",                                   \
                        first_sensor ? "" : ",", name);               \
        if(n < 0 || n >= buffer_size - len) {                         \
          return 0;                                                   \
        }                                                             \
        len += n;                                                     \
        first_sensor = 0;                                             \
      }                                                               \
    } while(0)

    ADD_SENSOR(SENSOR_HEART_RATE,          "heart-rate");
    ADD_SENSOR(SENSOR_SPO2,                "spo2");
    ADD_SENSOR(SENSOR_TEMPERATURE,         "temperature");
    ADD_SENSOR(SENSOR_PRESSURE_SYSTOLIC,   "sys-pressure");
    ADD_SENSOR(SENSOR_PRESSURE_DIASTOLIC,   "dia-pressure");
    ADD_SENSOR(SENSOR_RESPIRATION_RATE,    "respiration-rate");

  #undef ADD_SENSOR

  {
    int n = snprintf(buffer + len, buffer_size - len, "]}");

    if(n < 0 || n >= buffer_size - len) {
      return 0;
    }

    len += n;
  }

  return len;
}

/*---------------------------------------------------------------------------*/
/* Publishing                                                                */
/*---------------------------------------------------------------------------*/
/* 
 *  This function accepts the topic to publish on as input
 *  (before it was publish(void)). Moreover, it uses 
 *  functions to produce the correct SenML payload.
 */
static bool publish(uint8_t topic_id) {

  char *topic;
  char *buf;
  mqtt_qos_level_t qos;

  //  Payload construction

  if(topic_id == TOPIC_MSG_VITALS) { //  If topic = "vitals"

    topic = vitals_topic;
    buf = vitals_app_buffer;
    qos = MQTT_QOS_LEVEL_0;

    if(!build_payload_vitals(buf, APP_BUFFER_SIZE)) {
      LOG_ERR("Vitals payload construction fail\n");
      return false;
    }
  } else if(topic_id == TOPIC_MSG_ALERT) { //  If topic = "alert"

    start_RTT = clock_time();
    topic = alert_topic;
    qos = MQTT_QOS_LEVEL_1;
    buf = alert_app_buffer;

    if(!build_payload_alert(buf, APP_BUFFER_SIZE)) {
      LOG_ERR("Alert payload construction fail\n");
      return false;
    }
  } else if(topic_id == TOPIC_MSG_REGISTRATION) { //  If topic = "registration"
    topic = registration_topic;
    qos = MQTT_QOS_LEVEL_1;
    buf = registration_app_buffer;

    int len = build_payload_registration(buf, APP_BUFFER_SIZE);
    if(len <= 0) {
      LOG_ERR("Registration payload construction fail\n");
      return false;
    }
  } else {

    LOG_ERR("Unknown MQTT topic id: %u\n", topic_id);
    return false;
  }

  mqtt_status_t status = mqtt_service_publish(topic, (uint8_t *)buf, strlen(buf), qos);

  #ifdef ADAPTIVE_CONGESTION
    congestion_update(status == MQTT_STATUS_OK);
  #endif

  if(status != MQTT_STATUS_OK) {
    LOG_WARN("Publish on '%s' REFUSED, status %d (payload %u B)\n",
            topic, status, (unsigned)strlen(buf));
    return false;
  }

  if (topic_id == TOPIC_MSG_VITALS) {
    LOG_INFO("Published vitals number %u\n", seq_nr_value);
  } else {
    LOG_DBG("Publish on '%s'!\n", topic);
  }
  return true;
}

/*---------------------------------------------------------------------------*/
/* MQTT service callbacks                                                    */
/*---------------------------------------------------------------------------*/

/*
 * Called by the service once per (re)connection: the right place to
 * (re)establish subscriptions.
 */
static void on_mqtt_connected(void)
{
  //  Each time the device connects it publishes on the registration topic
  publish(TOPIC_MSG_REGISTRATION);

  #if PATIENT_SUBSCRIBE_ENABLED
    mqtt_service_subscribe(sub_topic);
  #else
    LOG_DBG("Connected (no subscriptions)\n");
  #endif
}

/*
 * Replays one retained reading.
 *
 * publish() and the payload builders read the globals, so the stored
 * snapshot is swapped in for the duration of the call and restored
 * afterwards. Safe because nothing runs in between: this is a single
 * protothread, not preemptive.
 */
static bool publish_buffered(uint8_t topic_id, const buffered_reading_t *entry)
{
  patient_vitals_t saved_vitals = current_vitals;
  uint8_t saved_alerts = active_alerts;
  clock_time_t saved_taken_at = publish_taken_at;
  bool ok;

  current_vitals = entry->vitals;
  active_alerts = entry->alerts;
  publish_taken_at = entry->taken_at;

  ok = publish(topic_id);

  current_vitals = saved_vitals;
  active_alerts = saved_alerts;
  publish_taken_at = saved_taken_at;

  return ok;
}

/*
 * Runs one measurement cycle and decides what to do with the reading:
 * publish it, retain it (gateway down) or suppress it (congestion).
 *
 * Driven by measure_timer, not by the MQTT service: tying it to the
 * connection being idle meant the node stopped measuring exactly when
 * the gateway failed, a.k.a. the moment retention matters most. Publishing
 * may now be refused if a previous message is still in flight, which is
 * tolerable for routine vitals and handled explicitly when flushing the
 * backlog (the entry is committed only on success).
 *
 */
static clock_time_t measure_and_publish(void) {

  buffered_reading_t entry;

  /* 
   * Gateway check comes first: with a dead gateway nothing can leave the
   * node, so publishing a deferred alert here would just hang on a
   * connection that is gone and the node would never get back to
   * measuring or buffering.
   */
  if(gateway_is_down()) {

    /* 
     * An alert was waiting for its slot when the gateway died: retain it
     * rather than losing it. current_vitals still holds the values that
     * raised it, since no measurement cycle has run since.
     */
    if(alert_pending) {
      retention_store_alert(&current_vitals, active_alerts);
      alert_pending = false;
      LOG_DBG("Gateway down: deferred alert retained\n");
    }

    LOG_DBG("Starting measurement cycle\n");
    patient_measurement_cycle();
    seq_nr_value++; //Incrementing the sequence number now and not on the backlog phase

    retention_store_vitals(&current_vitals, active_alerts);

    if(active_alerts != 0) {
      seq_nr_alert++;
      retention_store_alert(&current_vitals, active_alerts);
    }

    LOG_DBG("Gateway down: buffered (v=%u a=%u, dropped v=%u a=%u)\n",
            retention_vitals_count(), retention_alert_count(),
            retention_vitals_dropped(), retention_alerts_dropped());

    return DEFAULT_PUBLISH_INTERVAL;
  }

  /* From here on the gateway is reachable. */

  if(alert_pending) {
    if(mqtt_service_ready()) {
      LOG_DBG("Publishing deferred alert\n");
      publish(TOPIC_MSG_ALERT);
      alert_pending = false;

      return ALERT_PUBLISH_INTERVAL;
    } else {
      LOG_DBG("MQTT busy, alert stays pending\n");
    }
  }

  /* Backlog before new readings, oldest entry first, ALERTS BEFORE
   * VITALS: a deterioration that happened during the outage must reach
   * the cloud before the routine readings that surround it. */
  if(retention_pending()) {

    if(!mqtt_service_ready()) {
      return RETENTION_FLUSH_INTERVAL;   /* retry next slot */
    }
    
    if(retention_peek_alert(&entry)) {
      if(publish_buffered(TOPIC_MSG_ALERT, &entry)) {
        retention_commit_alert();
        LOG_DBG("Flushed buffered alert (%u left)\n", retention_alert_count());
      }
    } else if(retention_peek_vitals(&entry)) {
      if(publish_buffered(TOPIC_MSG_VITALS, &entry)) {
        retention_commit_vitals();
        LOG_DBG("Flushed buffered vitals (%u left)\n", retention_vitals_count());
      }
    }

    return RETENTION_FLUSH_INTERVAL;
  }

  LOG_DBG("Starting measurement cycle\n");
  patient_measurement_cycle();

  if(should_suppress_vitals()) {
    
    LOG_INFO("Congested, low-priority vitals suppressed\n");
  } else if(!mqtt_service_ready()) {
    
    /* The previous message is still going out. Attempting anyway just
     * gets refused and keeps the queue from draining. Decoupling the
     * cycle from MQTT removed the natural back-pressure the service
     * used to provide, so it has to be checked explicitly. */
    LOG_INFO("MQTT busy, skipping this publish\n");
  } else {
    LOG_INFO("Publishing vitals\n");
    publish(TOPIC_MSG_VITALS);
    seq_nr_value++;
  }

  if(active_alerts != 0) {
    LOG_INFO("Alerts detected, deferring to next slot\n");
    alert_pending = true;
    seq_nr_alert++;
    return ALERT_PUBLISH_INTERVAL;
  }

  return DEFAULT_PUBLISH_INTERVAL;
}

/*
 * MQTT no longer drives the cycle, measure_timer does. This callback
 * exists only to satisfy the service's interface.
 */
static clock_time_t on_publish_slot(void)
{
  return DEFAULT_PUBLISH_INTERVAL;
}
/*---------------------------------------------------------------------------*/

//  Broker publish handler (messages received on subscribed topics).
static void on_mqtt_incoming(const char *topic, uint16_t topic_len,
                 const uint8_t *payload, uint16_t payload_len)
{
  LOG_DBG("Incoming: topic='%s' (len=%u), payload_len=%u\n",
          topic, topic_len, payload_len);
}

/*---------------------------------------------------------------------------*/
/*                     REGISTRATION FUNCTIONS                                */
/*---------------------------------------------------------------------------*/

//  Extract the target patient id, the triage code and the nurse address from the response
int parse_registration(const char *payload, int payload_len, long *patient_id, triage_code_t *triage_code, coap_endpoint_t *nurse_address) {

  const char *id_key = "\"PATIENT_ID\":";
  const char *triage_key = "\"TRIAGE_CODE\":";
  const char *nurse_key = "\"NURSE_ADDRESS\":\"";

  const char *start;
  char *endptr;

  long id;
  long triage;

  if(payload == NULL || patient_id == NULL || triage_code == NULL || nurse_address == NULL) {
    return -1;
  }

  /* PATIENT_ID */

  start = strstr(payload, id_key);

  if(start == NULL) {
    return -1;
  }

  start += strlen(id_key);

  id = strtol(start, &endptr, 10);

  if(endptr == start || id <= 0) {
    return -1;
  }

  /* TRIAGE_CODE */

  start = strstr(payload, triage_key);

  if(start == NULL) {
    return -1;
  }

  start += strlen(triage_key);

  triage = strtol(start, &endptr, 10);

  if(endptr == start || triage < CODE_RED || triage > CODE_WHITE) {
    return -1;
  }

  /* NURSE_ADDRESS */

  start = strstr(payload, nurse_key);

  if(start == NULL) {
    return -1;
  }

  start += strlen(nurse_key);

  const char *end = strchr(start, '"');

  if(end == NULL) {
    return -1;
  }

  /* Temporarily copy the IPv6 address */
  char address[64];

  int address_len = end - start;

  if(address_len <= 0 || address_len >= sizeof(address)) {
    return -1;
  }

  memcpy(address, start, address_len);
  address[address_len] = '\0';

  /* coap_endpoint_parse() expects a URI */
  char uri[64 + 20];

  snprintf(uri, sizeof(uri), "coap://[%s]", address);

  if(coap_endpoint_parse(uri, strlen(uri), nurse_address) < 0) {
    return -1;
  }

  patient_info.patient_id = id;
  patient_info.triage_code = (triage_code_t)triage;

  return 0;

}      

void print_patient_info() {
  LOG_INFO("Patient ID: %li\t Triage code: %i\n", patient_info.patient_id, patient_info.triage_code);
}

/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS
/*---------------------------------------------------------------------------*/

//  Led colors are treated as strings, this funciton converts them to masks. 
  //  The correct mask is platform dependent, this funciton maps the colors to the Nordic nRF52840 Dongle masks
  int color_to_led_mask(char *color) {

      if(strcmp(color, "RED") == 0) {
          return LEDS_RED;
      }
      else if(strcmp(color, "GREEN") == 0) {
          return LEDS_GREEN;
      }
      else if(strcmp(color, "BLUE") == 0) {
          return LEDS_BLUE;
      }
      else if(strcmp(color, "YELLOW") == 0) {
          return LEDS_RED | LEDS_GREEN;
      }
      else if(strcmp(color, "MAGENTA") == 0) {
          return LEDS_RED | LEDS_BLUE;
      }
      else if(strcmp(color, "CYAN") == 0) {
          return LEDS_GREEN | LEDS_BLUE;
      }
      else if(strcmp(color, "WHITE") == 0) {
          return LEDS_RED | LEDS_GREEN | LEDS_BLUE;
      }

      return 0;

  }

  //  Turn on the led with the specified color
  void update_leds(char *color) {

    leds_off(LEDS_ALL);
    leds_on(color_to_led_mask(color));

  }

  //  Extract the color name from the payload received after an assistance request
  int parse_led_color(const char *payload, int payload_len, char *color, int color_size) {
    const char *key = "\"LED_COLOR\":\"";
    const char *start;
    const char *end;
    int len;

    if(payload == NULL || color == NULL || color_size <= 0) {
        return -1;
    }

    //  Find the LED_COLOR key
    start = strstr(payload, key);

    if(start == NULL) {
        return -1;
    }

    //  Move to the beginning of the value
    start += strlen(key);

    //  Look for the end of the value
    end = strchr(start, '"');

    if(end == NULL) {
        return -1;
    }

    len = end - start;

    //  Check is buffer size is sufficient
    if(len >= color_size) {
        return -1;
    }

    //  Copy the name color
    memcpy(color, start, len);
    color[len] = '\0';

    return 0;

  }

//  Extract the target patient id and the request status from the observable resource notification
  int parse_status(const char *payload, int payload_len, long *patient_id, patient_status_t *status) {

    const char *id_key = "\"PATIENT_ID\":";
    const char *status_key = "\"STATUS\":";
    const char *start;

    if(payload == NULL || patient_id == NULL || status == NULL) {
        return -1;
    }

    start = strstr(payload, id_key);
    if(start == NULL) {
        return -1;
    }

    start += strlen(id_key);
    *patient_id = atol(start);

    start = strstr(payload, status_key);
    if(start == NULL) {
        return -1;
    }

    start += strlen(status_key);
    *status = (patient_status_t)atoi(start);

    return 0;

}

 void stop_observation(void) {

    if(request_status) {
      LOG_INFO("Stopping observation\n");
      request_status = NULL;
    } 
  }

//  Assistance request status (observable) handling
  /*
    The client observes the /er/patient/assistance/status resource.

    As soon as the status of a request changes (i.e. the nurse aknowledges it), a notification is sent to all the clients observing 
    this resource (i.e. the clients that have a pending request).

    The notification's payload has the following format:
    {
      "PATIENT_ID": <id>,
      "STATUS":     <status>
    }
  */
  static void notification_callback(coap_observee_t *obs, void *notification, coap_notification_flag_t flag) {

    int len = 0;
    const uint8_t *payload = NULL;

    LOG_DBG("Notification handler\n");

    if(obs != NULL) {
        LOG_DBG("Observee URI: %s\n", obs->url);
    }

    if(notification) {
      len = coap_get_payload(notification, &payload);
    }

    long patient_id;
    patient_status_t status;
    char payload_buf[64];
    switch(flag) {

      case OBSERVE_OK: 
        LOG_INFO("Observation registered\n");
        break;

      case NOTIFICATION_OK: 

        if(payload == NULL || len <= 0 || len >= sizeof(payload_buf)) {
            LOG_INFO("Invalid notification payload\n");
            break;
        }

        memcpy(payload_buf, payload, len);
        payload_buf[len] = '\0';

        if(parse_status(payload_buf, len, &patient_id, &status) != 0) {
            LOG_INFO("Invalid notification payload\n");
            break;
        }

        LOG_INFO("Notification: Patient ID = %ld\t Status=0x%02X\n", patient_id, status);

        //  Process the notification only if it refers to this patient
        if(patient_id == patient_info.patient_id && status == STATUS_IDLE) {

            LOG_INFO("Request acknowledged\n");

            leds_off(LEDS_ALL);

            //  It's better to modify the observer outside of the notification function
            //stop_observation();
            process_post(&patient_process, stop_observation_event, NULL);
        }

        break;

      case OBSERVE_NOT_SUPPORTED:
        LOG_INFO("Observe not supported\n");
        request_status = NULL;
        break;

      case ERROR_RESPONSE_CODE:
        LOG_INFO("ERROR_RESPONSE_CODE: %*s\n", len, (char *)payload);
        request_status = NULL;
        break;

      case NO_REPLY_FROM_SERVER:
        if(obs != NULL) {
          LOG_INFO("NO_REPLY_FROM_SERVER: "
                "removing observe registration with token %x%x\n",
                obs->token[0], obs->token[1]);
        }
        request_status = NULL;
        break;

    }

  }

//  Start/stop the observation of the remote resource
  void start_observation(void) {

    if(request_status == NULL) {
      LOG_INFO("Starting observation\n"); 
      request_status = coap_obs_request_registration(&nurse_addr, OBS_STATUS_URI, notification_callback, NULL);
    }

    if(request_status == NULL) {
      LOG_DBG("ERROR: observation registration failed\n");
    }

  }

/*
 *  This function will be passed to COAP_BLOCKING_REQUEST() to handle responses
 */
void client_chunk_handler(coap_message_t *response) {

  const uint8_t *chunk;
  uint8_t status;
  int len;

  if(response == NULL) {
    puts("Request timed out, will retry");
    etimer_reset(&et);
    return;
  }

  status = response->code;
  LOG_DBG("Response: %u.%02u\n", status / 32, status % 32);

  len = coap_get_payload(response, &chunk);

  char payload[128];

  if(len <= 0 || len >= sizeof(payload)) {
    LOG_DBG("Invalid payload\n");
    return;
  }

  memcpy(payload, chunk, len);
  payload[len] = '\0';

  if(parse_registration(payload, len, &patient_info.patient_id, &patient_info.triage_code, &nurse_addr) == 0) {
    LOG_INFO("Device successfully registered\n");
    print_patient_info();
    flagRegistration = 1;

  } else {
    LOG_INFO("Patient info parsing error, will retry\n");
    etimer_reset(&et);
  }
}

//  This function will be passed to COAP_BLOCKING_REQUEST() to handle responses from the nurse
  void nurse_request_callback(coap_message_t *response) {

    const uint8_t *chunk;
    uint8_t status;
    int len;

    if(response == NULL) {
      puts("Request timed out");
      return;
    }

    status = response->code;
    LOG_DBG("Response: %u.%02u\n", status / 32, status % 32);

    len = coap_get_payload(response, &chunk);

    char payload[64];
    char color[10];

    if(len <= 0 || len >= sizeof(payload)) {
      LOG_DBG("Invalid payload\n");
      return;
    }

    memcpy(payload, chunk, len);
    payload[len] = '\0';

    if(parse_led_color(payload, len, color, sizeof(color)) == 0) {
      LOG_DBG("LED color: %s\n", color);
      update_leds(color);
      start_observation();
    } 
    
    else 
      LOG_DBG("LED_COLOR parsing error\n");
    
  }

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(patient_process, ev, data)
{

  PROCESS_BEGIN();
  

  LOG_INFO("Patient node process (DEVICE_ID=%d)\n", DEVICE_ID);
  vitals_buffer_init();
  triage_report_init();

  //  The device tries to register after some time it's on
  etimer_set(&et, 10 * CLOCK_SECOND);

  //  Set the resource URI /er/patient/registration/<DEVICE_ID>
  snprintf(REGISTRATION_URI, sizeof(REGISTRATION_URI), "/er/patient/registration/%i", DEVICE_ID);

  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  static coap_message_t request_for_nurse[1]; 

  coap_endpoint_parse(SERVER_ADDR, strlen(SERVER_ADDR), &server_addr);

  memset(&patient_info, 0, sizeof(patient_info));

  while(flagRegistration == 0) {

    PROCESS_YIELD();
        
    if(etimer_expired(&et)) {

      LOG_INFO("--Timer expired--\n");

      /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
      coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
      coap_set_header_uri_path(request, REGISTRATION_URI);

      LOG_INFO_COAP_EP(&server_addr);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_addr, request, client_chunk_handler);

      LOG_INFO("\n--Registration request sent--\n");
    }
  }
  // This event is needed to stop observing assistance requests status
  stop_observation_event = process_alloc_event(); 

  // This event is needed by the discharge resource to interrupt all MQTT and COAP communication
  discharge_event = process_alloc_event();

  /* Build our identifiers. A failure here is fatal: identifiers are
   * static strings, if they don't fit the buffers there is nothing we
   * can do at runtime. */
  if(
    construct_client_id() == 0 
    || construct_pub_topic() == 0
    #if PATIENT_SUBSCRIBE_ENABLED
        || construct_sub_topic() == 0
    #endif
    ) {

      LOG_ERR("Fatal: identifier construction failed\n");
      PROCESS_EXIT();
  }

  /*
   *  Patient discharge resource
   *  CLOUD -> DEVICE
   */
  coap_activate_resource(&res_discharge, "er/patient/discharge");

  /*
   *  Doctor change the triage code
   *  CLOUD -> DEVICE
   */
  coap_activate_resource(&res_triage, "er/patient/triage");

  /*
   * Populate patient's vitals with default values
   */
  patient_vitals_init();

  /* 
   * Hand control of the MQTT transport to the service. From here on,
   * this process only reacts to buttons and forwards events.
   */
  mqtt_service_init(&patient_process, client_id,
                    on_mqtt_connected, on_publish_slot, on_mqtt_incoming);


  //  Timer to check the gateway status
  etimer_set(&gateway_timer, GATEWAY_CHECK_INTERVAL);
  LOG_INFO("Gateway timer armed, interval=%lu ticks\n",
           (unsigned long)GATEWAY_CHECK_INTERVAL);

  etimer_set(&measure_timer, DEFAULT_PUBLISH_INTERVAL);

  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    if(ev == PROCESS_EVENT_TIMER && data == &gateway_timer) {
      gateway_update();
      etimer_reset(&gateway_timer);
      continue;
    }

    if(ev == PROCESS_EVENT_TIMER && data == &measure_timer) {
      clock_time_t next = measure_and_publish();
      etimer_set(&measure_timer, next);
      continue;
    }

    //  A discharge request triggers this event
    if(ev == discharge_event) {

        LOG_INFO("Patient discharged, stopping communications . . .\n");

        mqtt_service_stop();

        LOG_INFO("Patient node stopped. Waiting for shutdown.\n");

        PROCESS_EXIT();

    }

    if(ev == stop_observation_event) {
        stop_observation();
        continue;
    }

    /* Timers/polls belonging to the MQTT service */
    if(mqtt_service_handle_event(ev, data)) {
      continue;
    }

    if(ev == button_hal_release_event &&
       ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO) {
      
        //  When the button is pressed, an assistance request is sent to the nurse
        LOG_INFO("--Button pressed--\n");

        /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
        coap_init_message(request_for_nurse, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request_for_nurse, ASSISTANCE_URI);
        coap_set_header_content_format(request_for_nurse, APPLICATION_JSON);

        //  Build JSON payload
        char msg[40];
        snprintf(msg, sizeof(msg), "{\"PATIENT_ID\":%li,\"TIMESTAMP\":%li}", patient_info.patient_id, clock_seconds());

        coap_set_payload(request_for_nurse, (uint8_t *)msg, strlen(msg));

        LOG_INFO_COAP_EP(&nurse_addr);
        LOG_INFO_("\n");

        COAP_BLOCKING_REQUEST(&nurse_addr, request_for_nurse, nurse_request_callback);

        LOG_INFO("\n--Request sent--\n");

    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/