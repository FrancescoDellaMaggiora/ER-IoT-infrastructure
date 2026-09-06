package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Body of POST /er/doctor/next-patient. */
public class NextPatientRequest {

    @JsonProperty("id_dept")
    public String deptId;

    public NextPatientRequest(String deptId) {
        this.deptId = deptId;
    }
}