#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "common.h"
#include "nurse-patients.h"

/*
    Whenever a new patient enters the ER, they get associated to a nurse in the triage.
    Each nurse, then, has an array of associated patients. 
    This array has a known size, equal to the max number of patients that a nurse can handle.

    There is then a request queue, which stores requests ordered by priority (more on this later).
    The request queue has the same size of the array, so that each patient can always send one request (but not more than one).

    The association array is kept updated by the cloud (which communicates with nurses via CoAP messages) and holds information about:
        - patient id
        - SSN
        - name
        - surname
        - triage code
        - reception_timestamp (time at which the patient was associated with the nurse)
        - last_visit_timestamp 
        - status (either idle or with a pending assistance request)

    When a patient sends a new request all that's needed is the patient id and the request timestamp (used to determine its priority), the triage code is extracted from the association array.
    So, assistance requests cannot bring new information about the triage code, it is already knonw and computed by others.

    This file and its header handle these data structures.
*/

//  This array keeps track of what patients are associated to this nurse.
//  For this array, the field "timestamp" indicates the time at which the patient was associated to the nurse.
static patient_data_t nurse_patients[MAX_PATIENT_NUMBER];

/*
    This is the request queue.

    Whenever a patient sends an assistance request, this can be accepted by the nurse only if the patient is part of "nurse_patients".

    If the request is valid, this gets added to the "request_queue" which is an ordered array based on the request priority.
    The latter is computed based on the patient's triage code and the timestamp of the request (different requests with the same code are handled with a FIFO policy).

    The array is filled with a top-down approach, so that the next request (i.e. the current highest priority one) can always be extracted from the first location.
    A patient can issue a single assistance request at the time.

    For this array, the field "timestamp" indicates the time at which each request was issued.
*/  
static request_data_t request_queue[MAX_PATIENT_NUMBER];

//  Keep track of the current request number
static uint8_t current_requests;

//  Array to keep track of free colors that can be associated to new patients
static color_status_t available_colors[MAX_COLORS];

/*---------------------------------------------------------------------------*/
//  UTILITY FUNCTIONS

//  Initialize arrays
void init_arrays(void)
{
    int i;
    
    memset(&nurse_patients, 0, sizeof(nurse_patients));
    memset(&request_queue,  0, sizeof(request_queue));

    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {
        nurse_patients[i].status = STATUS_FREE_SLOT;
    }

    //  Initialize available logic colors
    available_colors[0].color = RED;
    available_colors[1].color = GREEN;
    available_colors[2].color = BLUE;
    available_colors[3].color = YELLOW;
    available_colors[4].color = MAGENTA;
    available_colors[5].color = CYAN;
    available_colors[6].color = WHITE;

    for(i = 0; i < MAX_COLORS; i++) {
        available_colors[i].free = 1;
    }

    current_requests = 0;
}

//  Shifts requests with index greater that "removed_index" one position to the left (overwriting the one in position "removed_index")
void shift_requests(int removed_index) {
    int i;

    for(i = removed_index; i < current_requests - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }

    current_requests--;
    memset(&request_queue[current_requests], 0, sizeof(request_data_t));
}

//  Return a pointer to a specific patient
patient_data_t* get_patient_pointer(int patient_id) {
    int i;

    //  Invalid ID
    if(patient_id < 0) 
        return NULL;

    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {
        if (nurse_patients[i].status != STATUS_FREE_SLOT && nurse_patients[i].patient_id == patient_id) 
            return &nurse_patients[i];
    }

    //  Patient not associated
    return NULL;
}

//  UTILITY FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  LEDS HANDLING FUNCTIONS

//  Return the first free color
patient_led_color_t get_new_color() {
    int i;
    
    for(i = 0; i < MAX_COLORS; i++) {

        if(available_colors[i].free == 1) {
            available_colors[i].free = 0;
            return available_colors[i].color;
        }

    }

    return COLOR_INVALID;
}

//  Free the specified color
void free_color(patient_led_color_t freed_color) {
    int i;
    
    for(i = 0; i < MAX_COLORS; i++) {

        if(available_colors[i].color == freed_color) {
            available_colors[i].free = 1;
            return;
        }

    }
}

//  Led colors are treated as numbers starting from 1. The correct mask is platform dependent, this funciton maps the colors to the Nordic nRF52840 Dongle masks
int color_to_led_mask(patient_led_color_t color) {

    switch(color) {

        case RED:
            return LEDS_RED;

        case GREEN:
            return LEDS_GREEN;

        case BLUE:
            return LEDS_BLUE;

        case YELLOW:
            return LEDS_RED | LEDS_GREEN;

        case MAGENTA:
            return LEDS_RED | LEDS_BLUE;

        case CYAN:
            return LEDS_GREEN | LEDS_BLUE;

        case WHITE:
            return LEDS_RED | LEDS_GREEN | LEDS_BLUE;

        default:
            return 0;
    }

}

//  Update the device leds with the color of the patient at the head of the request queue
void update_leds() {
    //  Turn off all leds
    leds_off(LEDS_ALL);

    if(current_requests == 0) 
        return;

    leds_on(color_to_led_mask(request_queue[0].led_color));
}

//  Retrive the patient's led color name (e.g. "GREEN" instead of a number)
int get_patient_color_name(int patient_id, char *color_name) {

    patient_data_t *patient = get_patient_pointer(patient_id);

    //  Patient not associated
    if(patient == NULL) 
        return RESULT_PATIENT_NOT_ASSOCIATED;

    switch(patient->led_color) {

        case RED:
            strcpy(color_name, "RED");
            return 1;

        case GREEN:
            strcpy(color_name, "GREEN");
            return 1;

        case BLUE:
            strcpy(color_name, "BLUE");
            return 1;

        case YELLOW:
            strcpy(color_name, "YELLOW");
            return 1;

        case MAGENTA:
            strcpy(color_name, "MAGENTA");
            return 1;

        case CYAN:
            strcpy(color_name, "CYAN");
            return 1;

        case WHITE:
            strcpy(color_name, "WHITE");
            return 1;
        
        default:
            strcpy(color_name, "ERROR");
            return -1;

    }   
    
    return -1;
}

//  LEDS HANDLING FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  PATIENT ASSOCIATION FUNCTIONS

//  These function handle the "nurse_patients" array and the process of association, de-association and association verification of patients.

//  Check whether a given patient is associated with this nurse (i.e. check if its ID is currently in a non-free slot of the "nurse_patients" array).
//  If the patient is associated the returned value is their status (always positive, so it can be used as "true")
int patient_associated(int patient_id) {
    int i;

    //  Invalid ID
    if(patient_id < 0) 
        return RESULT_INVALID_INPUT;

    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {

        if (nurse_patients[i].status != STATUS_FREE_SLOT && nurse_patients[i].patient_id == patient_id) {
            return nurse_patients[i].status;
        }
        
    }

    return RESULT_PATIENT_NOT_ASSOCIATED;
}

//  Add a patient with a valid ID only if they're not already associated
int add_patient(int patient_id, char *SSN, char *name, char *surname, int triage_code, uint32_t reception_timestamp, uint32_t last_visit_timestamp) {
    int i;

    //  Invalid input
    if(patient_id < 0 || triage_code < 1 || triage_code > 5) 
        return RESULT_INVALID_INPUT;
    
    //  A patient cannot be added two times. The whole array has to be searched upfront for this check using "patient_associated".
    if(patient_associated(patient_id) > 0) 
        return RESULT_PATIENT_ALREADY_ASSOCIATED;

    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {

        if (nurse_patients[i].status == STATUS_FREE_SLOT) {

            nurse_patients[i].patient_id = patient_id;

            strcpy(nurse_patients[i].SSN, SSN);
            strcpy(nurse_patients[i].name, name);
            strcpy(nurse_patients[i].surname, surname);

            nurse_patients[i].triage_code = triage_code;

            nurse_patients[i].reception_timestamp = reception_timestamp;
            nurse_patients[i].last_visit_timestamp = last_visit_timestamp;

            //  Associate the patient to a color
            nurse_patients[i].led_color = get_new_color();

            nurse_patients[i].status = STATUS_IDLE;

            return RESULT_SUCCESS;
        }

    }

    //  Array is full
    return RESULT_ARRAY_IS_FULL;
}

//  Remove a patient (if not already absent)
int remove_patient(int patient_id) {
    int i;
    bool found = false;

    //  Invalid ID
    if(patient_id < 0) 
        return RESULT_INVALID_INPUT;
    
    //  The patient has to be removed from the list of associated patients and his requests have to be removed from the request queue
    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {

        if (nurse_patients[i].status != STATUS_FREE_SLOT && nurse_patients[i].patient_id == patient_id) {

            //  Free this patient's color
            free_color(nurse_patients[i].led_color);
            
            //  Optional: reset every field of this entry
            memset(&nurse_patients[i], 0, sizeof(nurse_patients[i]));

            //  Free the nurse_patient array slot
            nurse_patients[i].status = STATUS_FREE_SLOT;

            found = true;
            break;
        }

    }

    if(!found)
        return RESULT_PATIENT_NOT_ASSOCIATED;

    //  Remove this patient's request (if there)
    for(i = 0; i < current_requests; i++) {
        if(request_queue[i].patient_id == patient_id) {

            //  Overwrite i-th request by shifting the next ones to the left
            shift_requests(i);
            update_leds();
            break;
        }
    }

    //  Patient removed successfully (even if they weren't associated in the first place)
    return RESULT_SUCCESS;
}

//  Update the triage code of an already associated patient.
//  Deliberately does NOT insert an unknown patient: a triage update for
//  someone this nurse was never told about means the cloud and this
//  device disagree on the assignment, and silently creating a record
//  would hide that inconsistency.
int update_patient_triage(int patient_id, int triage_code) {

    patient_data_t *patient;

    //  Patient IDs must be positive and the code must be a valid triage code
    if(patient_id <= 0 || triage_code < CODE_RED || triage_code > CODE_WHITE) {
        return RESULT_INVALID_INPUT;
    }

    if(!patient_associated(patient_id)) {
        return RESULT_PATIENT_NOT_ASSOCIATED;
    }

    patient = get_patient_pointer(patient_id);

    if(patient == NULL) {
        return RESULT_PATIENT_NOT_ASSOCIATED;
    }

    //  Not an error if it is already that value: the cloud re-sends the
    //  current code on a retry, and a doctor may "change" it to what it
    //  already was.
    patient->triage_code = (triage_code_t)triage_code;

    return RESULT_SUCCESS;
}

//  PATIENT ASSOCIATION FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  REQUEST FUNCTIONS

//  These functions handle patient requests and the "request_queue" array.
//  Requests can be added to the queue based on their priority and removed from the queue.

//  Add an assistance request to the queue
int add_request(int patient_id, uint32_t assistance_timestamp) {
    int i;
    patient_data_t *requesting_patient = NULL;

    //  Invalid input
    if(patient_id < 0) 
        return RESULT_INVALID_INPUT;

    //  A pointer to the requesting patient is defined (this is used later to easily change its status in the "nurse_patients" array)
    requesting_patient = get_patient_pointer(patient_id);

    //  Patient not associated
    if(requesting_patient == NULL) 
        return RESULT_PATIENT_NOT_ASSOCIATED;

    //  Patient already issued a request
    if(requesting_patient->status == STATUS_PENDING)
        return RESULT_PATIENT_ALREADY_PENDING;

    //  The new request object is built
    request_data_t new_request;
    new_request.patient_id = patient_id;
    new_request.triage_code = requesting_patient->triage_code;  //  Assistance requests cannot update the triage code
    new_request.assistance_timestamp = assistance_timestamp;
    new_request.led_color = requesting_patient->led_color;

    //  The request will be successfully added (there's enough place for a request per patient)
    requesting_patient->status = STATUS_PENDING;

    //  Look for a spot for the new request in the request queue
    for(i = 0; i < current_requests; i++) {

        //  The i-th request has a higher priority
        if(request_queue[i].triage_code < new_request.triage_code)
            continue;
        
        //  The i-th request has the same priority but was issued first
        else if(request_queue[i].triage_code == new_request.triage_code && request_queue[i].assistance_timestamp < assistance_timestamp) 
            continue;

        //  The new request has to be placed in an occupied slot after the others get shifted right by one position
        else {

            //  Shift other requests to the right
            int j;
            
            for(j = current_requests; j > i; j--) 
                request_queue[j] = request_queue[j-1];

            request_queue[i] = new_request;
            current_requests++;

            //  Turn on the first request's led
            update_leds();
            return RESULT_SUCCESS;
        }
    }

    //  Request has to be placed last
    request_queue[current_requests] = new_request;
    current_requests++;

    //  Turn on the first request's led
    update_leds();
    return RESULT_SUCCESS;
}

//  Retreive the next request from the head of the queue (e.g. the nurse presses the button)
request_data_t get_next_request() {

    //  Returned request
    request_data_t request_return;    

    //  Used to easily change the patient's status to idle         
    patient_data_t *requesting_patient = NULL;  

    //  Used to determine whether or not the returned request is valid
    request_return.patient_id = -1;

    //  Queue is empty
    if(current_requests == 0) 
        return request_return;

    //  A pointer to the requesting patient is defined (this is used later to easily change its status in the "nurse_patients" array)
    requesting_patient = get_patient_pointer(request_queue[0].patient_id);

    //  The request will be removed 
    requesting_patient->status = STATUS_IDLE;

    //  Return the head of the queue
    request_return = request_queue[0];
    shift_requests(0);

    update_leds();
    return request_return;
}

//  Retreive an assistance request from the queue given a patient ID 
request_data_t get_request_by_id(int patient_id) {
    request_data_t request_return;              //  Returned request
    patient_data_t *requesting_patient = NULL;  //  Used to easily change the patient's status to idle
    int i;

    //  Used to determine whether or not the returned request is valid
    request_return.patient_id = -1;

    //  Invalid input
    if(patient_id < 0) 
        return request_return;

    //  A pointer to the requesting patient is defined (this is used later to easily change its status in the "nurse_patients" array)
    requesting_patient = get_patient_pointer(patient_id);

    //  Patient not associated
    if(requesting_patient == NULL) 
        return request_return;

    //  Patient hasn't issued requests
    if(requesting_patient->status == STATUS_IDLE)
        return request_return;

    //  The request will be removed 
    requesting_patient->status = STATUS_IDLE;

    //  Look for the patient request in the request queue
    for(i = 0; i < current_requests; i++) {

        if(request_queue[i].patient_id == patient_id) {
            request_return = request_queue[i];

            //  Overwrite i-th request by shifting the next ones to the left
            shift_requests(i);

            break;
        }

    }

    update_leds();
    return request_return;
}

//  REQUEST FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  DEBUG FUNCTIONS

//  Print every associated patient information
void print_patients() {
    LOG_DBG("PATIENT LIST:\n");
    int i;
    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {
        LOG_DBG("Slot: %i\t ID: %li\t SSN: %s\t Name: %s\t Surname: %s\t Code: %i\t Reception timestamp: %li\t Last visit timestamp: %li\t Led color: %i\t Status: %i\n", 
            i, nurse_patients[i].patient_id, nurse_patients[i].SSN, nurse_patients[i].name, nurse_patients[i].surname,
            nurse_patients[i].triage_code, nurse_patients[i].reception_timestamp, nurse_patients[i].last_visit_timestamp, nurse_patients[i].led_color, nurse_patients[i].status);
    }
}

//  Print all requests
void print_requests() {
    LOG_DBG("REQUEST LIST (%i active request(s)):\n", current_requests);
    int i;
    for(i = 0; i < current_requests; i++) {
        LOG_DBG("Slot: %i\t ID: %li\t Code: %i\t Timestamp: %li\t Led color: %i\n", 
            i, request_queue[i].patient_id, request_queue[i].triage_code, request_queue[i].assistance_timestamp, request_queue[i].led_color);
    }
}

//  DEBUG FUNCTIONS END
/*---------------------------------------------------------------------------*/