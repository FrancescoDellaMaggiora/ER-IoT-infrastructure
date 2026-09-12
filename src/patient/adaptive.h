/*---------------------------------------------------------------------------*/
/*
 * ER-IOT-INFRASTRUCTURE
 * adaptive.h - The two adaptive mechanisms
 *
 *   #1 congestion detection  -> routine vitals of low-priority patients
 *                               are suppressed while the channel is busy
 *   #2 gateway failure       -> readings taken while the border router is
 *                               unreachable are retained and replayed
 *
 * The retention buffers are PRIVATE to adaptive.c: the application asks
 * this module to store a reading and to hand back the oldest one, it
 * never sees the circular buffers themselves.
 *
 * Both mechanisms are switched at build time (ADAPTIVE_CONGESTION and
 * ADAPTIVE_BUFFERING). The baseline builds keep the SAME prototypes and
 * provide do-nothing bodies in adaptive.c: the declarations below are
 * therefore always valid, whatever the build.
 */
/*---------------------------------------------------------------------------*/
#ifndef ADAPTIVE_H_
#define ADAPTIVE_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "patient.h"

#include <stdbool.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
#define CONGESTION_ENTER_THRESHOLD  3
#define CONGESTION_EXIT_THRESHOLD   5
#define CONGESTION_MAX_LATENCY      3

#define GATEWAY_LOST_THRESHOLD  3
#define GATEWAY_BACK_THRESHOLD  2

/* How many readings of each kind survive an outage */
#define RETENTION_CAPACITY 16
/*---------------------------------------------------------------------------*/
/*
 * One retained reading: the RAW SNAPSHOT (vitals plus the alert
 * bitmask), not the built JSON.
 */
typedef struct {
  patient_vitals_t vitals;
  uint8_t alerts;              /* active_alerts at the time of the reading */
  clock_time_t taken_at;       /* clock_seconds() when the reading was taken */
} buffered_reading_t;
/*---------------------------------------------------------------------------*/
/* Congestion (mechanism #1) */

/** Feeds the detector with the outcome of one publish attempt. */
void congestion_update(bool publish_ok);

/** True when routine vitals of this patient should be dropped. */
bool should_suppress_vitals(void);
/*---------------------------------------------------------------------------*/
/* Gateway failure and retention (mechanism #2) */

/** Polls the default route. Call it on the gateway timer. */
void gateway_update(void);

/** True while the border router is considered unreachable. */
bool gateway_is_down(void);

/** Retains one reading. Oldest entry is dropped when the buffer is full. */
void retention_store_vitals(const patient_vitals_t *v, uint8_t alerts);
void retention_store_alert(const patient_vitals_t *v, uint8_t alerts);

/** True while there is anything left to replay. */
bool retention_pending(void);

/*
 * Copies the OLDEST entry into 'out' without removing it: the caller
 * commits only once the publish has been accepted, so a refused publish
 * does not lose the reading. Return false when that buffer is empty.
 */
bool retention_peek_alert(buffered_reading_t *out);
bool retention_peek_vitals(buffered_reading_t *out);

/** Drops the entry previously returned by the matching peek. */
void retention_commit_alert(void);
void retention_commit_vitals(void);

/* Counters, for the logs and for the comparison against the baseline */
uint8_t retention_alert_count(void);
uint8_t retention_vitals_count(void);
uint16_t retention_alerts_dropped(void);
uint16_t retention_vitals_dropped(void);
/*---------------------------------------------------------------------------*/
#endif /* ADAPTIVE_H_ */
/*---------------------------------------------------------------------------*/