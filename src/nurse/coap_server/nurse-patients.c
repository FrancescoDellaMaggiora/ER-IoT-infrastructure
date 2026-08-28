/*
    Whenever a new patient enters the ER, they get associated to a nurse in the triage.
    Each nurse, then, has an array of associated patients. 
    This array has a known size, equal to the max number of patients that a nurse can handle.

    There is then a request queue, which stores requests ordered by priority (more on this later).
    The request queue has the same size of the array, so that each patient can always send one request (but not more than one).

    The association array is kept updated by the cloud (which communicates with nurses via CoAP messages) and holds information about:
        - patient id
        - triage code
        - status (either idle or with a pending assistance request)
        - timestamp (time at which the patient was associated with the nurse)

    When a patient sends a new request all that's needed is the patient id and the request timestamp (used to determine its priority), the triage code is extracted from the association array.
    So, assistance request cannot bring new information about the triage code, it is already knonw and computed by others.

    This file and its header handle these data structures.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "nurse-patients.h"

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

//  Initialize arrays
void init_arrays(void)
{
    int i;
    
    memset(&nurse_patients, 0, sizeof(nurse_patients));
    memset(&request_queue,  0, sizeof(request_queue));

    for(i = 0; i < MAX_PATIENT_NUMBER; i++) {
        nurse_patients[i].status = STATUS_FREE_SLOT;
    }

    current_requests = 0;
}

//  Shifts requests after removed_index one position to the left
void shift_requests(int removed_index) {
    int i;

    for(i = removed_index; i < current_requests - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }

    current_requests--;
    memset(&request_queue[current_requests], 0, sizeof(request_data_t));
}

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
int add_patient(int patient_id, int triage_code, uint32_t timestamp) {
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
            nurse_patients[i].triage_code = triage_code;
            nurse_patients[i].status = STATUS_IDLE;
            nurse_patients[i].timestamp = timestamp;

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
            nurse_patients[i].status = STATUS_FREE_SLOT;
            found = true;
            break;
        }

    }

    if(!found)
        return RESULT_PATIENT_NOT_ASSOCIATED;

    for(i = 0; i < current_requests; i++) {
        if(request_queue[i].patient_id == patient_id) {

            //  Overwrite i-th request by shifting the next ones to the left
            shift_requests(i);
            break;
        }
    }

    //  Patient removed successfully (even if they weren't associated in the first place)
    return RESULT_SUCCESS;
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

//  PATIENT ASSOCIATION FUNCTIONS END
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
//  REQUEST FUNCTIONS

//  These funcitons handle patient requests and the "request_queue" array.
//  Requests can be added to the queue based on their priority and removed from the queue.

//  Add an assistance request to the queue
int add_request(int patient_id, uint32_t timestamp) {
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
    new_request.timestamp = timestamp;

    //  The request will be successfully added (there's enough place for a request per patient)
    requesting_patient->status = STATUS_PENDING;

    //  Look for a spot for the new request in the request queue
    for(i = 0; i < current_requests; i++) {

        //  The i-th request has a higher priority
        if(request_queue[i].triage_code < new_request.triage_code)
            continue;
        
        //  The i-th request has the same priority but was issued first
        else if(request_queue[i].triage_code == new_request.triage_code && request_queue[i].timestamp < timestamp)
            continue;

        //  The new request has to be placed in an occupied slot after the others get shifted right by one position
        else {

            //  Shift other requests to the right
            int j;
            
            for(j = current_requests; j > i; j--) 
                request_queue[j] = request_queue[j-1];

            request_queue[i] = new_request;
            current_requests++;
            return RESULT_SUCCESS;
        }
    }

    //  Request has to be placed last
    request_queue[current_requests] = new_request;
    current_requests++;

    return RESULT_SUCCESS;
}

//  Remove an assistance request from the queue (e.g. the nurse presses the button)
int remove_request(int patient_id) {
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

    //  Patient hasn't issued requests
    if(requesting_patient->status == STATUS_IDLE)
        return RESULT_PATIENT_NOT_PENDING;

    //  The request will be removed 
    requesting_patient->status = STATUS_IDLE;

    //  Look for the patient request in the request queue
    for(i = 0; i < current_requests; i++) {

        if(request_queue[i].patient_id == patient_id) {

            //  Overwrite i-th request by shifting the next ones to the left
            shift_requests(i);
            break;
        }

    }

    return RESULT_SUCCESS;
}

//  REQUEST FUNCTIONS END
/*---------------------------------------------------------------------------*/