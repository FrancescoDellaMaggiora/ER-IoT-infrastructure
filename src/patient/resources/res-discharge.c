#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "coap-engine.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

//  The cloud application could inform the patient device that the patient has been discharged.
//  The device will stop performing MQTT and COAP communications.

extern process_event_t discharge_event;
extern struct process patient_process;

//  This variable is used to ignore every request after the first one
static bool patient_discharged = false;

static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_discharge,
         "title=\"Patient discharge\";rt=\"application/json\"",
         NULL,
         NULL,
         res_put_handler,
         NULL);

static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

    if(patient_discharged) {
        LOG_INFO("Patient already discharged, ignoring request.\n");
        coap_set_status_code(response, NOT_FOUND_4_04);
        return;
    }

    LOG_INFO("Discharge request . . .\n");

    patient_discharged = true;

    //  Tell the patient process to stop all communication
    process_post(&patient_process, discharge_event, NULL);

    coap_set_status_code(response, DELETED_2_02);
}