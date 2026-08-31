#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"

#include "client.h"
#include "sys/node-id.h"

#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#endif

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

//  Server address
#define SERVER_ADDR "coap://[fe80::201:1:1:1]"
#define TOGGLE_INTERVAL 2

void init_patient(patient_info_t *patient) {
  patient->patient_id = node_id;
  strcpy(patient->name, "Mario");
  strcpy(patient->surname, "Rossi");
  patient->triage_code = CODE_BLUE;
  patient->timestamp = 10;
}

PROCESS(er_client, "Assistance request client");
AUTOSTART_PROCESSES(&er_client);

//  URI definition
#define ASSISTANCE_URI "/er/patient/assistance"

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
void
client_chunk_handler(coap_message_t *response)
{
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }

  int len = coap_get_payload(response, &chunk);

  printf("|%.*s", len, (char *)chunk);
}

PROCESS_THREAD(er_client, ev, data)
{
  static coap_endpoint_t server_addr;
  static patient_info_t patient_info;

  PROCESS_BEGIN();

  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */


  init_patient(&patient_info);

  coap_endpoint_parse(SERVER_ADDR, strlen(SERVER_ADDR), &server_addr);

#if PLATFORM_HAS_BUTTON
#if !PLATFORM_SUPPORTS_BUTTON_HAL
  SENSORS_ACTIVATE(button_sensor);
#endif
  printf("Press a button to send an assistance request\n");
#endif /* PLATFORM_HAS_BUTTON */

  while(1) {
    PROCESS_YIELD();

#if PLATFORM_HAS_BUTTON
#if PLATFORM_SUPPORTS_BUTTON_HAL
    if(ev == button_hal_release_event) {
      
#endif

      printf("--Button pressed--\n");

      /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
      coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
      coap_set_header_uri_path(request, ASSISTANCE_URI);
      coap_set_header_content_format(request, APPLICATION_JSON);

      //  Build JSON payload
      char msg[40];
      snprintf(msg, sizeof(msg), "{\"PATIENT_ID\":%li,\"TIMESTAMP\":%i}", patient_info.patient_id, node_id);

      coap_set_payload(request, (uint8_t *)msg, strlen(msg));

      LOG_INFO_COAP_EP(&server_addr);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_addr, request, client_chunk_handler);

      printf("\n--Done--\n");

    }

#endif /* PLATFORM_HAS_BUTTON */
    
  }

  PROCESS_END();
}
