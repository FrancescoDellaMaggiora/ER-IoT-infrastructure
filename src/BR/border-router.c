/**
 * Border Router
 *
 * One firmware for all 6 departments.
 * The identity of this BR is set at build time:
 *
 *     make TARGET=<target> DEPT_ID=<0..5>
 *
 * Roles of this node:
 *   1. RPL border router (RPL root + SLIP tunnel to the host) - handled
 *      entirely by the `rpl-border-router` service module, no code here.
 *   2. CoAP server: /dept/signal
 *   3. CoAP client: N-click encoding on the local button to call
 *      department N:
 *        - N presses (within CLICK_TIMEOUT of each other), then pause
 *          -> PUT /dept/signal to department (N-1) with our id as payload.
 *        - If a call is currently shown on the LED, the FIRST press only
 *          acknowledges/clears it (it does not count as a click).
 *
 * What is deliberately NOT here: MQTT (vitals traffic is routed
 * transparently at the IP layer).
 */

#include "contiki.h"
#include <string.h>

#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "sys/etimer.h"

#include "dept-signal.h"

#include "sys/log.h"
#define LOG_MODULE "BR"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/* This BR's department id (0..5), injected from the Makefile */
#ifndef MY_DEPT_ID
#define MY_DEPT_ID 0
#endif

/* Inactivity window that closes the click sequence */
#define CLICK_TIMEOUT     (CLOCK_SECOND * 3 / 2)   /* 1.5 s */

/* Duration of the "request sent" LED feedback flash */
#define FEEDBACK_FLASH    (CLOCK_SECOND / 3)

#define LEDS_RGB_ALL (LEDS_RED | LEDS_GREEN | LEDS_BLUE)

/* Static endpoint table of ALL departments, indexed by dept id.
 * The firmware always knows all 6, regardless of how many are actually
 * deployed in a given simulation/demo.
 *
 * NOTE: each department is a separate RPL network; BR<->BR traffic
 * transits through the backbone (the host running the tunslip6
 * instances must forward IPv6 between the tun interfaces). */
static const char *dept_endpoints[DEPT_COUNT] = {
  #if CONTIKI_TARGET_COOJA
    "coap://[fe80::201:1:1:1]",   /* 0 pediatrico                */
    "coap://[fe80::202:2:2:1]",   /* 1 ostetrico-ginecologico    */
    "coap://[fe80::203:3:3:1]",   /* 2 agitazione psico-motoria  */
    "coap://[fe80::204:4:4:1]",   /* 3 disabilita' complessa     */
    "coap://[fe80::205:5:5:1]",   /* 4 vittime di violenza       */
    "coap://[fe80::206:6:6:1]",   /* 5 malato infettivo          */
  #else
    "coap://[fd00:1::f6ce:36cc:e20:721]",   /* 0 pediatrico                */
    "coap://[fd00:2::f6ce:3639:6320:445f]", /* 1 ostetrico-ginecologico    */
  #endif
};

/*---------------------------------------------------------------------------*/
extern coap_resource_t res_dept_signal;

static coap_endpoint_t dept_eps[DEPT_COUNT];
static coap_message_t request[1];

/*---------------------------------------------------------------------------*/
static void
signal_response_handler(coap_message_t *response)
{
  if(response == NULL) {
    LOG_WARN("Call request timed out\n");
    return;
  }
  LOG_INFO("Target BR replied, code %u.%02u\n",
           response->code >> 5, response->code & 0x1F);
}

/*---------------------------------------------------------------------------*/
PROCESS(er_border_router_process, "ER Border Router");
AUTOSTART_PROCESSES(&er_border_router_process);

PROCESS_THREAD(er_border_router_process, ev, data)
{
  static struct etimer click_timer;
  static struct etimer flash_timer;
  static uint8_t click_count = 0;
  static int target;
  static uint8_t payload;
  static int i;

  PROCESS_BEGIN();

  LOG_INFO("ER Border Router started, department id %d\n", MY_DEPT_ID);

  coap_activate_resource(&res_dept_signal, "dept/signal");

  /* Pre-parse all peer endpoints once at startup */
  for(i = 0; i < DEPT_COUNT; i++) {
    if(i == MY_DEPT_ID) {
      continue;   /* never call ourselves */
    }
    if(coap_endpoint_parse(dept_endpoints[i], strlen(dept_endpoints[i]),
                           &dept_eps[i]) == 0) {
      LOG_ERR("Invalid endpoint for dept %d: %s\n", i, dept_endpoints[i]);
    }
  }

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == button_hal_press_event) {

      if(dept_signal_is_active()) {
        /* A call is shown: this press acknowledges it, nothing else. */
        LOG_INFO("Call from dept %d acknowledged locally\n",
                 dept_signal_caller());
        dept_signal_clear();
        click_count = 0;
        etimer_stop(&click_timer);
      } else {
        /* Click counting: N presses select target department N-1 */
        click_count++;
        LOG_INFO("Click %u\n", click_count);
        etimer_set(&click_timer, CLICK_TIMEOUT);   /* restart window */
      }

    } else if(ev == PROCESS_EVENT_TIMER && data == &click_timer
              && click_count > 0) {

      /* Click sequence closed: commit */
      target = click_count - 1;
      click_count = 0;

      if(target >= DEPT_COUNT || target == MY_DEPT_ID) {
        LOG_WARN("Invalid target %d (out of range or self), ignored\n",
                 target);
        continue;
      }

      LOG_INFO("Calling department %d\n", target);

      coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
      coap_set_header_uri_path(request, "/dept/signal");

      /* Payload = OUR dept id, so the called dept knows who is calling */
      payload = (uint8_t)('0' + MY_DEPT_ID);
      coap_set_payload(request, &payload, 1);

      /* CON on purpose: rare, human-triggered, delivery matters.
       * NOTE: while this request is in flight (up to the CoAP
       * retransmission timeout) button presses are ignored. */
      COAP_BLOCKING_REQUEST(&dept_eps[target], request,
                            signal_response_handler);

      /* Brief "request sent" feedback, then restore the LED to
       * whatever the signal state requires */
      leds_on(LEDS_RGB_ALL);
      etimer_set(&flash_timer, FEEDBACK_FLASH);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&flash_timer));
      dept_signal_refresh_led();
    }
  }

  PROCESS_END();
}
