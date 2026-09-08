package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/** Body of a successful (201) response from POST /er/doctor/register. */
@JsonIgnoreProperties(ignoreUnknown = true)
public class RegisterPatientResponse {
    public int patientId;
    public String nurseId;
}