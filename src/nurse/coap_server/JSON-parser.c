#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "coap-engine.h"
#include "common.h"

int parse_assistance_payload(const uint8_t *buffer, int payload_len, int *patient_id, uint32_t *timestamp) {
    char *id_pointer;
    char *timestamp_pointer;
    char *end_pointer;

    /*
     * The buffer must be null-terminated because strstr(), strchr()
     * and strtol()/strtoul() operate on C strings.
     */
    if(buffer == NULL || patient_id == NULL || timestamp == NULL ||
       payload_len <= 0) {
        return -1;
    }

    /* PATIENT_ID */
    id_pointer = strstr((char *)buffer, "\"PATIENT_ID\"");

    if(id_pointer == NULL) {
        LOG_ERR("Bad assistance request: PATIENT_ID not found\n");
        return -1;
    }

    id_pointer = strchr(id_pointer, ':');

    if(id_pointer == NULL) {
        LOG_ERR("Bad assistance request: invalid PATIENT_ID field\n");
        return -1;
    }

    id_pointer++;

    while(*id_pointer == ' ' || *id_pointer == '\t' ||
          *id_pointer == '\n' || *id_pointer == '\r') {
        id_pointer++;
    }

    *patient_id = strtol(id_pointer, &end_pointer, 10);

    if(id_pointer == end_pointer) {
        LOG_ERR("Bad assistance request: invalid patient ID\n");
        return -1;
    }

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != ',') {
        LOG_ERR("Bad assistance request: expected ',' after PATIENT_ID\n");
        return -1;
    }

    /* TIMESTAMP */
    timestamp_pointer = strstr(end_pointer, "\"TIMESTAMP\"");

    if(timestamp_pointer == NULL) {
        LOG_ERR("Bad assistance request: TIMESTAMP not found\n");
        return -1;
    }

    timestamp_pointer = strchr(timestamp_pointer, ':');

    if(timestamp_pointer == NULL) {
        LOG_ERR("Bad assistance request: invalid TIMESTAMP field\n");
        return -1;
    }

    timestamp_pointer++;

    while(*timestamp_pointer == ' ' || *timestamp_pointer == '\t' ||
          *timestamp_pointer == '\n' || *timestamp_pointer == '\r') {
        timestamp_pointer++;
    }

    {
        unsigned long parsed_timestamp;

        parsed_timestamp = strtoul(timestamp_pointer, &end_pointer, 10);

        if(timestamp_pointer == end_pointer) {
            LOG_ERR("Bad assistance request: invalid timestamp\n");
            return -1;
        }

        if(parsed_timestamp > UINT32_MAX) {
            LOG_ERR("Bad assistance request: timestamp too large\n");
            return -1;
        }

        *timestamp = (uint32_t)parsed_timestamp;
    }

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != '}') {
        LOG_ERR("Bad assistance request: expected '}' after TIMESTAMP\n");
        return -1;
    }

    end_pointer++;

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != '\0') {
        LOG_ERR("Bad assistance request: invalid data after JSON body\n");
        return -1;
    }

    return 0;
}

int parse_association_payload(const uint8_t *buffer, int payload_len, int *patient_id, char *ssn, size_t ssn_size, char *name, size_t name_size, char *surname, 
                            size_t surname_size, int *triage_code, uint32_t *reception_timestamp, uint32_t *last_visit_timestamp) {
    char *pointer;
    char *end_pointer;
    unsigned long parsed_timestamp;
    int i;

    /*--------------------------------------------------------------------*/
    // Input validation

    if(buffer == NULL ||
       patient_id == NULL ||
       ssn == NULL ||
       name == NULL ||
       surname == NULL ||
       triage_code == NULL ||
       reception_timestamp == NULL ||
       last_visit_timestamp == NULL ||
       payload_len <= 0 ||
       ssn_size == 0 ||
       name_size == 0 ||
       surname_size == 0) {

        LOG_ERR("Bad association request: invalid parser arguments\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // PATIENT_ID

    pointer = strstr((char *)buffer, "\"PATIENT_ID\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: PATIENT_ID not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid PATIENT_ID field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    *patient_id = strtol(pointer, &end_pointer, 10);

    if(pointer == end_pointer) {
        LOG_ERR("Bad association request: invalid patient ID\n");
        return -1;
    }

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after PATIENT_ID\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // SSN

    pointer = strstr(end_pointer, "\"SSN\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: SSN not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid SSN field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: SSN must be a string\n");
        return -1;
    }

    pointer++;

    i = 0;

    while(*pointer != '"' && *pointer != '\0') {

        if((size_t)i >= ssn_size - 1) {
            LOG_ERR("Bad association request: SSN too long\n");
            return -1;
        }

        ssn[i++] = *pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: unterminated SSN\n");
        return -1;
    }

    ssn[i] = '\0';

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after SSN\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // NAME

    pointer = strstr(pointer, "\"NAME\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: NAME not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid NAME field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: NAME must be a string\n");
        return -1;
    }

    pointer++;

    i = 0;

    while(*pointer != '"' && *pointer != '\0') {

        if((size_t)i >= name_size - 1) {
            LOG_ERR("Bad association request: NAME too long\n");
            return -1;
        }

        name[i++] = *pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: unterminated NAME\n");
        return -1;
    }

    name[i] = '\0';

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after NAME\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // SURNAME

    pointer = strstr(pointer, "\"SURNAME\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: SURNAME not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid SURNAME field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: SURNAME must be a string\n");
        return -1;
    }

    pointer++;

    i = 0;

    while(*pointer != '"' && *pointer != '\0') {

        if((size_t)i >= surname_size - 1) {
            LOG_ERR("Bad association request: SURNAME too long\n");
            return -1;
        }

        surname[i++] = *pointer++;
    }

    if(*pointer != '"') {
        LOG_ERR("Bad association request: unterminated SURNAME\n");
        return -1;
    }

    surname[i] = '\0';

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    if(*pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after SURNAME\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // TRIAGE_CODE

    pointer = strstr(pointer, "\"TRIAGE_CODE\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: TRIAGE_CODE not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid TRIAGE_CODE field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    *triage_code = strtol(pointer, &end_pointer, 10);

    if(pointer == end_pointer) {
        LOG_ERR("Bad association request: invalid triage code\n");
        return -1;
    }

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after TRIAGE_CODE\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // RECEPTION_TIMESTAMP

    pointer = strstr(end_pointer, "\"RECEPTION_TIMESTAMP\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: RECEPTION_TIMESTAMP not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid RECEPTION_TIMESTAMP field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    parsed_timestamp = strtoul(pointer, &end_pointer, 10);

    if(pointer == end_pointer) {
        LOG_ERR("Bad association request: invalid reception timestamp\n");
        return -1;
    }

    if(parsed_timestamp > UINT32_MAX) {
        LOG_ERR("Bad association request: reception timestamp too large\n");
        return -1;
    }

    *reception_timestamp = (uint32_t)parsed_timestamp;

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != ',') {
        LOG_ERR("Bad association request: expected ',' after RECEPTION_TIMESTAMP\n");
        return -1;
    }

    /*--------------------------------------------------------------------*/
    // LAST_VISIT_TIMESTAMP

    pointer = strstr(end_pointer, "\"LAST_VISIT_TIMESTAMP\"");

    if(pointer == NULL) {
        LOG_ERR("Bad association request: LAST_VISIT_TIMESTAMP not found\n");
        return -1;
    }

    pointer = strchr(pointer, ':');

    if(pointer == NULL) {
        LOG_ERR("Bad association request: invalid LAST_VISIT_TIMESTAMP field\n");
        return -1;
    }

    pointer++;

    while(*pointer == ' ' || *pointer == '\t' ||
          *pointer == '\n' || *pointer == '\r') {
        pointer++;
    }

    parsed_timestamp = strtoul(pointer, &end_pointer, 10);

    if(pointer == end_pointer) {
        LOG_ERR("Bad association request: invalid last visit timestamp\n");
        return -1;
    }

    if(parsed_timestamp > UINT32_MAX) {
        LOG_ERR("Bad association request: last visit timestamp too large\n");
        return -1;
    }

    *last_visit_timestamp = (uint32_t)parsed_timestamp;

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    /*--------------------------------------------------------------------*/
    // End of JSON

    if(*end_pointer != '}') {
        LOG_ERR("Bad association request: expected '}' after LAST_VISIT_TIMESTAMP\n");
        return -1;
    }

    end_pointer++;

    while(*end_pointer == ' ' || *end_pointer == '\t' ||
          *end_pointer == '\n' || *end_pointer == '\r') {
        end_pointer++;
    }

    if(*end_pointer != '\0') {
        LOG_ERR("Bad association request: invalid data after JSON body\n");
        return -1;
    }

    return 0;
}