#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"

#include "common.h"
#include "nurse-patients.h"
#include "res-obs-status.h"

#if PLATFORM_SUPPORTS_BUTTON_HAL
  #include "dev/button-hal.h"
#endif

#ifdef PLATFORM_HAS_LEDS
    #include "dev/leds.h"
#endif

//  Necessary to trigger the notification of the observable resource
extern coap_resource_t res_obs_status;

//  When an assistance request is acknowledged the head of the request queue has to be removed and the led has to be turned off
void assistance_ack() {

    request_data_t request = get_next_request();

    if(request.patient_id == -1) {
        LOG_ERR("assistance_ack(): request queue's head retreival error\n");
        return;
    }

    LOG_INFO("Assistance acknowledged: patient %li\n", request.patient_id);

    set_status_notification(request.patient_id, STATUS_IDLE);
    res_obs_status.trigger();

}