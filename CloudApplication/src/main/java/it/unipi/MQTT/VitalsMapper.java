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

        for (SenMLRecord r : records) {
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
                case "rr":
                    resprationRate = (int) Math.round(r.getValue());
                default:
                    // TODO: log, don't silently ignore an unrecognised field
                    break;
            }
        }

        return new Vitals(
                patientId, Instant.now(),
                heartRate, spo2, temperature,
                pressureSystolic, pressureDiastolic,
                resprationRate);
    }
}