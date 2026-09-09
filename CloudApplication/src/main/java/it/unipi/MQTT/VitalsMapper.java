package it.unipi.MQTT;

import java.time.Instant;
import java.util.List;

/** Maps a parsed SenML pack onto the domain object Vitals. */
final class VitalsMapper {

    private static final String ALERT_SUFFIX = "-alert";

    private VitalsMapper() {}

    static Vitals fromSenML(String patientId, List<SenMLRecord> records) {
        Integer heartRate = null;
        Integer spo2 = null;
        Double temperature = null;
        Integer pressureSystolic = null;
        Integer pressureDiastolic = null;
        Integer resprationRate = null;
        
        Instant timestamp = Instant.now();
        for (SenMLRecord r : records) {

            if (r.getTime() != null && Math.abs(r.getTime()) < (1L << 28)) {
                timestamp = timestamp.plusSeconds(r.getTime().longValue());
                continue;
            }

            String name = r.getName();

            if (name.endsWith(ALERT_SUFFIX)) {
                name = name.substring(0, name.length() - ALERT_SUFFIX.length());
            }

            switch (name) {
                case "heart-rate":
                    heartRate = (int) Math.round(r.getValue());
                    break;
                case "spo2":
                    spo2 = (int) Math.round(r.getValue());
                    break;
                case "temperature":
                    temperature = r.getValue();
                    break;
                case "sys-pressure":
                    pressureSystolic = (int) Math.round(r.getValue());
                    break;
                case "dia-pressure":
                    pressureDiastolic = (int) Math.round(r.getValue());
                    break;
                case "respiration_rate":
                    resprationRate = (int) Math.round(r.getValue());
                default:
                    break;
            }
        }

        return new Vitals(
                patientId, timestamp,
                heartRate, spo2, temperature,
                pressureSystolic, pressureDiastolic,
                resprationRate);
    }
}