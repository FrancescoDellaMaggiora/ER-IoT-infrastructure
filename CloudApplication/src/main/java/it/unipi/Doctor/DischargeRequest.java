package it.unipi.Doctor;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Request body for POST /er/doctor/discharge. */
public class DischargeRequest {

    @JsonProperty("id_paziente")
    private String patientId;

    public DischargeRequest() {}

    public String getPatientId() { return patientId; }
}