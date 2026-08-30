#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"

#include "client.h"

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

PROCESS(er_client, "Assistance request client");
AUTOSTART_PROCESSES(&er_client);

//static patient_info_t patient_info;

//  The client should send an assistance request with the press of a button
//  It currently sends two requests with a timer, one every 2 seconds
static struct etimer et;

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

  //  How many requests to send
  //  %TODO: This variabile is used as long as the assistance requests are sent using the timer
  static int max_requests = 2;

  //  %TODO: For now the id is hardcoded to 1, a patient data structure will have to be added somewhere
  //static int patient_id = patient_info.patient_id;

  PROCESS_BEGIN();

  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */

  coap_endpoint_parse(SERVER_ADDR, strlen(SERVER_ADDR), &server_addr);

  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);

#if PLATFORM_HAS_BUTTON
#if !PLATFORM_SUPPORTS_BUTTON_HAL
  SENSORS_ACTIVATE(button_sensor);
#endif
  printf("Press a button to send an assistance request\n");
#endif /* PLATFORM_HAS_BUTTON */

  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et)) {
      printf("--Toggle timer--\n");

      max_requests--;

      /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
      coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
      coap_set_header_uri_path(request, ASSISTANCE_URI);
      coap_set_header_content_format(request, APPLICATION_JSON);

      //  Build JSON payload
      char msg[40];
      strcpy(msg, "{\"PATIENT_ID\":1,\"TIMESTAMP\":10}");

      coap_set_payload(request, (uint8_t *)msg, strlen(msg));

      LOG_INFO_COAP_EP(&server_addr);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_addr, request, client_chunk_handler);

      printf("\n--Done--\n");

      if(max_requests > 0)
        etimer_reset(&et);

#if PLATFORM_HAS_BUTTON
#if PLATFORM_SUPPORTS_BUTTON_HAL
    } else if(ev == button_hal_release_event) {
      
#endif

      //  %TODO: Modify this part to handle buttons

      
#endif /* PLATFORM_HAS_BUTTON */
    }
  }

  PROCESS_END();
}
