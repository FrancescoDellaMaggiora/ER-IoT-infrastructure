package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Body of POST /er/doctor/discharge. */
public class DischargeRequest {

    @JsonProperty("id_paziente")
    public String patientId;

    public DischargeRequest(String patientId) {
        this.patientId = patientId;
    }
}