package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/** Body of a successful (200) response from POST /er/doctor/update-triage. */
@JsonIgnoreProperties(ignoreUnknown = true)
public class UpdateTriageResponse {
    public int patientId;
    public String triageCode;
}