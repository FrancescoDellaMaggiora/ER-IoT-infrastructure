#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "../patient.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

/*
 *  The cloud application informs the patient device that its triage code
 *  changed. This covers both a doctor changing it from the DoctorApp and
 *  the cloud propagating a change decided elsewhere.
 *
 *  Payload JSON format:
 *  {
 *      "TRIAGE_CODE":          <number>
 *  }
 *
 *  The patient id is not carried: the device already knows which patient
 *  it is attached to, and the cloud addresses it directly.
 */

extern device_data_t patient_info;

static void res_put_handler(
    coap_message_t *request,
    coap_message_t *response,
    uint8_t *buffer,
    uint16_t preferred_size,
    int32_t *offset);

RESOURCE(res_triage,
         "title=\"Patient triage update\";rt=\"application/json\"",
         NULL,
         NULL,
         res_put_handler,
         NULL);

static void res_put_handler(
    coap_message_t *request,
    coap_message_t *response,
    uint8_t *buffer,
    uint16_t preferred_size,
    int32_t *offset) {

    //  *---------------------------------------------------------------------------*/
    //  PAYLOAD PARSING

        const uint8_t *payload;
        int payload_len;

        const char *triage_key = "\"TRIAGE_CODE\":";
        const char *start;
        char *end_pointer;

        long triage;

        //  Retrieve the PUT payload
        payload_len = coap_get_payload(request, &payload);

        //  Missing payload
        if(payload_len <= 0) {
            LOG_ERR("Bad triage update: no payload attached\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        //  Too big of a payload
        if(payload_len >= preferred_size) {
            LOG_ERR("Bad triage update: payload too large\n");
            coap_set_status_code(response, REQUEST_ENTITY_TOO_LARGE_4_13);
            return;
        }

        memcpy(buffer, payload, payload_len);
        buffer[payload_len] = '\0';

        LOG_INFO("Triage update received: %s\n", (char*)buffer);

        start = strstr((char *)buffer, triage_key);

        if(start == NULL) {
            LOG_ERR("Bad triage update: TRIAGE_CODE not found\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

        start += strlen(triage_key);

        triage = strtol(start, &end_pointer, 10);

        if(end_pointer == start || triage < CODE_RED || triage > CODE_WHITE) {
            LOG_ERR("Bad triage update: invalid triage code\n");
            coap_set_status_code(response, BAD_REQUEST_4_00);
            return;
        }

    //  PAYLOAD PARSING END
    //  *---------------------------------------------------------------------------*/

    LOG_INFO("Triage code of patient %li: %i -> %li\n",
             patient_info.patient_id, patient_info.triage_code, triage);

    patient_info.triage_code = (triage_code_t)triage;

    coap_set_status_code(response, CHANGED_2_04);
}
