package it.unipi.Doctor;

public class DischargeResponse {

    private final int patientId;
    private final String deviceId;

    public DischargeResponse(int patientId, String deviceId) {
        this.patientId = patientId;
        this.deviceId = deviceId;
    }

    public int getPatientId() { return patientId; }
    public String getDeviceId() { return deviceId; }
}