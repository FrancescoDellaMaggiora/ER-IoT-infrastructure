#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"

#include "sensor.h"
#include "sys/node-id.h"

#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#endif

#include "dev/leds.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

//  DEVICE_ID (default is 1) used to construct the registration request.
//  It is learnt at build time:
//    make TARGET=<target> DEVICE_ID=<number>

#ifndef DEVICE_ID
#define DEVICE_ID 1
#endif

//  Server address
//  %TODO: Use the correct address
#define SERVER_ADDR "coap://[fe80::201:1:1:1]"

/*
    After bootstrap, the device sends a GET request to retreive the info of the patient it has been attached to.

    More specifically, this device's ID will be associated to a patient's ID in the database. 
    The sensor sends a GET request to "/er/patient/registration/<DEVICE_ID>". 

    The answer will be a JSON payload containing the following information:
    {
        "PATIENT_ID":       <number>,
        "TRIAGE_CODE":      <code>,
        "NURSE_ADDRESS":    <IPv6 address>
    }
*/

/*---------------------------------------------------------------------------*/
//  URIs DEFINITION

    //  Registration
    #define URI_SIZE 64

/*---------------------------------------------------------------------------*/

static char REGISTRATION_URI[URI_SIZE];

static coap_endpoint_t server_addr;
static coap_endpoint_t nurse_addr;

static device_data_t patient_info;

PROCESS(er_client, "Assistance request client");
AUTOSTART_PROCESSES(&er_client);

static struct etimer et;

/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

    //  Extract the target patient id and the request status from the observable resource notification
    int parse_registration(const char *payload, int payload_len, long *patient_id, triage_code_t *triage_code, coap_endpoint_t *nurse_address) {

        const char *id_key = "\"PATIENT_ID\":";
        const char *triage_key = "\"TRIAGE_CODE\":";
        const char *nurse_key = "\"NURSE_ADDRESS\":\"";

        const char *start;
        char *endptr;

        long id;
        long triage;

        if(payload == NULL || patient_id == NULL || triage_code == NULL || nurse_address == NULL) {
            return -1;
        }

        /*--------------------------------------------------*/
        /* PATIENT_ID */

        start = strstr(payload, id_key);

        if(start == NULL) {
            return -1;
        }

        start += strlen(id_key);

        id = strtol(start, &endptr, 10);

        if(endptr == start || id <= 0) {
            return -1;
        }

        /*--------------------------------------------------*/
        /* TRIAGE_CODE */

        start = strstr(payload, triage_key);

        if(start == NULL) {
            return -1;
        }

        start += strlen(triage_key);

        triage = strtol(start, &endptr, 10);

        if(endptr == start || triage < CODE_RED || triage > CODE_WHITE) {
            return -1;
        }

        /*--------------------------------------------------*/
        /* NURSE_ADDRESS */

        start = strstr(payload, nurse_key);

        if(start == NULL) {
            return -1;
        }

        start += strlen(nurse_key);

        const char *end = strchr(start, '"');

        if(end == NULL) {
            return -1;
        }

        /* Temporarily copy the IPv6 address */
        char address[64];

        int address_len = end - start;

        if(address_len <= 0 || address_len >= sizeof(address)) {
            return -1;
        }

        memcpy(address, start, address_len);
        address[address_len] = '\0';

        /* coap_endpoint_parse() expects a URI */
        char uri[64 + 20];

        snprintf(uri, sizeof(uri), "coap://[%s]", address);

        if(coap_endpoint_parse(uri, strlen(uri), nurse_address) < 0) {
            return -1;
        }

        /*--------------------------------------------------*/

        *patient_id = id;
        *triage_code = (triage_code_t)triage;

        return 0;

    }      

    //  Printing function
    void print_patient_info() {
        printf("Patient ID: %li\t Triage code: %i\n", patient_info.patient_id, patient_info.triage_code);
    }

//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS

    //  This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses
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

        char payload[128];

        if(len <= 0 || len >= sizeof(payload)) {
            printf("Invalid payload\n");
            return;
        }

        memcpy(payload, chunk, len);
        payload[len] = '\0';

        if(parse_registration(payload, len, &patient_info.patient_id, &patient_info.triage_code, &nurse_addr) == 0) {
            printf("Device successfully registered\n");
            print_patient_info();
        } 
        
        else 
            printf("Patient info parsing error\n");
        
    }

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



PROCESS_THREAD(er_client, ev, data) {

    PROCESS_BEGIN();

    //  The device tries to register after 1 second it's on
    etimer_set(&et, 1 * CLOCK_SECOND);
    
    //  Set the resource URI /er/patient/registration/<DEVICE_ID>
    snprintf(REGISTRATION_URI, sizeof(REGISTRATION_URI), "/er/patient/registration/%i", DEVICE_ID);

    static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */

    coap_endpoint_parse(SERVER_ADDR, strlen(SERVER_ADDR), &server_addr);

    memset(&patient_info, 0, sizeof(patient_info));

    while(1) {

        PROCESS_YIELD();
        
        if(etimer_expired(&et)) {

            printf("--Timer expired--\n");

            /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
            coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
            coap_set_header_uri_path(request, REGISTRATION_URI);

            LOG_INFO_COAP_EP(&server_addr);
            LOG_INFO_("\n");

            COAP_BLOCKING_REQUEST(&server_addr, request, client_chunk_handler);

            printf("\n--Registration request sent--\n");

        }

    }

    PROCESS_END();
}