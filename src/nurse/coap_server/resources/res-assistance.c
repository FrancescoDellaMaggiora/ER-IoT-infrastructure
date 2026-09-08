#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "common.h"
#include "../nurse-patients.h"
#include "../JSON-parser.h"

//  Patients send assistance requests using the POST method. Their PATIENT_ID will be in the payload.
//  Payload JSON format:
//  {
//      "PATIENT_ID":   <number>,
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

    //  *---------------------------------------------------------------------------*/
    //  PAYLOAD PARSING

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

        memcpy(buffer, payload, payload_len);
        buffer[payload_len] = '\0';

        LOG_INFO("Assistance request received: %s\n", (char*)buffer);

        if(parse_assistance_payload(buffer, payload_len, &patient_id, &timestamp) != 0) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Assistance request by patient %i . . .\n", patient_id);

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

            /*
                Response payload constrcution.
                The response payload must contain the led color associated to the new patient:

                {"LED_COLOR":   <color>}
            */

            char color[10];
            
            if(get_patient_color_name(patient_id, color) != 1) {
                LOG_ERR("Error patient: %i: unable to retrieve LED color\n", patient_id);
                coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
                return;
            }

            int len = snprintf((char *)buffer, preferred_size, "{\"LED_COLOR\":\"%s\"}", color);

            if(len < 0 || len >= preferred_size) {
                LOG_ERR("Error patient %i: invalid response payload size\n", patient_id);
                coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
                return;
            }

            coap_set_header_content_format(response, APPLICATION_JSON);
            coap_set_payload(response, buffer, len);
            coap_set_status_code(response, CHANGED_2_04);
            return;
        
        default:
            LOG_ERR("Bad assistance request patient %i: uknown error\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
    }

}