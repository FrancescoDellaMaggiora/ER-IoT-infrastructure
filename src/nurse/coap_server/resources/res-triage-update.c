#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "common.h"
#include "../nurse-patients.h"
#include "../JSON-parser.h"

//  The cloud application informs the nurse that a patient's triage code changed.
//  This covers BOTH cases: a doctor changing it from the DoctorApp, and the
//  patient device reporting an autonomous change decided by its on-board model.
//  From this device's point of view they are the same event.
//
//  Payload JSON format:
//  {
//      "PATIENT_ID":           <number>,
//      "TRIAGE_CODE":          <number>
//  }

static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_triage_update,
         "title=\"Patient triage update\";rt=\"application/json\"",
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
        int triage_code;

        //  Retrieve the PUT payload
        payload_len = coap_get_payload(request, &payload);

        //  Missing payload
        if(payload_len <= 0) {
            LOG_ERR("Bad triage update: no payload attached\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Too big of a payload
        if(payload_len >= REST_MAX_CHUNK_SIZE) {
            LOG_ERR("Bad triage update: payload too large\n");
            coap_set_status_code(response, REQUEST_ENTITY_TOO_LARGE_4_13);
            return;
        }

        memcpy(buffer, payload, payload_len);
        buffer[payload_len] = '\0';

        LOG_INFO("Triage update received: %s\n", (char*)buffer);

        if(parse_triage_update_payload(buffer, payload_len, &patient_id, &triage_code) != 0) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Triage update of patient %i to code %i . . .\n", patient_id, triage_code);

    int update_outcome = update_patient_triage(patient_id, triage_code);

    //  *---------------------------------------------------------------------------*/
    //  DEBUG

        #if NURSE_DBG == 1

            print_patients();
            print_requests();

        #endif

    //  DEBUG END
    //  *---------------------------------------------------------------------------*/

    switch (update_outcome) {

        case RESULT_INVALID_INPUT:
            LOG_ERR("Bad triage update patient %i: invalid input data\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_PATIENT_NOT_ASSOCIATED:
            LOG_ERR("Bad triage update patient %i: not associated to this nurse\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;

        case RESULT_SUCCESS:
            LOG_INFO("Patient %i triage code updated to %i\n", patient_id, triage_code);
            coap_set_status_code(response, CHANGED_2_04);
            return;

        default:
            LOG_ERR("Bad triage update patient %i: uknown error\n", patient_id);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
    }

}