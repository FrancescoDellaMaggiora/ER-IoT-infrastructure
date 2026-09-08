package it.unipi.Doctor;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Request body for POST /er/doctor/update-triage. */
public class UpdateTriageRequest {

    @JsonProperty("id_paziente")
    private String patientId;   // parsed to int in the handler

    @JsonProperty("triage_code")
    private String triageCode;

    public UpdateTriageRequest() {}

    public String getPatientId() { return patientId; }
    public String getTriageCode() { return triageCode; }
}