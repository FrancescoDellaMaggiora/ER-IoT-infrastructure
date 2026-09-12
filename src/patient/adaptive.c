/*---------------------------------------------------------------------------*/
/*
 * adaptive.c - The two adaptive mechanisms (implementation)
 */
/*---------------------------------------------------------------------------*/
#include "adaptive.h"

#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"

#include "sys/log.h"
#define LOG_MODULE "adaptive"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* Owned by patient.c */
extern clock_time_t last_RTT;
extern device_data_t patient_info;
/*---------------------------------------------------------------------------*/
/* Congestion handling (adaptive mechanism #1)                               */
/*---------------------------------------------------------------------------*/
#if ADAPTIVE_CONGESTION

/*
 * A single refused publish means nothing: it happens routinely when the
 * previous message has not left the queue yet. What indicates congestion
 * is a SEQUENCE of refusals, i.e. the queue never draining across several
 * consecutive slots.
 *
 * The thresholds are asymmetric on purpose: entering quickly protects
 * the channel, leaving slowly avoids flapping back into congestion on a
 * single lucky success.
 *
 * Another way to find out a congestion is when a alert message is too slow
 * for a sequence of times.
 */
static uint8_t consecutive_failures = 0;
static uint8_t consecutive_successes = 0;
static uint8_t consecutive_latency = 0;
static bool network_congested = false;

void congestion_update(bool publish_ok)
{
  if(publish_ok) {
    consecutive_failures = 0;
    if(network_congested) {
      consecutive_successes++;
      if(consecutive_successes >= CONGESTION_EXIT_THRESHOLD) {
        network_congested = false;
        consecutive_successes = 0;
        LOG_INFO("Congestion cleared\n");
      }
    }
  } else {
    consecutive_successes = 0;
    if(!network_congested) {
      consecutive_failures++;
      if(consecutive_failures >= CONGESTION_ENTER_THRESHOLD) {
        network_congested = true;
        consecutive_failures = 0;
        LOG_INFO("Congestion detected for too much failure: throttling low-priority vitals\n");
      }
    }
  }

  
  if (last_RTT >= CONGESTION_MAX_LATENCY * CLOCK_SECOND ) {
    consecutive_latency++;
    last_RTT = 0;
    if (consecutive_latency >= CONGESTION_ENTER_THRESHOLD) {
      network_congested = true;
      LOG_INFO("Congestion detected for latency: throttling low-priority vitals\n");
    }
  } else {
    consecutive_latency = 0;
  }
}

/*
 * Under congestion, low-priority patients stop sending ROUTINE vitals:
 * network triage mirroring clinical triage. Alerts are never suppressed
 * as a deteriorating white-code patient must still be able to raise one,
 * and that is precisely what makes the mechanism safe to apply.
 */
bool should_suppress_vitals(void)
{
  if(!network_congested) {
    return false;
  }

  return patient_info.triage_code == CODE_GREEN ||
         patient_info.triage_code == CODE_WHITE;
}

#else  /* ADAPTIVE_CONGESTION == 0 */

/* Baseline build: no detection, nothing ever suppressed. */
void congestion_update(bool publish_ok) { (void)publish_ok; }
bool should_suppress_vitals(void) { return false; }

#endif /* ADAPTIVE_CONGESTION */

/*---------------------------------------------------------------------------*/
/* Gateway failure handling (adaptive mechanism #2)                          */
/*---------------------------------------------------------------------------*/

#if ADAPTIVE_BUFFERING

/*
 * Detecting a dead border router.
 *
 * Nothing notifies the node: the BR simply stops answering.
 *     The default route is checked, which RPL removes once the BR's DIOs stop
 *     arriving. Faster and more reliable than waiting for TCP to time
 *     out, and it is the same check mqtt-service already uses to decide
 *     whether it is worth connecting at all;
 *
 * Same asymmetric hysteresis as the congestion detector: react quickly
 * to a loss, return slowly, so a single missed DIO does not flip the
 * node into buffering mode.
 */
static uint8_t gw_missing_count = 0;
static uint8_t gw_present_count = 0;
static bool gateway_down = false;

/*
 * Retention buffers.
 *
 * Readings taken while the gateway is down are kept here and replayed
 * when it comes back.
 *
 * Vitals and alerts are kept apart so a burst of routine readings can
 * never push an alert out of the window, the two fill up at their own
 * pace and the alert buffer only grows when something is actually wrong.
 */
static buffered_reading_t vitals_retention[RETENTION_CAPACITY];
static uint8_t vitals_ret_head = 0;
static uint8_t vitals_ret_count = 0;

static buffered_reading_t alert_retention[RETENTION_CAPACITY];
static uint8_t alert_ret_head = 0;
static uint8_t alert_ret_count = 0;

/* 
 * Dropped-on-overflow counters: the metric that makes the mechanism
 * measurable against the baseline build, where EVERY reading taken
 * during an outage is lost.
 */
static uint16_t vitals_dropped = 0;
static uint16_t alerts_dropped = 0;
/*---------------------------------------------------------------------------*/
void gateway_update(void)
{
  /*
   * The border router is this network's default router: when it dies,
   * RPL stops receiving its DIOs and removes the default route. Checking
   * the routing table directly is faster and more reliable than waiting
   * for TCP to time out on the MQTT connection, and it isolates the
   * failure we actually care about (the gateway) from an unrelated
   * broker outage, which would look identical through the MQTT state.
   */
  bool reachable = (uip_ds6_defrt_choose() != NULL);

  LOG_INFO("Gateway check: reachable=%d, missing=%u, down=%d\n",
           reachable, gw_missing_count, gateway_down);

  if(reachable) {
    gw_missing_count = 0;
    if(gateway_down) {
      gw_present_count++;
      if(gw_present_count >= GATEWAY_BACK_THRESHOLD) {
        gateway_down = false;
        gw_present_count = 0;
        LOG_INFO("Gateway back\n");
      }
    }
  } else {
    gw_present_count = 0;
    if(!gateway_down) {
      gw_missing_count++;
      if(gw_missing_count >= GATEWAY_LOST_THRESHOLD) {
        gateway_down = true;
        gw_missing_count = 0;
        LOG_WARN("Gateway unreachable: buffering readings locally\n");
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
bool gateway_is_down(void)
{
  return gateway_down;
}
/*---------------------------------------------------------------------------*/
static void
retention_push(buffered_reading_t *buf, uint8_t *head, uint8_t *count,
               uint16_t *dropped, const patient_vitals_t *v, uint8_t alerts)
{
  buf[*head].vitals = *v;
  buf[*head].alerts = alerts;
  buf[*head].taken_at = clock_seconds();
  *head = (uint8_t)((*head + 1) % RETENTION_CAPACITY);

  if(*count < RETENTION_CAPACITY) {
    (*count)++;
  } else {
    /* Full: the write above overwrote the oldest entry, and head now
     * points at the new oldest. Newest-wins on purpose, during a long
     * outage the recent clinical picture matters more than the stale one. */
    (*dropped)++;
  }
}
/*---------------------------------------------------------------------------*/
static bool
retention_peek(const buffered_reading_t *buf, uint8_t head, uint8_t count,
               buffered_reading_t *out)
{
  if(count == 0) {
    return false;
  }

  *out = buf[(head + RETENTION_CAPACITY - count) % RETENTION_CAPACITY];
  return true;
}
/*---------------------------------------------------------------------------*/
static void retention_commit(uint8_t *count)
{
  if(*count > 0) {
    (*count)--;
  }
}
/*---------------------------------------------------------------------------*/
void retention_store_vitals(const patient_vitals_t *v, uint8_t alerts)
{
  retention_push(vitals_retention, &vitals_ret_head, &vitals_ret_count,
                 &vitals_dropped, v, alerts);
}

void retention_store_alert(const patient_vitals_t *v, uint8_t alerts)
{
  retention_push(alert_retention, &alert_ret_head, &alert_ret_count,
                 &alerts_dropped, v, alerts);
}
/*---------------------------------------------------------------------------*/
bool retention_pending(void)
{
  return vitals_ret_count > 0 || alert_ret_count > 0;
}
/*---------------------------------------------------------------------------*/
bool retention_peek_alert(buffered_reading_t *out)
{
  return retention_peek(alert_retention, alert_ret_head, alert_ret_count, out);
}

bool retention_peek_vitals(buffered_reading_t *out)
{
  return retention_peek(vitals_retention, vitals_ret_head, vitals_ret_count,
                        out);
}

void retention_commit_alert(void)
{
  retention_commit(&alert_ret_count);
}

void retention_commit_vitals(void)
{
  retention_commit(&vitals_ret_count);
}
/*---------------------------------------------------------------------------*/
uint8_t retention_alert_count(void) { return alert_ret_count; }
uint8_t retention_vitals_count(void) { return vitals_ret_count; }
uint16_t retention_alerts_dropped(void) { return alerts_dropped; }
uint16_t retention_vitals_dropped(void) { return vitals_dropped; }

#else  /* ADAPTIVE_BUFFERING == 0 */

/* Baseline build: no detection, readings published (and lost) as usual. */
void gateway_update(void) { }
bool gateway_is_down(void) { return false; }

void retention_store_vitals(const patient_vitals_t *v, uint8_t alerts)
{
  (void)v; (void)alerts;
}

void retention_store_alert(const patient_vitals_t *v, uint8_t alerts)
{
  (void)v; (void)alerts;
}

bool retention_pending(void) { return false; }

bool retention_peek_alert(buffered_reading_t *out) { (void)out; return false; }
bool retention_peek_vitals(buffered_reading_t *out) { (void)out; return false; }

void retention_commit_alert(void) { }
void retention_commit_vitals(void) { }

uint8_t retention_alert_count(void) { return 0; }
uint8_t retention_vitals_count(void) { return 0; }
uint16_t retention_alerts_dropped(void) { return 0; }
uint16_t retention_vitals_dropped(void) { return 0; }

#endif /* ADAPTIVE_BUFFERING */
/*---------------------------------------------------------------------------*/