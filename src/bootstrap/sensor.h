//  Triage emergency color codes
typedef enum {
    CODE_RED    = 1,    //  0   minutes (max priority)
    CODE_ORANGE = 2,    //  15  minutes
    CODE_BLUE   = 3,    //  60  minutes
    CODE_GREEN  = 4,    //  120 minutes
    CODE_WHITE  = 5     //  240 minutes (min priority)
} triage_code_t;

//  Patient info needed by the device
typedef struct {
    long patient_id;
    triage_code_t triage_code;
} device_data_t;



/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

    //  Request status parser
    int parse_registration(const char*, int, long*, triage_code_t*, coap_endpoint_t*);

    //  Printing function
    void print_patient_info();

//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS

    //  This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses
    void client_chunk_handler(coap_message_t*);

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/