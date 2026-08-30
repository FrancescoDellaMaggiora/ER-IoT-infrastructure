/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

//  This macro enables debug when set to 1
#ifndef NURSE_DBG
#define NURSE_DBG 0
#endif

//  This macro can be set at build time to add patients for debug porpouses
#ifndef DBG_ADD_PATIENT
#define DBG_ADD_PATIENT 0
#endif