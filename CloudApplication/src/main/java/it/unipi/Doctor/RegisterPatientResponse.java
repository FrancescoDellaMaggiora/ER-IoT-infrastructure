package it.unipi.Doctor;

public class RegisterPatientResponse {

    private final int patientId;
    private final String nurseId;

    public RegisterPatientResponse(int patientId, String nurseId) {
        this.patientId = patientId;
        this.nurseId = nurseId;
    }

    public int getPatientId() { return patientId; }
    public String getNurseId() { return nurseId; }
}