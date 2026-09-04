package it.unipi.Patient;

public class NextPatientResponse {
    private final int patientId;
    private final String deviceId;

    public NextPatientResponse(int patientId, String deviceId) {
        this.patientId = patientId;
        this.deviceId = deviceId;
    }

    public int getPatientId() { return patientId; }
    public String getDeviceId() { return deviceId; }
}