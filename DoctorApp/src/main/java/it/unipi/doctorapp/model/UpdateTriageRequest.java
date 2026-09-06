package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Body of POST /er/doctor/update-triage. */
public class UpdateTriageRequest {

    @JsonProperty("id_paziente")
    public String patientId;

    @JsonProperty("triage_code")
    public String triageCode;

    public UpdateTriageRequest(String patientId, String triageCode) {
        this.patientId = patientId;
        this.triageCode = triageCode;
    }
}