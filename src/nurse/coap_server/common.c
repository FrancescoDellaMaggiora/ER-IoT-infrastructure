#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "dev/button-hal.h"

#include "common.h"
#include "nurse-patients.h"

#ifdef PLATFORM_HAS_LEDS
    #include "dev/leds.h"
#endif

//  When a request is accepted the led is turned on
//  %TODO: Adjust this function based on how we want the leds to work
void notify_request_led() {
    #ifdef PLATFORM_HAS_LEDS
        leds_on(LEDS_TO_NUM_MASK(LEDS_GREEN));
    #endif
}

//  When an assistance request is acknowledged the head of the request queue has to be removed and the led has to be turned off
void assistance_ack() {

    //  %TODO: Does this need to be returned? It depends on how leds are handled
    request_data_t request = get_next_request();

    if(request.patient_id == -1) {
        LOG_ERR("assistance_ack(): retreival error of the request queue's head\n");
        return;
    }

    LOG_INFO("Assistance acknowledged: patient %li\n", request.patient_id);

    //  %TODO: Adjust based on how we want the leds to work
    #ifdef PLATFORM_HAS_LEDS
        leds_off(LEDS_TO_NUM_MASK(LEDS_GREEN));
    #endif
}