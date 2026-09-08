package it.unipi.MQTT;

import java.time.Instant;

/**
 * Domain object for a patient's vitals reading.
 *
 * This is what the rest of the system (Storage, Control Logic) works
 * with - it has no idea SenML or MQTT exist. It is built FROM a
 * List<SenMLRecord> + the patient id extracted from the topic, but
 * that mapping is a separate step (not here): this class is just data.
 *
 * Fields are boxed (Integer/Double), not primitives, because not every
 * patient necessarily has every sensor attached (see 'attached_sensors'
 * bitmask on the firmware side) - a missing sensor means the field
 * stays null, not a fake zero.
 */
public class Vitals {

    private final String patientId;
    private final Instant timestamp;

    private final Integer heartRate;         // beat/min
    private final Integer spo2;              // /100
    private final Double temperature;        // Cel
    private final Integer pressureSystolic;  // mmHg
    private final Integer pressureDiastolic; // mmHg
    private final Integer resprationRate;    // breaths/min

    public Vitals(String patientId, Instant timestamp,
                  Integer heartRate, Integer spo2, Double temperature,
                  Integer pressureSystolic, Integer pressureDiastolic,
                  Integer resprationRate) {
        this.patientId = patientId;
        this.timestamp = timestamp;
        this.heartRate = heartRate;
        this.spo2 = spo2;
        this.temperature = temperature;
        this.pressureSystolic = pressureSystolic;
        this.pressureDiastolic = pressureDiastolic;
        this.resprationRate = resprationRate;
    }

    public String getPatientId() { return patientId; }
    public Instant getTimestamp() { return timestamp; }
    public Integer getHeartRate() { return heartRate; }
    public Integer getSpo2() { return spo2; }
    public Double getTemperature() { return temperature; }
    public Integer getPressureSystolic() { return pressureSystolic; }
    public Integer getPressureDiastolic() { return pressureDiastolic; }
    public Integer getResprationRate() { return resprationRate; }

    @Override
    public String toString() {
        return "Vitals{" +
                "patientId='" + patientId + '\'' +
                ", timestamp=" + timestamp +
                ", heartRate=" + heartRate +
                ", spo2=" + spo2 +
                ", temperature=" + temperature +
                ", pressureSystolic=" + pressureSystolic +
                ", pressureDiastolic=" + pressureDiastolic +
                '}';
    }
}