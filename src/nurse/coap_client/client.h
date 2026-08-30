typedef enum {
    CODE_RED    = 1,    //  0   minutes (max priority)
    CODE_ORANGE = 2,    //  15  minutes
    CODE_BLUE   = 3,    //  60  minutes
    CODE_GREEN  = 4,    //  120 minutes
    CODE_WHITE  = 5     //  240 minutes (min priority)
} triage_code_t;

typedef struct {
    long patient_id;
    char *name;
    char *surname;
    triage_code_t triage_code;
    uint32_t timestamp;
} patient_info_t;