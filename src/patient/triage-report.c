/*---------------------------------------------------------------------------*/
/*
 * triage-report.c - Reports an autonomous triage-code change (implementation)
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "coap-engine.h"
#include "coap-callback-api.h"

#include "triage-report.h"

#include <stdio.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "triage-rep"
#ifdef PATIENT_CONF_LOG_LEVEL
#define LOG_LEVEL PATIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
/*---------------------------------------------------------------------------*/
/* Payload: {"PATIENT_ID":2147483647,"TRIAGE_CODE":5} is 45 chars, so 64
 * leaves some margin without being wasteful. */
#define PAYLOAD_BUF_SIZE 64

/*
 * ALL of these must be static, not locals.
 *
 * With the callback API the call returns before the packet has even
 * been transmitted: the CoAP engine keeps referring to the request
 * message, the payload buffer and the request state until the exchange
 * ends. Locals would go out of scope immediately and the engine would
 * read freed stack.
 */
static coap_endpoint_t cloud_ep;
static coap_message_t request[1];
static coap_callback_request_state_t  request_state;
static char payload_buf[PAYLOAD_BUF_SIZE];

/* Patient id of the report currently in flight, kept for the log line
 * in the callback. */
static long inflight_patient_id;

static bool busy;
/*---------------------------------------------------------------------------*/
/*
 * Called by the CoAP engine when the exchange ends, from the engine's
 * own context - no waiting, no event stealing.
 */
static void report_callback(coap_callback_request_state_t  *state)
{

  coap_request_state_t *request_state = &state->state;
  coap_message_t *response = request_state->response;

  switch(request_state->status) {
  
    case COAP_REQUEST_STATUS_RESPONSE:      

      LOG_INFO("Cloud replied %u.%02u\n",
              response->code >> 5, response->code & 0x1F);

      /* 4.04 means the Cloud does not know this PATIENT_ID: almost always
      * the build-time PATIENT_ID was sent instead of the one learnt at
      * bootstrap. Worth an explicit message, it is easy to misdiagnose. */
      if(response->code == NOT_FOUND_4_04) {
        LOG_ERR("Cloud does not know patient %ld - wrong id?\n",
                inflight_patient_id);
      }
      break;
    
    case COAP_REQUEST_STATUS_TIMEOUT:
      /* Timed out after the CoAP retransmissions. Nothing is retried here
      * on purpose: a stale retry could overwrite a newer code decided in
      * the meantime, and the Cloud keeps receiving the vitals anyway. */
      LOG_WARN("Triage report timed out\n");
      break;

    default:
      LOG_WARN("Triage report ended with status %u\n", request_state->status);
      break;
  }

  /* Released only here, not when coap_send_request() returns: the
   * buffers above stay in use for the whole exchange. */
  busy = false;
}
/*---------------------------------------------------------------------------*/
void
triage_report_init(void)
{
  if(coap_endpoint_parse(TRIAGE_REPORT_CLOUD_EP,
                         strlen(TRIAGE_REPORT_CLOUD_EP), &cloud_ep) == 0) {
    LOG_ERR("Invalid Cloud endpoint: %s\n", TRIAGE_REPORT_CLOUD_EP);
  }

  busy = false;
  LOG_INFO("Triage report client ready, cloud=%s\n", TRIAGE_REPORT_CLOUD_EP);
}
/*---------------------------------------------------------------------------*/
bool
triage_report_busy(void)
{
  return busy;
}
/*---------------------------------------------------------------------------*/
bool
triage_report_send(long patient_id, uint8_t triage_code)
{
  if(triage_code < 1 || triage_code > 5) {
    LOG_ERR("Refusing to report out-of-range triage code %u\n", triage_code);
    return false;
  }

  if(patient_id <= 0) {
    /* Bootstrap has not completed yet: the node does not know which
     * patient it is attached to, so a report would be meaningless. */
    LOG_WARN("No patient id yet, report dropped\n");
    return false;
  }

  if(busy) {
    /* DROP, do not queue. The static buffers above belong to the
     * exchange in flight, and a CoAP request can live for tens of
     * seconds through its retransmissions;
     */
    LOG_WARN("A report is already in flight, dropping code %u\n", triage_code);
    return false;
  }

  inflight_patient_id = patient_id;

  snprintf(payload_buf, sizeof(payload_buf),
           "{\"PATIENT_ID\":%ld,\"TRIAGE_CODE\":%u}",
           patient_id, triage_code);

  LOG_INFO("Reporting triage code %u for patient %ld\n",
           triage_code, patient_id);

  coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
  coap_set_header_uri_path(request, TRIAGE_REPORT_URI);
  coap_set_header_content_format(request, APPLICATION_JSON);
  coap_set_payload(request, (uint8_t *)payload_buf, strlen(payload_buf));

  /* CON, not NON: a triage change is a rare, clinically meaningful
   * event - delivery matters more than the extra ack. Consistent with
   * the CON/NON policy used elsewhere in the project.
   *
   * Returns as soon as the request is queued; report_callback() runs
   * later. */
  busy = true;
  if(!coap_send_request(&request_state, &cloud_ep, request, &report_callback)) {
    LOG_ERR("Could not queue the triage report\n");
    busy = false;
    return false;
  }

  return true;
}
/*---------------------------------------------------------------------------*/
