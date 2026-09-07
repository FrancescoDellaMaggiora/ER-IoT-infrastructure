/*---------------------------------------------------------------------------*/
/*
 * ER-IOT-INFRASTRUCTURE
 * vitals-buffer.h - Sliding window of recent readings, fed to the CNN
 *
 * The triage CNN takes a window of
 * VITALS_WINDOW consecutive readings, each with VITALS_FEATURES values.
 * This module keeps that window as a circular buffer: every measurement
 * cycle pushes one reading, and once the buffer has filled up the flat
 * array can be handed to the interpreter.
 */
/*---------------------------------------------------------------------------*/
#ifndef VITALS_BUFFER_H_
#define VITALS_BUFFER_H_
/*---------------------------------------------------------------------------*/
#include "patient.h"

#include <stdbool.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/

#define VITALS_WINDOW    20

/* Number of features per reading */
#define VITALS_FEATURES  6

/* Feature positions, in TRAINING order (HR, SBP, DBP, RR, SpO2, Temp) */
#define VITALS_IDX_HR    0
#define VITALS_IDX_SBP   1
#define VITALS_IDX_DBP   2
#define VITALS_IDX_RR    3
#define VITALS_IDX_SPO2  4
#define VITALS_IDX_TEMP  5
/*---------------------------------------------------------------------------*/
/** Empties the buffer. Call once at startup. */
void vitals_buffer_init(void);

/**
 * Pushes one reading, overwriting the oldest one when full.
 * Call once per measurement cycle, after the vitals have been updated.
 */
void vitals_buffer_push(const patient_vitals_t *vitals);

/**
 * True once VITALS_WINDOW readings have been pushed - i.e. the model
 * can be run. Before that, the window is incomplete and a prediction
 * would be based on padding.
 *
 * With one reading per publish slot, this becomes true only after
 * VITALS_WINDOW slots: that is the time-to-first-prediction after boot.
 */
bool vitals_buffer_is_full(void);

/**
 * Copies the window into 'out' as a flat, chronologically ordered
 * array: out[t * VITALS_FEATURES + f], oldest reading first.
 *
 * The copy is needed because the internal storage is circular - the
 * oldest reading is not necessarily at index 0 - while the model
 * expects a contiguous time-ordered tensor.
 *
 * 'out' must have room for VITALS_WINDOW * VITALS_FEATURES floats.
 * Returns false (and leaves 'out' untouched) if the buffer is not full.
 */
bool vitals_buffer_export(float *out);
/*---------------------------------------------------------------------------*/
#endif /* VITALS_BUFFER_H_ */
/*---------------------------------------------------------------------------*/
