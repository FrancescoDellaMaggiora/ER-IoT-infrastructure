/**
 * CoAP resource: /dept/signal
 *
 * A peer department BR PUTs here a single ASCII digit = ITS OWN dept id
 * ("who is calling"). This node stores the caller and lights the RGB LED
 * with the caller's colour (see dept-signal.h for the mapping).
 *
 * The signal is cleared ONLY locally (button press on this BR): there is
 * no remote clear, by design.
 *
 * GET is exposed for inspection/debugging: returns the caller id, or -1.
 */

#include <stdio.h>
#include <string.h>

#include "coap-engine.h"
#include "dev/leds.h"
#include "dept-signal.h"

#include "sys/log.h"
#define LOG_MODULE "res-signal"
#define LOG_LEVEL LOG_LEVEL_INFO

/* This BR's own department id (0..5), injected from the Makefile:
 *   make DEPT_ID=<n>  ->  CFLAGS += -DMY_DEPT_ID=<n> */
#ifndef MY_DEPT_ID
#define MY_DEPT_ID 0
#endif

/* All three RGB channels */
#define LEDS_RGB_ALL (LEDS_RED | LEDS_GREEN | LEDS_BLUE)

/*---------------------------------------------------------------------------*/
/* Shared signal state                                                       */
/*---------------------------------------------------------------------------*/

/* Current caller: -1 = idle */
static int caller = -1;

/* Colour of each department on the RGB LED (index = dept id). */
static const unsigned char dept_colour[DEPT_COUNT] = {
  LEDS_RED,                  /* 0 pediatrico                */
  LEDS_GREEN,                /* 1 ostetrico-ginecologico    */
  #if CONTIKI_TARGET_COOJA
    LEDS_BLUE,                 /* 2 agitazione psico-motoria  */
    LEDS_RED | LEDS_GREEN,     /* 3 disabilita' complessa     */
    LEDS_RED | LEDS_BLUE,      /* 4 vittime di violenza       */
    LEDS_GREEN | LEDS_BLUE,    /* 5 malato infettivo          */
  #endif
};

int dept_signal_is_active(void)
{
  return caller >= 0;
}

int dept_signal_caller(void)
{
  return caller;
}

void dept_signal_refresh_led(void)
{
  leds_off(LEDS_RGB_ALL);
  if(caller >= 0 && caller < DEPT_COUNT) {
    leds_on(dept_colour[caller]);
  }
}

void dept_signal_clear(void)
{
  caller = -1;
  dept_signal_refresh_led();
}

void dept_signal_raise(uint8_t caller_id)
{
  /* Last caller wins: if two departments call before the local staff
   * acknowledges, the LED shows the most recent one. All calls are in
   * the log. TODO: document this policy in the report. */
  caller = caller_id;
  dept_signal_refresh_led();
}

/*---------------------------------------------------------------------------*/
/* CoAP handlers                                                             */
/*---------------------------------------------------------------------------*/
static void res_get_handler(coap_message_t *request, coap_message_t *response,
                            uint8_t *buffer, uint16_t preferred_size,
                            int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response,
                            uint8_t *buffer, uint16_t preferred_size,
                            int32_t *offset);

RESOURCE(res_dept_signal,
         "title=\"Inter-department signal\";rt=\"dept.signal\"",
         res_get_handler,   /* GET  */
         NULL,              /* POST */
         res_put_handler,   /* PUT  */
         NULL);             /* DELETE */

/*---------------------------------------------------------------------------*/
static void 
res_get_handler(coap_message_t *request, coap_message_t *response,
                uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int len = snprintf((char *)buffer, preferred_size, "%d", caller);

  coap_set_header_content_format(response, TEXT_PLAIN);
  coap_set_payload(response, buffer, len);
}

/*---------------------------------------------------------------------------*/
static void
res_put_handler(coap_message_t *request, coap_message_t *response,
                uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload = NULL;
  size_t len = coap_get_payload(request, &payload);

  /* Expected payload: one ASCII digit '0'..'5' = caller's dept id,
   * different from our own id (a department cannot call itself). */
  if(len == 1 && payload[0] >= '0' && payload[0] < '0' + DEPT_COUNT
     && (payload[0] - '0') != MY_DEPT_ID) {
    uint8_t caller_id = (uint8_t)(payload[0] - '0');

    dept_signal_raise(caller_id);
    LOG_INFO("Incoming call from dept %u -> LED on\n", caller_id);

    coap_set_status_code(response, CHANGED_2_04);
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00);
  }
}
