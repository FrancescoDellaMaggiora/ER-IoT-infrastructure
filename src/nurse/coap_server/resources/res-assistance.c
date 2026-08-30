#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"
#include "common.h"
#include "../nurse-patients.h"

//  Patients send assistance requests using the POST method. Their PATIENT_ID will be in the payload.
//  Payload JSON format:
//  {
//      "PATIENT_ID":   <number>
//      "TIMESTAMP":    <timestamp>  
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

    int patient_id;
    uint32_t timestamp;

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

        char *id_pointer;
        char *timestamp_pointer;
        char *end_pointer;

        /*-------------------------------------------------------------------------*/
        //  PATIENT_ID

        //  Make "id_pointer" point to the beginning of "PATIENT_ID"
        id_pointer = strstr((char *)buffer, "\"PATIENT_ID\"");

        if(id_pointer == NULL) {
            LOG_ERR("Bad assistance request: PATIENT_ID not found in body\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Find ':' after PATIENT_ID
        id_pointer = strchr(id_pointer, ':');

        if(id_pointer == NULL) {
            LOG_ERR("Bad assistance request: invalid PATIENT_ID field\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Move past ':'
        id_pointer++;

        //  Skip optional whitespace
        while(*id_pointer == ' ' || *id_pointer == '\t' ||
            *id_pointer == '\n' || *id_pointer == '\r') {
            id_pointer++;
        }

        //  Convert PATIENT_ID to integer
        patient_id = strtol(id_pointer, &end_pointer, 10);

        //  No number was found
        if(id_pointer == end_pointer) {
            LOG_ERR("Bad assistance request: invalid patient ID\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Skip optional whitespace after the number
        while(*end_pointer == ' ' || *end_pointer == '\t' ||
            *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

        //  PATIENT_ID must be followed by a comma
        if(*end_pointer != ',') {
            LOG_ERR("Bad assistance request: expected ',' after PATIENT_ID\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        /*-------------------------------------------------------------------------*/
        //  TIMESTAMP

        //  Move past ','
        end_pointer++;

        //  Skip optional whitespace
        while(*end_pointer == ' ' || *end_pointer == '\t' ||
            *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

        //  Find TIMESTAMP after PATIENT_ID
        timestamp_pointer = strstr(end_pointer, "\"TIMESTAMP\"");

        if(timestamp_pointer == NULL) {
            LOG_ERR("Bad assistance request: TIMESTAMP not found in body\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Find ':' after TIMESTAMP
        timestamp_pointer = strchr(timestamp_pointer, ':');

        if(timestamp_pointer == NULL) {
            LOG_ERR("Bad assistance request: invalid TIMESTAMP field\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Move past ':'
        timestamp_pointer++;

        //  Skip optional whitespace
        while(*timestamp_pointer == ' ' || *timestamp_pointer == '\t' ||
            *timestamp_pointer == '\n' || *timestamp_pointer == '\r') {
            timestamp_pointer++;
        }

        //  Convert TIMESTAMP to unsigned integer
        {
            unsigned long parsed_timestamp;

            parsed_timestamp = strtoul(timestamp_pointer, &end_pointer, 10);

            //  No number was found
            if(timestamp_pointer == end_pointer) {
                LOG_ERR("Bad assistance request: invalid timestamp\n");
                coap_set_status_code(response, BAD_REQUEST_4_00);
                return;
            }

            //  Check that timestamp fits in uint32_t
            if(parsed_timestamp > UINT32_MAX) {
                LOG_ERR("Bad assistance request: timestamp too large\n");
                coap_set_status_code(response, BAD_REQUEST_4_00);
                return;
            }

            timestamp = (uint32_t)parsed_timestamp;
        }

        //  Skip optional whitespace after TIMESTAMP
        while(*end_pointer == ' ' || *end_pointer == '\t' ||
            *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

        //  TIMESTAMP must be followed by the closing brace
        if(*end_pointer != '}') {
            LOG_ERR("Bad assistance request: expected '}' after TIMESTAMP\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Move past '}'
        end_pointer++;

        //  Only whitespace is allowed after the closing brace
        while(*end_pointer == ' ' || *end_pointer == '\t' ||
            *end_pointer == '\n' || *end_pointer == '\r') {
            end_pointer++;
        }

        //  Nothing else should be present
        if(*end_pointer != '\0') {
            LOG_ERR("Bad assistance request: invalid data after JSON body\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Assistance request by patient: %i\n", patient_id);

    int add_outcome = add_request(patient_id, timestamp);
    
    //  *---------------------------------------------------------------------------*/
    //  DEBUG

        #if NURSE_DBG == 1

            print_patients();
            print_requests();

        #endif

    //  DEBUG END
    //  *---------------------------------------------------------------------------*/

    switch (add_outcome) {

        case RESULT_INVALID_INPUT:
            LOG_ERR("Bad assistance request patient %i: invalid input data\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_PATIENT_NOT_ASSOCIATED:
            LOG_ERR("Bad assistance request patient %i: not associated to this nurse\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_PATIENT_ALREADY_PENDING:
            LOG_ERR("Bad assistance request patient %i: they already have a pending request\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_SUCCESS:
            LOG_INFO("Assistance request by patient %i has been reported\n", patient_id);
            coap_set_status_code(response, CHANGED_2_04);
            return;
        
        default:
            LOG_ERR("Bad assistance request patient %i: uknown error\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
    }

}