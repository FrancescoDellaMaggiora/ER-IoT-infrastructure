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

#include "contiki.h"
#include "dev/button-hal.h"
#include "os/sys/log.h"

#include "patient.h"
#include "triage_model.h"
#include "mqtt-service.h"
#include "coap-engine.h"
#include "vitals-buffer.h"
#include "triage-report.h"

#include "coap-blocking-api.h"
#include "coap-log.h"

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
/* Feature flag: subscription support.
 * SENSORS CAN'T RECEIVE MESSAGES YET: set to 1 once the node needs to
 * receive commands, then adapt construct_sub_topic() and
 * on_mqtt_incoming() below. */
#define PATIENT_SUBSCRIBE_ENABLED 0
/*---------------------------------------------------------------------------*/

/* Publish intervals.
 * The rate policy lives HERE (application), not in the MQTT service:
 * on_publish_slot() returns the delay until the next slot.
 * Publish every 30 seconds
 */
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)

/*
 * Alert Interval
 * The rate for publish an alert.
 * Publish every 1 second
 */
#define ALERT_PUBLISH_INTERVAL      (1 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
/*
 * Buffer for the registration uri
 */
#define URI_SIZE 64
static char REGISTRATION_URI[URI_SIZE];

/*
 * URI for ask assistance to a nurse 
 */
#define ASSISTANCE_URI "/er/patient/assistance"
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topics.
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];

static char vitals_topic[BUFFER_SIZE];
static char alert_topic[BUFFER_SIZE];

#if PATIENT_SUBSCRIBE_ENABLED
static char sub_topic[BUFFER_SIZE];
#endif

/*---------------------------------------------------------------------------*/
/*
 * The two MQTT payload buffer.
 * We will need to increase if we start publishing more data.
 * We need 2 separete buffer because otherwise one buffer will be completed override.
 */
#define APP_BUFFER_SIZE 512
static char vitals_app_buffer[APP_BUFFER_SIZE];
static char alert_app_buffer[APP_BUFFER_SIZE];

/*---------------------------------------------------------------------------*/
/*  The sequence number is associated to a measurement cycle, so vital +
 *  alert, e.g.:
 *    measurement cycle #1 -> vitals #1
 *    measurement cycle #2 -> vitals #2, alert #2
 *
 *  As of now, this number never gets transmitted anywhere
 */
static uint16_t seq_nr_value = 0;

/*---------------------------------------------------------------------------*/
/*
 *  This variable indicates which sensors are attached to the patient using
 *  the SENSOR macros (in this case the all the vitals are measured)
 */
static uint8_t attached_sensors =
  SENSOR_HEART_RATE | SENSOR_SPO2 | SENSOR_TEMPERATURE |
  SENSOR_PRESSURE_SYSTOLIC | SENSOR_PRESSURE_DIASTOLIC |
  SENSOR_RESPIRATION_RATE;


/*  This variabile indicates which alert conditions are currently active
 *  using the ALERT macros
*/
static uint8_t active_alerts = 0;

/*---------------------------------------------------------------------------*/
/*
 *  Data structure holding current patient vitals
 */
static patient_vitals_t current_vitals;

/*
 *  Event triggered during a patient discharge request
 */
process_event_t discharge_event;

/*
 * Device information
 */
static device_data_t patient_info;

/*---------------------------------------------------------------------------*/
/*
 * Input for the AI model Useless?
 */ 
//static float model_input[VITALS_WINDOW * VITALS_FEATURES];

/*---------------------------------------------------------------------------*/
/*
 *  Discharge resource
 */
extern coap_resource_t res_discharge;

/*
 * Cloud Application CoAP endpoint
 */
static coap_endpoint_t server_addr;

/*
 * Nurse CoAP endpoint
 */
static coap_endpoint_t nurse_addr;


/*
 * Simulation Parameters
 */
const simulation_parameters_t SIMULATION_VALUES[SIM_PARAM_COUNT] = {
  /* baseline, noise_amp, pull_pct */
  {  72.0f,      3.3f,      15.0f },   /* SIM_HR   */
  {  98.0f,      0.5f,      25.0f },   /* SIM_SPO2 */
  {  36.0f,      0.3f,      20.0f },   /* SIM_TEMP */
  { 115.0f,      6.9f,      20.0f },   /* SIM_SBP  */
  {  61.0f,      6.3f,      20.0f },   /* SIM_DBP  */
  {  14.0f,      0.5f,      0.5f },   /* SIM_RR   */
};

/*
 * Simulation state kept in floating point.
 *
 * The published vitals are integers, but the AR(1) process MUST NOT be
 * iterated on them: casting to int truncates toward zero, so positive
 * noise is discarded while negative noise is amplified. That is a
 * systematic -0.5/step drift, not noise - the mean reversion then
 * settles at whatever deviation makes the pull cancel it (SpO2 ended up
 * stuck at 56 instead of 98). Keeping the state as float and rounding
 * only for publication removes the bias entirely.
 */
static float sim_hr, sim_spo2, sim_sbp, sim_dbp, sim_rr;

/*---------------------------------------------------------------------------*/
PROCESS(patient_process, "Patient node");
AUTOSTART_PROCESSES(&patient_process);

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
 *  mechanism. The publish rate consequence (1 s instead of 30 s) is
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

  /* 0 until the window fills up (VITALS_WINDOW measurement cycles
    * after boot), and on inference failure. */
  uint8_t predicted_code = triage_model_predict();

  if(predicted_code != 0 && predicted_code != patient_info.triage_code) {
 
    if(predicted_code != 0 && predicted_code != patient_info.triage_code) {
 
      LOG_INFO("Model changed triage code: %u -> %u\n",
             patient_info.triage_code, predicted_code);
 
      patient_info.triage_code = predicted_code;
  
      /* Then tell the Cloud. Returns immediately; the CoAP exchange
      * completes in the background. */
      triage_report_send(patient_info.patient_id, predicted_code);
    } 
  }
}


/*---------------------------------------------------------------------------*/
/* SenML payload construction                                                */
/*---------------------------------------------------------------------------*/
//  Functions to build each topic's payload. The payload will 
//  depend on mounted sensors, active alerts, ...


typedef struct {
  char *buf;        /* current write position          */
  int remaining;    /* space left, including the '\0'  */
  bool first;       /* no record written yet -> no ',' */
  bool failed;      /* a snprintf overflowed           */
} senml_builder_t;

/* 
 * Internal: append formatted text, tracking overflow
 */
static void senml_append(senml_builder_t *b, const char *fmt, ...)
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

/* 
 * Open the SenML pack: '['
 */
static void senml_begin(senml_builder_t *b, char *buffer, int buffer_size)
{
  b->buf = buffer;
  b->remaining = buffer_size;
  b->first = true;
  b->failed = false;

  senml_append(b, "[");
}

/*
 * Append one record with an integer value:
 * {"n":"<name>","u":"<unit>","v":<value>}
 */
static void senml_add_int(senml_builder_t *b, const char *name, const char *unit,
              int value)
{
  if(!b->first) {
    //  A comma has to be added first
    senml_append(b, ",");
  }
  senml_append(b, "{\"n\":\"%s\",\"u\":\"%s\",\"v\":%d}", name, unit, value);
  b->first = false;
}

/* 
 * Append one record with a float value printed with one decimal.
 * NOTE for the real nRF52840 deployment: newlib-nano's printf does not
 * print floats by default (needs the '-u _printf_float' linker flag, or
 * a conversion to integer tenths). On Cooja this works as-is.
 */
static void senml_add_float1(senml_builder_t *b, const char *name, const char *unit,
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

/* 
 * Close the SenML pack: ']'. Returns 1 on success, 0 on overflow.
 */
static int senml_end(senml_builder_t *b, const char *caller)
{
  senml_append(b, "]");

  if(b->failed) {
    LOG_ERR("%s: payload buffer too short\n", caller);
    return 0;
  }
  return 1;
}

/*---------------------------------------------------------------------------*/
static int build_payload_vitals(char *buffer, int buffer_size)
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
static int build_payload_alert(char *buffer, int buffer_size)
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
  if(active_alerts & ALERT_RESPIRATION_RATE) {
    senml_add_int(&b, "respiration-rate-alert", "respiration/min",
                  current_vitals.respiration_rate);
  }

  return senml_end(&b, "build_payload_alert()");
}

/*---------------------------------------------------------------------------*/
/* Publishing                                                                */
/*---------------------------------------------------------------------------*/
/* 
 *  ALESSANDRO: This function accepts the topic to publish as input
 *  (before it was publish(void)). Moreover, it uses 
 *  functions to produce the correct SenML payload.
 */
static void publish(uint8_t topic_id) {

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
      return;
    }
  } else if(topic_id == TOPIC_MSG_ALERT) { //  If topic = "alert"

    topic = alert_topic;
    qos = MQTT_QOS_LEVEL_1;
    buf = alert_app_buffer;

    if(!build_payload_alert(buf, APP_BUFFER_SIZE)) {
      LOG_ERR("Alert payload construction fail\n");
      return;
    }
  } else {

    LOG_ERR("Unknown MQTT topic id: %u\n", topic_id);
    return;
  }

  mqtt_status_t status = mqtt_service_publish(topic, (uint8_t *)buf, strlen(buf), qos);

  if(status != MQTT_STATUS_OK) {
    LOG_WARN("Publish on '%s' REFUSED, status %d (payload %u B)\n",
            topic, status, (unsigned)strlen(buf));
    return;
  }

  LOG_DBG("Publish on '%s'!\n", topic);
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
  #if PATIENT_SUBSCRIBE_ENABLED
    mqtt_service_subscribe(sub_topic);
  #else
    LOG_DBG("Connected (no subscriptions)\n");
  #endif
}

/*---------------------------------------------------------------------------*/
/*
 * Contiki-NG's MQTT out queue holds ONE message at a time: a second
 * mqtt_publish() in the same slot is refused with
 * MQTT_STATUS_OUT_QUEUE_FULL, regardless of payload size (a 46 B alert
 * was refused just like a 180 B one). So the alert is deferred to the
 * next slot rather than sent back-to-back with the vitals.
 */

static bool alert_pending = false;

static clock_time_t on_publish_slot(void) {

  if(alert_pending) {
    /* The previous slot raised an alert. Publish it now and skip the
     * measurement cycle: the alert must carry the values that actually
     * triggered it, not fresher ones that may be back in range. */
    LOG_DBG("Publishing deferred alert\n");
    publish(TOPIC_MSG_ALERT);
    alert_pending = false;
    return ALERT_PUBLISH_INTERVAL;
  }
  //  "patient_measurement_cycle()" checks sensor values
  LOG_DBG("Starting measurement cycle\n");
  patient_measurement_cycle();

  seq_nr_value++;

  LOG_DBG("Publishing vitals\n");
  publish(TOPIC_MSG_VITALS);

  //  If alerts are detected they get published
  if(active_alerts != 0) {
    LOG_DBG("Alerts detected, deferring to next slot\n");
    alert_pending = true;
    return ALERT_PUBLISH_INTERVAL;
  }

  //  In case of alert publish every second instead of every 30 seconds
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
  printf("Patient ID: %li\t Triage code: %i\n", patient_info.patient_id, patient_info.triage_code);
}

/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS
/*---------------------------------------------------------------------------*/

static struct etimer et;
static int flagRegistration = 0;

/*
 *  This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses
 */
void client_chunk_handler(coap_message_t *response) {

  const uint8_t *chunk;
  uint8_t status;
  int len;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }

  status = response->code;
  printf("Response: %u.%02u\n", status / 32, status % 32);

  len = coap_get_payload(response, &chunk);

  char payload[128];

  if(len <= 0 || len >= sizeof(payload)) {
    printf("Invalid payload\n");
    return;
  }

  memcpy(payload, chunk, len);
  payload[len] = '\0';

  if(parse_registration(payload, len, &patient_info.patient_id, &patient_info.triage_code, &nurse_addr) == 0) {
    printf("Device successfully registered\n");
    print_patient_info();
    flagRegistration = 1;

  } else {
    printf("Patient info parsing error, will retry\n");
    etimer_reset(&et);
  }
       
}

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/




/*---------------------------------------------------------------------------*/
PROCESS_THREAD(patient_process, ev, data)
{

  PROCESS_BEGIN();
  

  printf("Patient node process (DEVICE_ID=%d)\n", DEVICE_ID);
  vitals_buffer_init();
  triage_report_init();

  //  The device tries to register after 1 second it's on
  etimer_set(&et, 60 * CLOCK_SECOND);
    
  //  Set the resource URI /er/patient/registration/<DEVICE_ID>
  snprintf(REGISTRATION_URI, sizeof(REGISTRATION_URI), "/er/patient/registration/%i", DEVICE_ID);

  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  static coap_message_t request_for_nurse[1]; 

  coap_endpoint_parse(SERVER_ADDR, strlen(SERVER_ADDR), &server_addr);

  memset(&patient_info, 0, sizeof(patient_info));

  while(flagRegistration == 0) {

    PROCESS_YIELD();
        
    if(etimer_expired(&et)) {

      printf("--Timer expired--\n");

      /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
      coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
      coap_set_header_uri_path(request, REGISTRATION_URI);

      LOG_INFO_COAP_EP(&server_addr);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_addr, request, client_chunk_handler);

      printf("\n--Registration request sent--\n");
    }
  }

  //  This event is needed by the discharge resource to interrupt all MQTT and COAP communication
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
   * Populate patient's vitals with default values
   */
  patient_vitals_init();

  /* 
   * Hand control of the MQTT transport to the service. From here on,
   * this process only reacts to buttons and forwards events.
   */
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
      
        //When press the button, ask for help to a nurse<
        printf("--Button pressed--\n");

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

        COAP_BLOCKING_REQUEST(&nurse_addr, request_for_nurse, client_chunk_handler);

        printf("\n--Request sent--\n");

    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
