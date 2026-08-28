#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "dev/button-hal.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#include "nurse-patients.h"

//  ALESSANDRO: I included the correct resources
extern coap_resource_t res_assistance;

PROCESS(er_example_server, "Nurse CoAP server");
AUTOSTART_PROCESSES(&er_example_server);

PROCESS_THREAD(er_example_server, ev, data)
{
  PROCESS_BEGIN();

  PROCESS_PAUSE();

  init_arrays();
  
  //  DEBUG
  //  Add patient 1 to test the CoAP server
  //  Normally this would be done statically at triage
  //  The cloud would communicate this info to the device via another CoAP resource exposed by this very server
  //  
  //  Uncomment this line below to add patient 1
  //  add_patient(1, CODE_BLUE, 1);

  LOG_INFO("Starting nurse CoAP server\n");

  /*
   * Bind the resources to their Uri-Path.
   * WARNING: Activating twice only means alternate path, not two instances!
   * All static variables are the same for each URI path.
   */
  coap_activate_resource(&res_assistance, "er/patient/assistance");

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  }                             /* while (1) */

  PROCESS_END();
}
