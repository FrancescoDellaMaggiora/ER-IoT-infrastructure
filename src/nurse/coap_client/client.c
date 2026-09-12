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

#include "dev/leds.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

//  Server address
#define SERVER_ADDR "coap://[fe80::201:1:1:1]"

#define REMOTE_PORT UIP_HTONS(COAP_DEFAULT_PORT)

/*---------------------------------------------------------------------------*/
//  URIs DEFINITION

  //  Assistance
  #define ASSISTANCE_URI "/er/patient/assistance"

  //  Observable assistance request status
  #define OBS_STATUS_URI "/er/patient/assistance/status"

/*---------------------------------------------------------------------------*/

static coap_endpoint_t server_addr;
static coap_observee_t *request_status;
static process_event_t stop_observation_event;

PROCESS(er_client, "Assistance request client");
AUTOSTART_PROCESSES(&er_client);

/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

  //  Debug function
  void init_patient(patient_data_t *patient) {

    patient->patient_id = node_id;

    strcpy(patient->SSN, "RSSMRA800");
    strcpy(patient->name, "Mario");
    strcpy(patient->surname, "Rossi");

    patient->triage_code = CODE_BLUE;
    patient->reception_timestamp = 1;
    patient->last_visit_timestamp = 1;

  }

  //  Extract the color name from the payload received after an assistance request
  int parse_led_color(const char *payload, int payload_len, char *color, int color_size) {
    const char *key = "\"LED_COLOR\":\"";
    const char *start;
    const char *end;
    int len;

    if(payload == NULL || color == NULL || color_size <= 0) {
        return -1;
    }

    //  Find the LED_COLOR key
    start = strstr(payload, key);

    if(start == NULL) {
        return -1;
    }

    //  Move to the beginning of the value
    start += strlen(key);

    //  Look for the end of the value
    end = strchr(start, '"');

    if(end == NULL) {
        return -1;
    }

    len = end - start;

    //  Check is buffer size is sufficient
    if(len >= color_size) {
        return -1;
    }

    //  Copy the name color
    memcpy(color, start, len);
    color[len] = '\0';

    return 0;

  }

  //  Extract the target patient id and the request status from the observable resource notification
  int parse_status(const char *payload, int payload_len, long *patient_id, patient_status_t *status) {

    const char *id_key = "\"PATIENT_ID\":";
    const char *status_key = "\"STATUS\":";
    const char *start;

    if(payload == NULL || patient_id == NULL || status == NULL) {
        return -1;
    }

    start = strstr(payload, id_key);
    if(start == NULL) {
        return -1;
    }

    start += strlen(id_key);
    *patient_id = atol(start);

    start = strstr(payload, status_key);
    if(start == NULL) {
        return -1;
    }

    start += strlen(status_key);
    *status = (patient_status_t)atoi(start);

    return 0;

}

//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  LEDS HANDLING FUNCTIONS

  //  Led colors are treated as strings, this funciton converts them to masks. 
  //  The correct mask is platform dependent, this funciton maps the colors to the Nordic nRF52840 Dongle masks
  int color_to_led_mask(char *color) {

      if(strcmp(color, "RED") == 0) {
          return LEDS_RED;
      }
      else if(strcmp(color, "GREEN") == 0) {
          return LEDS_GREEN;
      }
      else if(strcmp(color, "BLUE") == 0) {
          return LEDS_BLUE;
      }
      else if(strcmp(color, "YELLOW") == 0) {
          return LEDS_RED | LEDS_GREEN;
      }
      else if(strcmp(color, "MAGENTA") == 0) {
          return LEDS_RED | LEDS_BLUE;
      }
      else if(strcmp(color, "CYAN") == 0) {
          return LEDS_GREEN | LEDS_BLUE;
      }
      else if(strcmp(color, "WHITE") == 0) {
          return LEDS_RED | LEDS_GREEN | LEDS_BLUE;
      }

      return 0;

  }

  //  Turn on the led with the specified color
  void update_leds(char *color) {

    leds_off(LEDS_ALL);
    leds_on(color_to_led_mask(color));

  }

//  LEDS HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS

  //  This function will be passed to COAP_BLOCKING_REQUEST() to handle responses
  void client_chunk_handler(coap_message_t *response) {

    const uint8_t *chunk;
    uint8_t status;
    int len;

    if(response == NULL) {
      puts("Request timed out");
      return;
    }

    status = response->code;
    printf("Response: %u.%02u\n", status / 32, status % 32);

    len = coap_get_payload(response, &chunk);

    char payload[64];
    char color[10];

    if(len <= 0 || len >= sizeof(payload)) {
      printf("Invalid payload\n");
      return;
    }

    memcpy(payload, chunk, len);
    payload[len] = '\0';

    if(parse_led_color(payload, len, color, sizeof(color)) == 0) {
      printf("LED color: %s\n", color);
      update_leds(color);
      start_observation();
    } 
    
    else 
      printf("LED_COLOR parsing error\n");
    
  }

  /*
    Suppose a client (patient) performs an assistance request.
    The client observes the /er/patient/assistance/status resource.

    As soon as the status of a request changes (i.e. the nurse aknowledges it), a notification is sent to all the clients observing 
    this resource (i.e. the clients that have a pending request).

    The notification's payload has the following format:
    {
      "PATIENT_ID": <id>,
      "STATUS":     <status>
    }
    
    Patients can understand if their request has been aknowledged based on the "PATIENT_ID" field
  */
  static void notification_callback(coap_observee_t *obs, void *notification, coap_notification_flag_t flag) {

    int len = 0;
    const uint8_t *payload = NULL;

    printf("Notification handler\n");

    if(obs != NULL) {
        printf("Observee URI: %s\n", obs->url);
    }

    if(notification) {
      len = coap_get_payload(notification, &payload);
    }

    switch(flag) {

      case OBSERVE_OK: 
        printf("Observation registered\n");
        break;

      case NOTIFICATION_OK: 
        long patient_id;
        patient_status_t status;

        char payload_buf[64];

        if(payload == NULL || len <= 0 || len >= sizeof(payload_buf)) {
            printf("Invalid notification payload\n");
            break;
        }

        memcpy(payload_buf, payload, len);
        payload_buf[len] = '\0';

        if(parse_status(payload_buf, len, &patient_id, &status) != 0) {
            printf("Invalid notification payload\n");
            break;
        }

        printf("Notification: Patient ID = %ld\t Status=0x%02X\n", patient_id, status);

        //  Process the notification only if it refers to this patient
        if(patient_id == node_id && status == STATUS_IDLE) {

            printf("Request acknowledged\n");

            leds_off(LEDS_ALL);

            //  It's better to modify the observer outside of the notification function
            //stop_observation();
            process_post(&er_client, stop_observation_event, NULL);
        }

        break;

      case OBSERVE_NOT_SUPPORTED:
        printf("Observe not supported\n");
        request_status = NULL;
        break;

      case ERROR_RESPONSE_CODE:
        printf("ERROR_RESPONSE_CODE: %*s\n", len, (char *)payload);
        request_status = NULL;
        break;

      case NO_REPLY_FROM_SERVER:
        if(obs != NULL) {
          printf("NO_REPLY_FROM_SERVER: "
                "removing observe registration with token %x%x\n",
                obs->token[0], obs->token[1]);
        }
        request_status = NULL;
        break;

    }

  }

  //  Start/stop the observation of the remote resource
  void start_observation(void) {

    if(request_status == NULL) {
      printf("Starting observation\n"); 
      request_status = coap_obs_request_registration(&server_addr, OBS_STATUS_URI, notification_callback, NULL);
    }

    if(request_status == NULL) {
      printf("ERROR: observation registration failed\n");
    }

  }

  void stop_observation(void) {

    if(request_status) {
      printf("Stopping observation\n");
      coap_obs_remove_observee(request_status);
      request_status = NULL;
    } 

  }

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



PROCESS_THREAD(er_client, ev, data) {

  static patient_data_t patient_info;

  PROCESS_BEGIN();

  //  Used to stop the observation of the request status when it gets aknowledged
  stop_observation_event = process_alloc_event();

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

    if(ev == stop_observation_event) {
        stop_observation();
        continue;
    }

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

      printf("\n--Request sent--\n");

    }

#endif /* PLATFORM_HAS_BUTTON */
    
  }

  PROCESS_END();
}
