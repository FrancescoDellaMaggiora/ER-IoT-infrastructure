package it.unipi.Patient;

import com.fasterxml.jackson.annotation.JsonProperty;

public class NextPatientRequest {
    @JsonProperty("id_dept")
    private String deptId;

    public NextPatientRequest() {}
    public String getDeptId() { return deptId; }
}