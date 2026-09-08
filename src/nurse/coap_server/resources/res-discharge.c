#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "common.h"
#include "../nurse-patients.h"
#include "../JSON-parser.h"

//  The cloud application could inform the nurse that a patient needs to be dissociated. The payload will only contain the patient ID.
//  Payload JSON format:
//  {
//      "PATIENT_ID":           <number>
//  }

static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_discharge,
         "title=\"Patient discharge\";rt=\"application/json\"",
         NULL,
         NULL,
         res_put_handler,
         NULL);

static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

    //  *---------------------------------------------------------------------------*/
    //  PAYLOAD PARSING

        const uint8_t *payload;
        int payload_len;

        int patient_id;

        //  Retrieve the PUT payload
        payload_len = coap_get_payload(request, &payload);
        
        //  Missing payload
        if(payload_len <= 0) {
            LOG_ERR("Bad discharge request: no payload attached\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Too big of a payload
        if(payload_len >= REST_MAX_CHUNK_SIZE) {
            LOG_ERR("Bad discharge request: payload too large\n");
            coap_set_status_code(response, REQUEST_ENTITY_TOO_LARGE_4_13);
            return;
        }

        memcpy(buffer, payload, payload_len);
        buffer[payload_len] = '\0';

        LOG_INFO("Discharge request received: %s\n", (char*)buffer);
        
        if(parse_discharge_payload(buffer, &patient_id) != 0) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Discharge request of patient %i . . .\n", patient_id);

    int remove_outcome = remove_patient(patient_id);
    
    //  *---------------------------------------------------------------------------*/
    //  DEBUG

        #if NURSE_DBG == 1

            print_patients();
            print_requests();

        #endif

    //  DEBUG END
    //  *---------------------------------------------------------------------------*/

    switch (remove_outcome) {

        case RESULT_INVALID_INPUT:
            LOG_ERR("Bad discharge request patient %i: invalid input data\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_PATIENT_NOT_ASSOCIATED:
            LOG_ERR("Bad discharge request patient %i: was not associated to this nurse\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_SUCCESS:
            LOG_INFO("Patient %i has been removed\n", patient_id);
            coap_set_status_code(response, DELETED_2_02);
            return;
        
        default:
            LOG_ERR("Bad discharge request patient %i: uknown error\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
    }

}