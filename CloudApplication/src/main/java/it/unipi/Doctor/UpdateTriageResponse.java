package it.unipi.Doctor;

public class UpdateTriageResponse {

    private final int patientId;
    private final String triageCode;

    public UpdateTriageResponse(int patientId, String triageCode) {
        this.patientId = patientId;
        this.triageCode = triageCode;
    }

    public int getPatientId() { return patientId; }
    public String getTriageCode() { return triageCode; }
}