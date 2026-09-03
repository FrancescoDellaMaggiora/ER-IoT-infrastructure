/*
    Parse an assistance request JSON payload.

    Expected format:
    {
        "PATIENT_ID":  <number>,
        "TIMESTAMP":   <number>
    }

    Returns 0 on success, -1 on parsing error.
*/
int parse_assistance_payload(const uint8_t*, int, int*, uint32_t*);

/*
    Parse an association request JSON payload.

    Expected format:
    {
        "PATIENT_ID":           <number>,
        "SSN":                  <string>,
        "NAME":                 <string>,
        "SURNAME":              <string>,
        "TRIAGE_CODE":          <number>,
        "RECEPTION_TIMESTAMP":  <timestamp>, 
        "LAST_VISIT_TIMESTAMP": <timestamp> 
    }

    Returns 0 on success, -1 on parsing error.
*/
int parse_association_payload(const uint8_t*, int, int*, char*, size_t, char*, size_t, char*, size_t, int*, uint32_t*, uint32_t*);

/*
    Parse a dissociation request JSON payload.

    Expected format:
    {
        "PATIENT_ID":           <number>
    }

    Returns 0 on success, -1 on parsing error.
*/
int parse_dissociation_payload(const uint8_t*, int*);