package it.unipi.Doctor;

import com.fasterxml.jackson.annotation.JsonProperty;

/** Request body for POST /er/doctor/register. */
public class RegisterPatientRequest {

    @JsonProperty("name")
    private String name;

    @JsonProperty("surname")
    private String surname;

    @JsonProperty("SSN")
    private String ssn;

    @JsonProperty("triage_code")
    private String triageCode;

    @JsonProperty("arrival_time")
    private String arrivalTime;      // ISO-8601, parsato a parte

    @JsonProperty("last_visit_time")
    private String lastVisitTime;    // opzionale: può essere null

    @JsonProperty("id_device")
    private String deviceId;

    @JsonProperty("id_dept")
    private String deptId;

    public RegisterPatientRequest() {}

    public String getName() { return name; }
    public String getSurname() { return surname; }
    public String getSsn() { return ssn; }
    public String getTriageCode() { return triageCode; }
    public String getArrivalTime() { return arrivalTime; }
    public String getLastVisitTime() { return lastVisitTime; }
    public String getDeviceId() { return deviceId; }
    public String getDeptId() { return deptId; }
}