#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

//  Patients send assistance requests using the POST method. Their PATIENT_ID will be in the payload.
//  Payload JSON format:
//  {
//      "PATIENT_ID": <number>
//  }

static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_assistance,
         "title=\"Patient assistance\";rt=\"application/json\"",
         NULL,
         res_post_handler,
         NULL,
         NULL);

static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload;
    int payload_len;

    char *id_pointer;
    long patient_id;

    //  Needed by the strtol function
    char *end_pointer;

    //  Retrieve the POST payload
    payload_len = coap_get_payload(request, &payload);
    
    //  Missing payload
    if(payload_len <= 0) {
        LOG_ERR("Bad assistance request: no payload attached\n");
        coap_set_status_code(response, BAD_REQUEST_4_00);
        return;
    }

    //  Too big of a payload
    if(payload_len >= REST_MAX_CHUNK_SIZE) {
        LOG_ERR("Bad assistance request: payload too large\n");
        coap_set_status_code(response, REQUEST_ENTITY_TOO_LARGE_4_13);
        return;
    }

    //  Buffer should be use to build the POST response
    memcpy(buffer, payload, payload_len);
    buffer[payload_len] = '\0';

    LOG_INFO("Assistance request received: %s\n", (char*)buffer);

    //  *---------------------------------------------------------------------------*/
    //  PAYLOAD PARSING
    //  %TODO: Finish it

        //  Make "id_pointer" point to the 'D' of "PATIENT_ID"
        id_pointer = strstr((char*)buffer, "\"PATIENT_ID\"");

        if(id_pointer == NULL) {
            LOG_ERR("Bad assistance request: PATIENT_ID not found in body\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Find ':' after PATIENT_ID
        //  Body example: {"PATIENT_ID":1}
        
        id_pointer = strchr(id_pointer, ':');

        if(id_pointer == NULL) {
            LOG_ERR("Bad assistance request: invalid PATIENT_ID field\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Move past ':' 
        id_pointer++;

        //  Convert char ID to integer using the strtol function (string to long, better than atoi)
        patient_id = strtol(id_pointer, &end_pointer, 10);

        //  No number was found
        if(id_pointer == end_pointer) {
            LOG_ERR("Bad assistance request: invalid patient ID\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Skip optional whitespace after the number 
        while(*end_pointer == ' ' || *end_pointer == '\t' || *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

        //  The number must be followed by the closing brace 
        if(*end_pointer != '}') {
            LOG_ERR("Bad assistance request: invalid characters after patient ID\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Skip the closing brace 
        end_pointer++;

        //  Only whitespace is allowed after the closing brace 
        while(*end_pointer == ' ' || *end_pointer == '\t' || *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    //  Nothing else should be present 
    if(*end_pointer != '\0') {
        LOG_ERR("Bad assistance request: invalid data after JSON body\n");
        coap_set_status_code(response, BAD_REQUEST_4_00);
        return;
    }

    LOG_INFO("Assistance request by patient: %ld\n", patient_id);

    if(!nurse_patient_is_associated()) {
        LOG_ERR("Bad assistance request: patient is not associated to this nurse\n");
        coap_set_status_code(response, BAD_REQUEST_4_00);
    }

    coap_set_status_code(response, CHANGED_2_04);
}