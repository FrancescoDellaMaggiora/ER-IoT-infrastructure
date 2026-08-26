/**
 * Shared state of the inter-department signal.
 *
 * The 6 departments follow the Tuscany ER pathway model:
 *   0 = pediatrico
 *   1 = ostetrico-ginecologico
 *   2 = agitazione psico-motoria
 *   3 = disabilita' complessa
 *   4 = vittime di violenza
 *   5 = malato infettivo
 *
 * RGB colour of the caller (one LED, 6 combinations):
 *   0 red, 1 green, 2 blue, 3 yellow (R+G), 4 magenta (R+B), 5 cyan (G+B)
 */

#ifndef DEPT_SIGNAL_H_
#define DEPT_SIGNAL_H_

#include <stdint.h>

#define DEPT_COUNT 6

/* 1 if a call from another department is currently shown on the LED */
int dept_signal_is_active(void);

/* Department id of the current caller, or -1 if idle */
int dept_signal_caller(void);

/* Clear the current call (LED off, back to idle) */
void dept_signal_clear(void);

/* Show an incoming call: store caller id and light its colour */
void dept_signal_raise(uint8_t caller_id);

/* Re-apply the LED according to the current state (used after
 * temporary LED effects, e.g. the send-feedback flash) */
void dept_signal_refresh_led(void);

#endif /* DEPT_SIGNAL_H_ */
