package it.unipi.Repository;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.sql.*;
import java.time.LocalDateTime;
import java.util.Properties;

/**
 * Data access layer for patients and device assignments.
 *
 * Owns all SQL for the doctor-facing endpoints. DoctorApiServer never
 * builds a query itself - it calls methods here and translates the
 * outcome (success / SQLException) into an HTTP response.
 *
 * One connection per call (DriverManager), not a pool: the call volume
 * on this endpoint (patient registrations) is low compared to the
 * vitals stream, so the simplicity is worth more than the (here,
 * negligible) performance cost of reconnecting each time.
 */
public class PatientRepository {

    private static final String KEY_URL = "db.url";
    private static final String KEY_USER = "db.user";
    private static final String KEY_PASSWORD = "db.password";

    private final String url;
    private final String user;
    private final String password;

    public PatientRepository(String configPath) throws IOException {
        Properties props = new Properties();
        try (InputStream in = new FileInputStream(configPath)) {
            props.load(in);
        }

        this.url = requireProperty(props, KEY_URL);
        this.user = requireProperty(props, KEY_USER);
        this.password = requireProperty(props, KEY_PASSWORD);
    }

    private static String requireProperty(Properties props, String key) throws IOException {
        String value = props.getProperty(key);
        if (value == null || value.isBlank()) {
            throw new IOException("Missing required property: " + key);
        }
        return value;
    }

    /**
     * Registers a new patient AND assigns them the given device, as a
     * single transaction: either both inserts succeed, or neither does.
     * Without this, a failure between the two statements could leave a
     * patient with no device, or a device assignment pointing at a
     * patient that doesn't exist.
     *
     * @return the generated patient_id
     * @throws SQLException on any database error, including constraint
     *                        violations (duplicate SSN, device already
     *                        assigned - see trg_device_assignment_unique)
     */
    public int registerPatient(int deptID, String name, String surname, String ssn,
                               String triageCode, LocalDateTime arrivalTime,
                               LocalDateTime lastVisitTime, String deviceId)
            throws SQLException {

        String insertPatient =
                "INSERT INTO patients (name, surname, ssn, triage_code, arrival_time, last_visit_time) " +
                        "VALUES (?, ?, ?, ?, ?, ?)";

        String insertAssignment =
                "INSERT INTO device_assignments (device_id, patient_id, assigned_at, released_at) " +
                        "VALUES (?, ?, ?, NULL)";

        try (Connection conn = DriverManager.getConnection(url, user, password)) {
            conn.setAutoCommit(false);

            int patientId;

            try (PreparedStatement ps = conn.prepareStatement(
                    insertPatient, Statement.RETURN_GENERATED_KEYS)) {
                ps.setString(1, name);
                ps.setString(2, surname);
                ps.setString(3, ssn);
                ps.setString(4, triageCode);
                ps.setTimestamp(5, Timestamp.valueOf(arrivalTime));
                ps.setTimestamp(6, lastVisitTime != null ? Timestamp.valueOf(lastVisitTime) : null);
                ps.executeUpdate();

                try (ResultSet keys = ps.getGeneratedKeys()) {
                    if (!keys.next()) {
                        throw new SQLException("Insert into patients did not return a generated id");
                    }
                    patientId = keys.getInt(1);
                }
            }

            try (PreparedStatement ps = conn.prepareStatement(insertAssignment)) {
                ps.setString(1, deviceId);
                ps.setInt(2, patientId);
                // Assignment starts now (registration time), which may
                // differ from arrival_time if the patient waited before
                // being handed the device.
                ps.setTimestamp(3, Timestamp.valueOf(LocalDateTime.now()));
                ps.executeUpdate();
            }

            conn.commit();
            return patientId;

        } catch (SQLException e) {
            // NOTE: the connection is closed by try-with-resources
            // regardless, which implicitly rolls back an uncommitted
            // transaction - but being explicit here documents the
            // intent and avoids relying on that implicit behaviour.
            throw e;
        }
    }
}