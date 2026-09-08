#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"

#include "common.h"
#include "res-obs-status.h"
#include "../nurse-patients.h"

//  Assistance requests will be aknowledged and their change of status will be notified to patients with this observable resource
//  Payload JSON format:
//  {
//      "PATIENT_ID":   <number>,
//      "STATUS":       <status>  
//  }

//  Information that need to be notified to patients (i.e. last request that was aknowledged)
static long notification_patient_id = -1;
static patient_status_t notification_status = STATUS_IDLE;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_obs_status_event_handler();

EVENT_RESOURCE(res_obs_status,
        "title=\"Request status\";rt=\"application/json\"",
        res_get_handler,
        NULL,
        NULL,
        NULL,
        res_obs_status_event_handler);

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    int len;

    len = snprintf((char *)buffer, preferred_size, "{\"PATIENT_ID\":%ld,\"STATUS\":%d}", notification_patient_id, notification_status);

    if(len < 0 || len >= preferred_size) {
        coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
        return;
    }

    coap_set_header_content_format(response, APPLICATION_JSON);
    coap_set_payload(response, buffer, len);
    coap_set_status_code(response, CONTENT_2_05);
}

static void res_obs_status_event_handler() {
    coap_notify_observers(&res_obs_status);
}

void set_status_notification(long patient_id, patient_status_t status) {
    notification_patient_id = patient_id;
    notification_status = status;
}