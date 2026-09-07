/*
 * vitals-buffer.c - Sliding window of recent readings (implementation)
 */

 /*---------------------------------------------------------------------------*/
#include "vitals-buffer.h"

#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "vitals-buf"
#ifdef PATIENT_CONF_LOG_LEVEL
#define LOG_LEVEL PATIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
/*---------------------------------------------------------------------------*/
/*
 * Circular storage.
 *
 * Stored as float rather than the native per-vital types because the
 * model works on real numbers and temperature carries a decimal part;
 * quantisation to int8 (if the deployed model is the quantised one)
 * belongs to the inference step, not here - keeping raw values means
 * this buffer stays valid whichever variant of the model is used.
 *
 * Memory: VITALS_WINDOW * VITALS_FEATURES * 4 B = 20 * 6 * 4 = 480 B.
 */
static float window[VITALS_WINDOW][VITALS_FEATURES];

/* Index where the NEXT reading will be written */
static uint8_t head;

/* How many readings have been pushed, capped at VITALS_WINDOW */
static uint8_t count;
/*---------------------------------------------------------------------------*/
void
vitals_buffer_init(void)
{
  memset(window, 0, sizeof(window));
  head = 0;
  count = 0;
}
/*---------------------------------------------------------------------------*/
void
vitals_buffer_push(const patient_vitals_t *vitals)
{
  float *slot = window[head];

  /* Written by explicit index, in TRAINING order - see the warning in
   * vitals-buffer.h. This is the one place where the firmware's field
   * order and the model's feature order meet. */
  slot[VITALS_IDX_HR]   = (float)vitals->heart_rate;
  slot[VITALS_IDX_SBP]  = (float)vitals->pressure_systolic;
  slot[VITALS_IDX_DBP]  = (float)vitals->pressure_diastolic;
  slot[VITALS_IDX_RR]   = (float)vitals->respiration_rate;
  slot[VITALS_IDX_SPO2] = (float)vitals->spo2;
  slot[VITALS_IDX_TEMP] = vitals->temperature;

  head = (uint8_t)((head + 1) % VITALS_WINDOW);

  if(count < VITALS_WINDOW) {
    count++;
    if(count == VITALS_WINDOW) {
      LOG_INFO("Window full: predictions can start\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
bool
vitals_buffer_is_full(void)
{
  return count == VITALS_WINDOW;
}
/*---------------------------------------------------------------------------*/
bool
vitals_buffer_export(float *out)
{
  uint8_t i;

  if(!vitals_buffer_is_full()) {
    return false;
  }

  /* When the buffer is full, 'head' points at the OLDEST reading (the
   * one about to be overwritten), so unrolling from there gives the
   * window in chronological order. */
  for(i = 0; i < VITALS_WINDOW; i++) {
    uint8_t src = (uint8_t)((head + i) % VITALS_WINDOW);
    memcpy(&out[i * VITALS_FEATURES], window[src],
           VITALS_FEATURES * sizeof(float));
  }

  return true;
}
/*---------------------------------------------------------------------------*/
