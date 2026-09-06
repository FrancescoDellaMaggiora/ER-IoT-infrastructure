package it.unipi.Repository;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.sql.*;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import it.unipi.Patient.*;

/**
 * Data access layer for patients and device assignments.
 *
 * Owns all SQL for the doctor-facing endpoints.
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
            throw e;
        }
    }

    /**
     * Looks up the patient currently (actively) assigned to a given
     * device. Used by the device bootstrap endpoint
     * (GET /er/patient/registration/{DEVICE_ID}).
     *
     * @return info of the patient with an ACTIVE device assignment, or
     *         null if no active assignment exists for this device_id
     */
    public ActivePatientInfo findActivePatientByDevice(String deviceId) throws SQLException {
        String sql = "SELECT p.patient_id, p.triage_code " +
                "FROM patients p " +
                "JOIN device_assignments da ON da.patient_id = p.patient_id " +
                "WHERE da.device_id = ? AND da.released_at IS NULL";

        try (Connection conn = DriverManager.getConnection(url, user, password);
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, deviceId);
            try (ResultSet rs = ps.executeQuery()) {
                if (!rs.next()) {
                    return null;
                }
                return new ActivePatientInfo(rs.getInt("patient_id"), rs.getString("triage_code"));
            }
        }
    }
    
    /**
     * Patient still "active" (with a device and still in the ER) in a department
     * with the last visit's time.
     * 
     * @param deptId Department that are we checking
     * @return ArrayList of WaitingPatient. 
     * @throws SQLException whenever something goes wrong
     */
    public List<WaitingPatient> findActivePatientsInDept(int deptId) throws SQLException {
        String sql = "SELECT p.patient_id, da.device_id, p.triage_code, " +
                "       COALESCE(p.last_visit_time, p.arrival_time) AS last_check " +
                "FROM patients p " +
                "JOIN device_assignments da ON da.patient_id = p.patient_id " +
                "WHERE p.dept_id = ? AND da.released_at IS NULL";

        List<WaitingPatient> result = new ArrayList<>();
        try (Connection conn = DriverManager.getConnection(url, user, password);
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setInt(1, deptId);
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    result.add(new WaitingPatient(
                            rs.getInt("patient_id"),
                            rs.getString("device_id"),
                            rs.getString("triage_code"),
                            rs.getTimestamp("last_check").toLocalDateTime()));
                }
            }
        }
        return result;
    }

    /**
     * Update to 'when' the last_visit_time of the patient 'patient_id'
     * @param patientId Patient's ID that want to update
     * @param when Timestamp of the visit
     * @throws SQLException  whenever something goes wrong
     */
    public void updateLastVisitTime(int patientId, LocalDateTime when) throws SQLException {
        String sql = "UPDATE patients SET last_visit_time = ? WHERE patient_id = ?";
        try (Connection conn = DriverManager.getConnection(url, user, password);
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setTimestamp(1, Timestamp.valueOf(when));
            ps.setInt(2, patientId);
            ps.executeUpdate();
        }
    }

    /**
     * @return the device_id of the patient's ACTIVE device assignment,
     *         or null if the patient has no active assignment (e.g.
     *         unknown patient_id, or already discharged)
     */
    public String findActiveDeviceForPatient(int patientId) throws SQLException {
        String sql = "SELECT device_id FROM device_assignments " +
                "WHERE patient_id = ? AND released_at IS NULL";

        try (Connection conn = DriverManager.getConnection(url, user, password);
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setInt(1, patientId);
            try (ResultSet rs = ps.executeQuery()) {
                return rs.next() ? rs.getString("device_id") : null;
            }
        }
    }

    /**
     * Updates a patient's triage code. The trg_triage_history_update
     * trigger fires automatically on the DB side, appending a row to
     * triage_history if the value actually changed - no application
     * code needed for that part.
     *
     * @return true if a row was actually updated (patient_id existed), false otherwise
     */
    public boolean updateTriageCode(int patientId, String newTriageCode) throws SQLException {
        String sql = "UPDATE patients SET triage_code = ? WHERE patient_id = ?";
        try (Connection conn = DriverManager.getConnection(url, user, password);
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, newTriageCode);
            ps.setInt(2, patientId);
            return ps.executeUpdate() > 0;
        }
    }

    /**
     * Discharges a patient: closes their ACTIVE device assignment
     * (released_at = now). This is the only DB-side signal needed - a
     * patient with no active device_assignments row is no longer
     * "active" anywhere else in the schema
     * Also frees the device_id for reassignment.
     *
     * @return the device_id that was released, or null if the patient
     *         had no active assignment (unknown patient_id, or already
     *         discharged)
     */
    public String releaseActiveDeviceForPatient(int patientId) throws SQLException {
        String selectSql = "SELECT device_id FROM device_assignments " +
                "WHERE patient_id = ? AND released_at IS NULL";
        String updateSql = "UPDATE device_assignments SET released_at = ? " +
                "WHERE patient_id = ? AND released_at IS NULL";

        try (Connection conn = DriverManager.getConnection(url, user, password)) {
            String deviceId;
            try (PreparedStatement ps = conn.prepareStatement(selectSql)) {
                ps.setInt(1, patientId);
                try (ResultSet rs = ps.executeQuery()) {
                    if (!rs.next()) {
                        return null;
                    }
                    deviceId = rs.getString("device_id");
                }
            }

            try (PreparedStatement ps = conn.prepareStatement(updateSql)) {
                ps.setTimestamp(1, Timestamp.valueOf(LocalDateTime.now()));
                ps.setInt(2, patientId);
                ps.executeUpdate();
            }

            return deviceId;
        }
    }
}