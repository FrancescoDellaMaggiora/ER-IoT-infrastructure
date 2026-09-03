//  Social security number's size (9 characters + '\0')
#define SSN_SIZE 10

//  Patient name and surname's buffer size
#define NAME_SIZE 30

//  Triage emergency color codes
typedef enum {
    CODE_RED    = 1,    //  0   minutes (max priority)
    CODE_ORANGE = 2,    //  15  minutes
    CODE_BLUE   = 3,    //  60  minutes
    CODE_GREEN  = 4,    //  120 minutes
    CODE_WHITE  = 5     //  240 minutes (min priority)
} triage_code_t;

//  Patient info
typedef struct {
    long patient_id;

    char SSN[SSN_SIZE];
    char name[NAME_SIZE];
    char surname[NAME_SIZE];

    triage_code_t triage_code;
    uint32_t reception_timestamp;
    uint32_t last_visit_timestamp;
} patient_data_t;

//  When a request is sent its state becomes "STATUS_PENDING". As soon as the nurse aknowledges it, its status gets back to "STATUS_IDLE".
//  The client observes a resource to get notified about this and turn off its led
typedef enum {
    STATUS_IDLE         = 0x01,
    STATUS_PENDING      = 0x02
} patient_status_t;



/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

    //  Initialize patient (used for testing)
    void init_patient(patient_data_t*);

    //  Led color parser (the nurse associates a color to the patient)
    int parse_led_color(const char*, int, char*, int);

    //  Request status parser
    int parse_status(const char*, int, long*, patient_status_t*);

//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/





/*---------------------------------------------------------------------------*/
//  LEDS HANDLING FUNCTIONS

    //  Led colors are treated as strings, this funciton converts them to masks. 
    //  The correct mask is platform dependent, this funciton maps the colors to the Nordic nRF52840 Dongle masks
    int color_to_led_mask(char*);

    //  Turn on the led with the specified color
    void update_leds(char*);

//  LEDS HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
// RESOURCE HANDLING FUNCTIONS

    //  This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses
    void client_chunk_handler(coap_message_t*);

    //  Start/stop the observation of the remote resource
    void start_observation(void);
    void stop_observation(void);

// RESOURCE HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/