#ifndef NURSE_PATIENTS_H_
#define NURSE_PATIENTS_H_

#include <stdint.h>
#include <stddef.h>

#include "dev/leds.h"

//  Max number of patients that can be associated to a nurse
#define MAX_PATIENT_NUMBER 7

//  Max number of colors available (Nordic nRF52840 Dongle)
#define MAX_COLORS 7

//  Social security number's size (9 characters + '\0')
#define SSN_SIZE 10

//  Patient name and surname's buffer size
#define NAME_SIZE 30

//  Defition of function return values 
#define RESULT_SUCCESS                       1
#define RESULT_INVALID_INPUT                 0
#define RESULT_ARRAY_IS_FULL                -1
#define RESULT_PATIENT_NOT_ASSOCIATED       -2
#define RESULT_PATIENT_ALREADY_ASSOCIATED   -3
#define RESULT_PATIENT_ALREADY_PENDING      -4
#define RESULT_PATIENT_NOT_PENDING          -5

#define COLOR_INVALID 0

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

//  Patient led colors
typedef enum {
    RED       = 1,
    GREEN     = 2,
    BLUE      = 3,
    YELLOW    = 4,
    MAGENTA   = 5,
    CYAN      = 6,
    WHITE     = 7
} patient_led_color_t;
/*
    Each patient is paired with a led color. Led handling features are platform-dependent, so the way colors are handled is the following:
    The previous enum "patient_led_color_t" is a series of logical values for each color.
    There is then a function called "color_to_led_mask" which maps these logical values into real device-dependent color masks.
*/

//  ---------- Patient data structure ----------
typedef struct {

    long patient_id;

    char SSN[SSN_SIZE];
    char name[NAME_SIZE];
    char surname[NAME_SIZE];

    triage_code_t triage_code;
    uint32_t reception_timestamp;
    uint32_t last_visit_timestamp;

    patient_led_color_t led_color;

    patient_status_t status;

} patient_data_t;
//  --------------------------------------------



//  ---------- Request data structure ----------
typedef struct {

    long patient_id;
    triage_code_t triage_code;
    uint32_t assistance_timestamp;
    patient_led_color_t led_color;

} request_data_t;
//  --------------------------------------------



//  ---------- Led colors data structure -------
//  To assign colors to patients, free colors need to be tracked
typedef struct {

    patient_led_color_t color;
    int free;

} color_status_t;
//  --------------------------------------------



/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

    //  Initialize arrays
    void init_arrays(void);

    //  Shift requests to the left (overwriting one)
    void shift_requests(int);

    //  Return a pointer to a specific patient
    patient_data_t* get_patient_pointer(int);
    
//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  LEDS HANDLING FUNCTIONS

    //  Return the first free color
    patient_led_color_t get_new_color();

    //  Free the specified color
    void free_color(patient_led_color_t);

    //  Led colors are treated as numbers starting from 1. The correct mask is platform dependent, this funciton maps the colors to the Nordic nRF52840 Dongle masks
    int color_to_led_mask(patient_led_color_t);

    //  Update the device leds based on the head of the request queue
    void update_leds();
    
    //  Retrive the patient's led color name (e.g. "GREEN" instead of a number)
    int get_patient_color_name(int, char*);

//  LEDS HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  PATIENT ASSOCIATION FUNCTIONS

    //  Check whethere a given patient is associated with this nurse (i.e. check if its ID is currently in a non-free slot of the array)
    int patient_associated(int);

    //  Add a patient with a valid ID only if they're not already associated
    int add_patient(int, char*, char*, char*, int, uint32_t, uint32_t);

    //  Remove a patient (if not already absent)
    int remove_patient(int);

    //  Update the triage code of an already associated patient
    int update_patient_triage(int, int);

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

#endif /* NURSE_PATIENTS_H_ */