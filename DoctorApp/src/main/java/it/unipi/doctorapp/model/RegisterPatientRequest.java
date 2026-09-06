package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Body of POST /er/doctor/register.
 *
 * Field names must match exactly what the Cloud's
 * RegisterPatientRequest expects (snake_case, uppercase SSN).
 */
public class RegisterPatientRequest {

    @JsonProperty("name")
    public String name;

    @JsonProperty("surname")
    public String surname;

    @JsonProperty("SSN")
    public String ssn;

    @JsonProperty("triage_code")
    public String triageCode;

    @JsonProperty("arrival_time")
    public String arrivalTime;      // ISO-8601

    @JsonProperty("last_visit_time")
    public String lastVisitTime;    // optional, may stay null

    @JsonProperty("id_device")
    public String deviceId;

    @JsonProperty("id_dept")
    public String deptId;
}