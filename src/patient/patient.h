/*
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
/*
 * ER-IOT-INFRASTRUCTURE
 * patient.h - Application data model of the patient node
 *
 * This header holds everything that describes the patient DOMAIN:
 * which sensors exist, the vitals data structure, the clinical
 * thresholds and the MQTT topic naming scheme. Nothing in here knows
 * how MQTT works: the transport lives in mqtt-service.{h,c}.
 *
 */
/*---------------------------------------------------------------------------*/
#ifndef PATIENT_H_
#define PATIENT_H_
/*---------------------------------------------------------------------------*/
#include <stdint.h>
/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//  PATIENT_ID (default is 0) used to construct the MQTT CLIENT_ID.
//  It is learnt at build time:
//    make TARGET=<target> PATIENT_ID=<number>

#ifndef PATIENT_ID
#define PATIENT_ID 1
#endif

/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//  These macros are used, together with PATIENT_ID, to construct topic names.
//
//  - PUB_TOPIC_NAME is the topic namespace
//  - PATIENT_ID identifies the patient
//  - The last field is the data type. Many types can be defined, for example:
//
//  PUB_TOPIC_NAME/PATIENT_ID/PATIENT_VITALS = "er/patient/1/vitals"

#define PUB_TOPIC_NAME "er/patient"

#define PATIENT_VITALS "vitals"
#define PATIENT_ALERT "alert"

//  These IDs identify the topics.
//  They are used, for example, in the publish(uint8_t topic_id) function to
//  differentiate between topics and to build the correct payload
#define TOPIC_MSG_VITALS 0
#define TOPIC_MSG_ALERT 1

/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code
//
//  These macros are bitmasks used to define what sensors are attached to the
//  patient (using the "attached_sensors" variable).
//
//  TODO: decide what units of measurement to use:
//  The used units are listed as well as they are needed to build the SenML
//  JSON payload.
//  "beat/min" and "Cel" are compliant with RFC 8428, whereas "/100" is a
//  secondary unit belonging to the extension in RFC 8798 (not always valid).
//  Moreover, "mmHg" is not a valid unit of measurement and it may need to be
//  converted to "Pa" (1 mmHg is circa 133.322 Pa)

//      SENSOR_TYPE                          UNIT OF MEASUREMENT (RFC 8428, RFC 8798)
#define SENSOR_HEART_RATE         (1 << 0)  //  beat/min
#define SENSOR_SPO2               (1 << 1)  //  /100
#define SENSOR_TEMPERATURE        (1 << 2)  //  Cel
#define SENSOR_PRESSURE_SYSTOLIC  (1 << 3)  //  mmHg
#define SENSOR_PRESSURE_DIASTOLIC (1 << 4)  //  mmHg

//  These macros are bitmasks used to identify current alert parameters
//  (using the "active_alerts" variable).

#define ALERT_HEART_RATE          (1 << 0)
#define ALERT_SPO2                (1 << 1)
#define ALERT_TEMPERATURE         (1 << 2)
#define ALERT_PRESSURE_SYSTOLIC   (1 << 3)
#define ALERT_PRESSURE_DIASTOLIC  (1 << 4)

//  These macros define thresholds for each measured parameter.
//  They implement the "Level 1" fixed clinical safety net discussed in
//  the project design: crossing any of them raises an alert and switches
//  the node to the fast publish rate, regardless of anything else.

#define MIN_HEART_RATE          60
#define MAX_HEART_RATE          100

#define MIN_SPO2                95
#define MAX_SPO2                100

#define MIN_TEMPERATURE         36.0f
#define MAX_TEMPERATURE         37.5f

#define MIN_PRESSURE_SYSTOLIC   90
#define MAX_PRESSURE_SYSTOLIC   140

#define MIN_PRESSURE_DIASTOLIC  60
#define MAX_PRESSURE_DIASTOLIC  90

/*---------------------------------------------------------------------------*/

//  ALESSANDRO: I wrote this code

/*
 * Data structure declaration to store patient vitals
 */
typedef struct {

  uint16_t heart_rate;
  uint8_t spo2;
  float temperature;
  uint16_t pressure_systolic;
  uint16_t pressure_diastolic;

} patient_vitals_t;

/*---------------------------------------------------------------------------*/
#endif /* PATIENT_H_ */
/*---------------------------------------------------------------------------*/
