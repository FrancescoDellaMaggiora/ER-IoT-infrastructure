/*---------------------------------------------------------------------------*/
/*
 * triage-model.c - On-device triage classification (implementation)
 */
/*---------------------------------------------------------------------------*/
#include "triage_model.h"

#include <math.h>
#include <string.h>

/* emlearn generated model.
 * TODO: rename to whatever the notebook produced (it is named after the
 * window size: triage_w20.h -> struct triage_w20). */
#include "triage_w20.h"
#define ML_MODEL triage_w20

#include "sys/log.h"
#define LOG_MODULE "triage-ml"
#ifdef PATIENT_CONF_LOG_LEVEL
#define LOG_LEVEL PATIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
/*---------------------------------------------------------------------------*/

static const float FEATURE_MEAN[ML_N_INPUTS] = {
  74.387909f, 1.324344f, 72.650040f, 76.525810f, 0.000353f, 109.649620f, 2.409699f, 105.997200f, 113.496552f, 0.003130f, 59.236668f, 1.751063f, 56.425270f, 61.812954f, 0.001647f, 13.674246f, 0.078965f, 13.565016f, 13.785368f, 0.000347f, 96.314041f, 0.109620f, 96.241760f, 96.357643f, -0.000546f, 33.501007f, 0.018429f, 33.469517f, 33.528313f, 0.000139f
};

static const float FEATURE_SCALE[ML_N_INPUTS] = {
  17.021902f, 2.786590f, 16.764019f, 18.377077f, 0.354184f, 39.519066f, 5.465877f, 39.858753f, 40.525295f, 0.717942f, 29.222193f, 5.088573f, 29.795784f, 30.690975f, 0.628427f, 3.778821f, 0.460263f, 3.814805f, 3.866155f, 0.066621f, 4.273029f, 0.468495f, 4.539761f, 4.225090f, 0.062073f, 6.008343f, 0.288988f, 6.053574f, 5.995627f, 0.037810f
};

/* Scratch buffers: static, not on the stack. The window alone is
 * VITALS_WINDOW * VITALS_FEATURES floats (480 B at W=20), too large a
 * stack frame for a constrained node. */
static float window[VITALS_WINDOW * VITALS_FEATURES];
static float features[ML_N_INPUTS];
static float probabilities[ML_N_CLASSES];
/*---------------------------------------------------------------------------*/
/*
 * Fills 'features' with the 5 statistics of every vital, feature-major:
 *   [HR_mean, HR_std, HR_min, HR_max, HR_slope, SBP_mean, ...]
 *
 * This is the exact layout the notebook's extract_features() produces;
 * the deployment cell prints it as a comment block for cross-checking.
 */
static void
compute_features(void)
{
  const float t_mean = (VITALS_WINDOW - 1) / 2.0f;
  float t_var = 0.0f;
  uint8_t f, i;

  /* sum((t - t_mean)^2) with t = 0..W-1, constant for a given window */
  for(i = 0; i < VITALS_WINDOW; i++) {
    float dt = (float)i - t_mean;
    t_var += dt * dt;
  }

  for(f = 0; f < VITALS_FEATURES; f++) {
    float sum = 0.0f, sum_sq = 0.0f, cov = 0.0f;
    float mn, mx, mean, var;

    mn = mx = window[f];      /* first reading of this vital */

    for(i = 0; i < VITALS_WINDOW; i++) {
      float v = window[i * VITALS_FEATURES + f];
      sum += v;
      if(v < mn) { mn = v; }
      if(v > mx) { mx = v; }
    }
    mean = sum / VITALS_WINDOW;

    /* Second pass for the variance and the covariance with time.
     * Two passes rather than the sum-of-squares shortcut: with values
     * around 115 (SBP) the shortcut subtracts two large nearly-equal
     * numbers and loses most of the precision in a float. */
    for(i = 0; i < VITALS_WINDOW; i++) {
      float dv = window[i * VITALS_FEATURES + f] - mean;
      sum_sq += dv * dv;
      cov += dv * ((float)i - t_mean);
    }

    /* POPULATION variance (divide by W, not W-1): numpy's .std()
     * defaults to ddof=0, and the training features were computed with
     * it. Using the sample variance here would shift every std input. */
    var = sum_sq / VITALS_WINDOW;

    features[f * ML_N_STATS + 0] = mean;
    features[f * ML_N_STATS + 1] = sqrtf(var);
    features[f * ML_N_STATS + 2] = mn;
    features[f * ML_N_STATS + 3] = mx;
    features[f * ML_N_STATS + 4] = cov / t_var;   /* least-squares slope */
  }
}
/*---------------------------------------------------------------------------*/
static void
standardize(void)
{
  uint8_t i;

  for(i = 0; i < ML_N_INPUTS; i++) {
    features[i] = (features[i] - FEATURE_MEAN[i]) / FEATURE_SCALE[i];
  }
}
/*---------------------------------------------------------------------------*/
static uint8_t
argmax(const float *v, uint8_t n)
{
  uint8_t i, best = 0;

  for(i = 1; i < n; i++) {
    if(v[i] > v[best]) {
      best = i;
    }
  }
  return best;
}
/*---------------------------------------------------------------------------*/
uint8_t
triage_model_predict(void)
{
  int32_t status;
  uint8_t class_idx;

  if(!vitals_buffer_export(window)) {
    return 0;      /* window not full yet */
  }

  compute_features();
  standardize();

  status = eml_net_predict_proba(&ML_MODEL, features, ML_N_INPUTS,
                                 probabilities, ML_N_CLASSES);

  if(status != 0) {
    LOG_ERR("Inference failed, status %ld\n", (long)status);
    return 0;
  }

  class_idx = argmax(probabilities, ML_N_CLASSES);

  LOG_INFO("Predicted class %u (p=%d%%)\n",
          class_idx+1, (int)(probabilities[class_idx] * 100));

  /* The network outputs a class INDEX 0-4; triage codes run 1-5.
   * TODO: confirm against CODE_LABELS in the notebook - if the labels
   * were built as (code - 1), this +1 is right; if they were stored as
   * the codes themselves, drop it. */
  return (uint8_t)(class_idx + 1);
}
/*---------------------------------------------------------------------------*/

// Dummy function to suppress unused variable/function warnings from emlearn
void suppress_emlearn_warnings(void) {
    (void)eml_error_str(0);
    (void)eml_net_activation_function_strs;
}