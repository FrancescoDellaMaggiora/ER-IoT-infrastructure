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
    char name[30];
    char surname[30];
    triage_code_t triage_code;
    uint32_t timestamp;
} patient_info_t;

//  Initialize patient (used for testing)
void init_patient(patient_info_t*);