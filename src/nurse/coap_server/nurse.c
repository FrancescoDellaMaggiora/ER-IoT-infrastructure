#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "coap-engine.h"
#include "dev/button-hal.h"

#include "common.h"
#include "nurse-patients.h"

extern coap_resource_t res_assistance;

PROCESS(er_example_server, "Nurse CoAP server");
AUTOSTART_PROCESSES(&er_example_server);

PROCESS_THREAD(er_example_server, ev, data)
{
  PROCESS_BEGIN();

  button_hal_init();
  init_arrays();

  //  *---------------------------------------------------------------------------*/
  //  DEBUG

    //  Array printing
    #if NURSE_DBG == 1

      LOG_DBG("NURSE_DBG = %i\n", NURSE_DBG);
      LOG_DBG("DBG_ADD_PATIENT = %i\n", DBG_ADD_PATIENT);

    #endif

    /*
      Add <DBG_ADD_PATIENT> patients to the nurse array.
      Normally this would be done statically at triage.
      The cloud would communicate this info to the device via another CoAP resource exposed by this very server.
    */
    #if DBG_ADD_PATIENT > 0

      int i;

      for(i = 1; i <= DBG_ADD_PATIENT; i++)
        add_patient(i, CODE_BLUE, 1);

      print_patients();

    #endif

  //  DEBUG END
  //  *---------------------------------------------------------------------------*/

  PROCESS_PAUSE();
  
  LOG_INFO("Starting nurse CoAP server\n");

  coap_activate_resource(&res_assistance, "er/patient/assistance");

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();

    //  If the button is pressed the request assistance is acknowledged
    if(ev == button_hal_press_event) {
      LOG_INFO("BUTTON PRESSED\n");
      assistance_ack();
    }

  }                             /* while (1) */

  PROCESS_END();
}
