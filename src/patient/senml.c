/*---------------------------------------------------------------------------*/
/*
 * senml.c - Generic SenML pack builder (implementation)
 */
/*---------------------------------------------------------------------------*/
#include "senml.h"

#include "sys/log.h"
#define LOG_MODULE "senml"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* 
 * Internal: append formatted text, tracking overflow
 */
void senml_append(senml_builder_t *b, const char *fmt, ...)
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
/*---------------------------------------------------------------------------*/
/* 
 * Open the SenML pack: '['
 */
void senml_begin(senml_builder_t *b, char *buffer, int buffer_size)
{
  b->buf = buffer;
  b->remaining = buffer_size;
  b->first = true;
  b->failed = false;

  senml_append(b, "[");
}
/*---------------------------------------------------------------------------*/
/*
 * Append one record with an integer value:
 * {"n":"<n>","u":"<unit>","v":<value>}
 */
void senml_add_int(senml_builder_t *b, const char *name, const char *unit,
              int value)
{
  if(!b->first) {
    //  A comma has to be added first
    senml_append(b, ",");
  }
  senml_append(b, "{\"n\":\"%s\",\"u\":\"%s\",\"v\":%d}", name, unit, value);
  b->first = false;
}
/*---------------------------------------------------------------------------*/
/*
 * Adding the 't' key
 *
 * Live readings emit no 't' at all: age zero means "now", which is the
 * default the receiver already assumes.
 */
void
senml_add_age(senml_builder_t *b, clock_time_t taken_at)
{
  long age;

  if(taken_at == 0) {
    return;      /* live reading */
  }

  age = (long)clock_seconds() - (long)taken_at;

  if(age <= 0) {
    return;      /* same second, nothing to correct */
  }

  if(!b->first) {
    senml_append(b, ",");
  }
  senml_append(b, "{\"t\":-%ld}", age);
  b->first = false;
}
/*---------------------------------------------------------------------------*/
/*
 * Append one record with a value printed to one decimal.
 *
 * The decimal part is built by hand from an integer rather than with
 * "%.1f": newlib-nano, the libc used on the nRF52840, omits floating
 * point support from printf to save flash, so "%f" silently prints
 * nothing and the JSON comes out as {"v":}.
 */
void
senml_add_float1(senml_builder_t *b, const char *name, const char *unit,
                 float value)
{
  int whole, tenths;
  long scaled;

  if(!b->first) {
    senml_append(b, ",");
  }

  /* Round to one decimal, then split. Handles negatives correctly:
   * -36.25 -> -36 and 3 tenths, printed as -36.3 */
  scaled = (long)(value * 10.0f + (value >= 0 ? 0.5f : -0.5f));
  whole = (int)(scaled / 10);
  tenths = (int)(scaled % 10);
  if(tenths < 0) {
    tenths = -tenths;
  }

  senml_append(b, "{\"n\":\"%s\",\"u\":\"%s\",\"v\":%d.%d}",
               name, unit, whole, tenths);
  b->first = false;
}
/*---------------------------------------------------------------------------*/
/* 
 * Close the SenML pack: ']'. Returns 1 on success, 0 on overflow.
 */
int senml_end(senml_builder_t *b, const char *caller)
{
  senml_append(b, "]");

  if(b->failed) {
    LOG_ERR("%s: payload buffer too short\n", caller);
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/