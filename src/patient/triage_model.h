/*---------------------------------------------------------------------------*/
/*
 * ER-IOT-INFRASTRUCTURE
 * triage_model.h - On-device triage classification
 *
 * Turns the sliding window held by vitals-buffer into a triage code:
 *
 *   window (W x 6)  ->  5 statistics per vital (30 values)
 *                   ->  standardize with the training mean/scale
 *                   ->  emlearn MLP  ->  class index  ->  triage code 1-5
 *
 * Every step here mirrors extract_features() and the standardization in
 * the notebook. Any divergence - a different statistic, a different
 * ordering, a different std convention - silently feeds the network
 * inputs it was never trained on: it will still return a class, just a
 * meaningless one.
 */
/*---------------------------------------------------------------------------*/
#ifndef TRIAGE_MODEL_H_
#define TRIAGE_MODEL_H_
/*---------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

#include "vitals-buffer.h"
/*---------------------------------------------------------------------------*/
/* 5 statistics per vital: mean, std, min, max, slope. */
#define ML_N_STATS   5
#define ML_N_INPUTS  (VITALS_FEATURES * ML_N_STATS)

/* Number of classes the network outputs (triage codes 1-5) */
#define ML_N_CLASSES 5
/*---------------------------------------------------------------------------*/
/**
 * Runs the model on the current window.
 *
 * Returns the predicted triage code, 1 (red) to 5 (white), or 0 if the
 * window is not full yet or the inference failed. 0 is never a
 * valid code, so the caller can use it as "no prediction".
 */
uint8_t triage_model_predict(void);
/*---------------------------------------------------------------------------*/
#endif /* TRIAGE_MODEL_H_ */
/*---------------------------------------------------------------------------*/