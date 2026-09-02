#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "common.h"
#include "../nurse-patients.h"
#include "../JSON-parser.h"

//  The cloud application will inform the nurse that a new patient has to be associated. The payload will contain patient information
//
//  Payload JSON format:
//  {
//      "PATIENT_ID":           <number>,
//      "SSN":                  <string>,
//      "NAME":                 <string>,
//      "SURNAME":              <string>,
//      "TRIAGE_CODE":          <number>,
//      "RECEPTION_TIMESTAMP":  <timestamp>, 
//      "LAST_VISIT_TIMESTAMP": <timestamp>  
//  }

static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_association,
         "title=\"Patient association\";rt=\"application/json\"",
         NULL,
         res_post_handler,
         NULL,
         NULL);

static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

    //  *---------------------------------------------------------------------------*/
    //  PAYLOAD PARSING

        const uint8_t *payload;
        int payload_len;

        int patient_id;
        int triage_code;
        uint32_t reception_timestamp;
        uint32_t last_visit_timestamp;

        char ssn[SSN_SIZE];
        char name[NAME_SIZE];
        char surname[NAME_SIZE];

        //  Retrieve the POST payload
        payload_len = coap_get_payload(request, &payload);
        
        //  Missing payload
        if(payload_len <= 0) {
            LOG_ERR("Bad association request: no payload attached\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Too big of a payload
        if(payload_len >= REST_MAX_CHUNK_SIZE) {
            LOG_ERR("Bad association request: payload too large\n");
            coap_set_status_code(response, REQUEST_ENTITY_TOO_LARGE_4_13);
            return;
        }

        memcpy(buffer, payload, payload_len);
        buffer[payload_len] = '\0';

        LOG_INFO("Association request received: %s\n", (char*)buffer);
        
        if(parse_association_payload(buffer, payload_len, &patient_id, ssn, sizeof(ssn), name, sizeof(name), surname, sizeof(surname), &triage_code, &reception_timestamp, 
            &last_visit_timestamp) != 0) {

            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Association request of patient %i . . .\n", patient_id);

    int add_outcome = add_patient(patient_id, ssn, name, surname, triage_code, reception_timestamp, last_visit_timestamp);
    
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
            LOG_ERR("Bad association request patient %i: invalid input data\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_PATIENT_ALREADY_ASSOCIATED:
            LOG_ERR("Bad association request patient %i: already associated to this nurse\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_ARRAY_IS_FULL:
            LOG_ERR("Bad association request patient %i: this nurse is already associated to %i patients\n", patient_id, MAX_PATIENT_NUMBER);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_SUCCESS:
            LOG_INFO("Patient %i has been associated\n", patient_id);
            coap_set_status_code(response, CREATED_2_01);
            return;
        
        default:
            LOG_ERR("Bad association request patient %i: uknown error\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
    }

}