package it.unipi.Repository;

public class ActivePatientInfo {
    private final int patientId;
    private final String triageCode;

    public ActivePatientInfo(int patientId, String triageCode) {
        this.patientId = patientId;
        this.triageCode = triageCode;
    }

    public int getPatientId() { return patientId; }
    public String getTriageCode() { return triageCode; }
}