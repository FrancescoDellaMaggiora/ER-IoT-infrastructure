#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "coap-engine.h"

#if PLATFORM_SUPPORTS_BUTTON_HAL
  #include "dev/button-hal.h"
#endif

#include "common.h"
#include "nurse-patients.h"

extern coap_resource_t res_association;
extern coap_resource_t res_assistance;
extern coap_resource_t res_obs_status;
extern coap_resource_t res_discharge;
extern coap_resource_t res_triage_update;

PROCESS(er_server, "Nurse CoAP server");
AUTOSTART_PROCESSES(&er_server);

PROCESS_THREAD(er_server, ev, data)
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

      int patients_to_add = DBG_ADD_PATIENT > MAX_PATIENT_NUMBER ? MAX_PATIENT_NUMBER : DBG_ADD_PATIENT;
      int i;

      //  %TODO: This is hard-coded (obviously, this is for debugging purpouses)
      for(i = 1; i <= patients_to_add; i++)
        add_patient(i, "RSSMRA800", "Mario", "Rossi", CODE_BLUE, 1, 1);

      print_patients();

    #endif

  //  DEBUG END
  //  *---------------------------------------------------------------------------*/

  PROCESS_PAUSE();
  
  LOG_INFO("Starting nurse CoAP server\n");

  //  Patient association resource
  //  CLOUD -> NURSE
  coap_activate_resource(&res_association, "er/patient/association");

  //  Patients assistance resource
  //  PATIENT -> NURSE
  coap_activate_resource(&res_assistance, "er/patient/assistance");

  //  Assistance request status observable resouce
  //  NURSE -> PATIENT
  coap_activate_resource(&res_obs_status, "er/patient/assistance/status");

  //  Patien discharge resource
  //  CLOUD -> NURSE
  coap_activate_resource(&res_discharge, "er/patient/discharge");

  //  Patient triage code update resource
  //  CLOUD -> NURSE
  coap_activate_resource(&res_triage_update, "er/patient/triage-update");

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();

    #if PLATFORM_SUPPORTS_BUTTON_HAL

      //  If the button is pressed the request assistance is acknowledged
      if(ev == button_hal_press_event) {
        LOG_INFO("BUTTON PRESSED\n");
        assistance_ack();
      }
      
    #endif

  }                             /* while (1) */

  PROCESS_END();
}
