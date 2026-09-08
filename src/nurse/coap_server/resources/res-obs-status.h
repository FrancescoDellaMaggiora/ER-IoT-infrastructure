#include "coap-engine.h"
#include "nurse-patients.h"

extern coap_resource_t res_obs_status;

void set_status_notification(long, patient_status_t);