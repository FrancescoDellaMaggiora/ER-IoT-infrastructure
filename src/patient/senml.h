/*---------------------------------------------------------------------------*/
/*
 * ER-IOT-INFRASTRUCTURE
 * senml.h - Generic SenML pack builder
 *
 * This module knows NOTHING about the patient: it only appends records
 * to a caller-provided buffer, tracking overflow. The payloads that
 * depend on the patient state (vitals, alerts, registration) are built
 * in patient.c, which owns that state.
 */
/*---------------------------------------------------------------------------*/
#ifndef SENML_H_
#define SENML_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
/*---------------------------------------------------------------------------*/
typedef struct {
  char *buf;        /* current write position          */
  int remaining;    /* space left, including the '\0'  */
  bool first;       /* no record written yet -> no ',' */
  bool failed;      /* a snprintf overflowed           */
} senml_builder_t;
/*---------------------------------------------------------------------------*/
void senml_append(senml_builder_t *b, const char *fmt, ...);
void senml_begin(senml_builder_t *b, char *buffer, int buffer_size);
void senml_add_int(senml_builder_t *b, const char *name, const char *unit,
                   int value);
void senml_add_age(senml_builder_t *b, clock_time_t taken_at);
void senml_add_float1(senml_builder_t *b, const char *name, const char *unit,
                      float value);
int senml_end(senml_builder_t *b, const char *caller);
/*---------------------------------------------------------------------------*/
#endif /* SENML_H_ */
/*---------------------------------------------------------------------------*/