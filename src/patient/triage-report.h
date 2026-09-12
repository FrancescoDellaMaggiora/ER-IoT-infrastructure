/*---------------------------------------------------------------------------*/
/*
 * triage-report.h - Reports an autonomous triage-code change to the Cloud
 *
 * When the on-device model decides the patient's triage code has
 * changed, the following CoAP request is sent to the cloud:
 *
 *   PUT coap://<cloud>/er/patient/triage-report
 *   { "PATIENT_ID": <int>, "TRIAGE_CODE": <int 1-5> }
 *
 * The Cloud updates MySQL (its trg_triage_history_update trigger logs
 * the change by itself) and forwards the news to the nurse device. It
 * answers 2.04 Changed BEFORE contacting the nurse, so this node is not
 * kept waiting on a second round-trip.
 *
 * NON-BLOCKING BY DESIGN:
 * this module uses CoAP's callback API (coap_send_request), not
 * COAP_BLOCKING_REQUEST. The blocking macro waits with
 * PROCESS_WAIT_EVENT_UNTIL(), which DISCARDS every event that does not
 * match - inside the patient process it would silently swallow the
 * mqtt-service timers arriving meanwhile, stalling the publish state
 * machine exactly when a deterioration is being reported. With the
 * callback API the call returns immediately and the result arrives
 * later, so everything stays in one process and nothing is lost.
 */
/*---------------------------------------------------------------------------*/
#ifndef TRIAGE_REPORT_H_
#define TRIAGE_REPORT_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include <stdbool.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
/*
 * CoAP endpoint of the Cloud Application, overridable from project-conf.h.
 */
#ifdef TRIAGE_REPORT_CONF_CLOUD_EP
#define TRIAGE_REPORT_CLOUD_EP TRIAGE_REPORT_CONF_CLOUD_EP
#else
#define TRIAGE_REPORT_CLOUD_EP "coap://[fd00:1::1]"
#endif

#define TRIAGE_REPORT_URI "/er/patient/triage-report"
/*---------------------------------------------------------------------------*/
/** Parses the Cloud endpoint. Called once at startup. */
void triage_report_init(void);

/**
 * Sends 'triage_code' for 'patient_id' and RETURNS IMMEDIATELY - the
 * response (or timeout) is handled later in the internal callback.
 *
 * Returns false without sending if the arguments are out of range or if
 * a report is still in flight.
 *
 * 'triage_code' is the numeric code 1-5 of triage_code_t.
 */
bool triage_report_send(long patient_id, uint8_t triage_code);

/** True while a report is in flight. */
bool triage_report_busy(void);
/*---------------------------------------------------------------------------*/
#endif /* TRIAGE_REPORT_H_ */
/*---------------------------------------------------------------------------*/
