#include <stdint.h>
#include <stddef.h>

//  Max number of patients that can be associated to a nurse
#define MAX_PATIENT_NUMBER 10

//  Defition of function return values 
#define RESULT_SUCCESS                       1
#define RESULT_INVALID_INPUT                 0
#define RESULT_ARRAY_IS_FULL                -1
#define RESULT_PATIENT_NOT_ASSOCIATED       -2
#define RESULT_PATIENT_ALREADY_ASSOCIATED   -3
#define RESULT_PATIENT_ALREADY_PENDING      -4
#define RESULT_PATIENT_NOT_PENDING          -5

//  Defition of states a patient can be in
typedef enum {
    STATUS_IDLE         = 0x01,
    STATUS_PENDING      = 0x02,   
    STATUS_FREE_SLOT    = 0xFF  //  This status represents an empty position in the array of patients
} patient_status_t;

//  Triage emergency color codes
typedef enum {
    CODE_RED    = 1,    //  0   minutes (max priority)
    CODE_ORANGE = 2,    //  15  minutes
    CODE_BLUE   = 3,    //  60  minutes
    CODE_GREEN  = 4,    //  120 minutes
    CODE_WHITE  = 5     //  240 minutes (min priority)
} triage_code_t;

//  Patient data structure
typedef struct {
    long patient_id;
    triage_code_t triage_code;
    patient_status_t status;
    uint32_t timestamp;
} patient_data_t;

//  Request data structure (same as the previous one except for the removed status field)
typedef struct {
    long patient_id;
    triage_code_t triage_code;
    uint32_t timestamp;
} request_data_t;



/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

    //  Initialize arrays
    void init_arrays(void);

    //  Shift requests to the left (overwriting one)
    void shift_requests(int);
    
//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  PATIENT ASSOCIATION FUNCTIONS

    //  Check whethere a given patient is associated with this nurse (i.e. check if its ID is currently in a non-free slot of the array)
    int patient_associated(int);

    //  Add a patient with a valid ID only if they're not already associated
    int add_patient(int, int, uint32_t);

    //  Remove a patient (if not already absent)
    int remove_patient(int);

    //  Return a pointer to a specific patient
    patient_data_t* get_patient_pointer(int);

//  PATIENT ASSOCIATION FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  REQUEST FUNCTIONS

    //  Add an assistance request to the queue
    int add_request(int, uint32_t);

    //  Get the next request (head of the queue)
    request_data_t get_next_request();

    //  Retreive an assistance request from the queue
    request_data_t get_request_by_id(int);

//  REQUEST FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  DEBUG FUNCTIONS

    //  Print all patients
    void print_patients(void);

    //  Print all requests
    void print_requests(void);

//  DEBUG FUNCTIONS END
/*---------------------------------------------------------------------------*/