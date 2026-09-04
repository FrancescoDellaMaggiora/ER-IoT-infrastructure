package it.unipi.Patient;

import java.time.LocalDateTime;

public class WaitingPatient {
    private final int patientId;
    private final String deviceId;
    private final String triageCode;
    private final LocalDateTime lastCheckTime;   // last_visit_time, o arrival_time se mai visitato

    public WaitingPatient(int patientId, String deviceId, String triageCode, LocalDateTime lastCheckTime) {
        this.patientId = patientId;
        this.deviceId = deviceId;
        this.triageCode = triageCode;
        this.lastCheckTime = lastCheckTime;
    }

    public int getPatientId() { return patientId; }

    public String getDeviceId() { return deviceId; }

    public String getTriageCode() { return triageCode; }

    public LocalDateTime getLastCheckTime() { return lastCheckTime; }
}