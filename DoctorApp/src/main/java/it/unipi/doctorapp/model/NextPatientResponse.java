package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/** Body of a successful (200) response from POST /er/doctor/next-patient. */
@JsonIgnoreProperties(ignoreUnknown = true)
public class NextPatientResponse {
    public int patientId;
    public String deviceId;
}