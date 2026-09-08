package it.unipi.CoAP;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import it.unipi.Nurse.Nurse;
import it.unipi.Nurse.NurseAssignmentService;
import it.unipi.Patient.TriageCodeMapper;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.core.CoapResource;
import org.eclipse.californium.core.coap.CoAP;
import org.eclipse.californium.core.server.resources.CoapExchange;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.IOException;
import java.sql.SQLException;

/**
 * PUT /er/patient/triage-report
 *
 * The patient device can autonomously decide to change its own triage
 * code and reports it here. Direction is DEVICE -> CLOUD, the opposite of
 * PatientTriageUpdateClient (CLOUD -> device, for doctor-initiated
 * changes).
 *
 * Expected request payload (device -> Cloud):
 *   { "PATIENT_ID": <int>, "TRIAGE_CODE": <int 1-5> }
 *
 * On success: updates MySQL and notifies the nurse device on the DEDICATED
 * triage-update resource (same one used for doctor-initiated changes -
 * from the nurse's point of view, "the code changed" is the same event
 * regardless of who triggered it).
 *
 * The device itself is NOT notified back: it already knows its own new
 * state, it's the one that decided it.
 */
public class TriageReportResource extends CoapResource {

    private static final ObjectMapper MAPPER = new ObjectMapper();

    private final PatientRepository patientRepository;
    private final NurseAssignmentService nurseAssignmentService;
    private final NurseTriageUpdateClient nurseTriageUpdateClient;

    public TriageReportResource(PatientRepository patientRepository,
                                NurseAssignmentService nurseAssignmentService,
                                NurseTriageUpdateClient nurseTriageUpdateClient) {
        super("triage-report");
        this.patientRepository = patientRepository;
        this.nurseAssignmentService = nurseAssignmentService;
        this.nurseTriageUpdateClient = nurseTriageUpdateClient;
    }

    @Override
    public void handlePUT(CoapExchange exchange) {
        JsonNode payload;
        try {
            payload = MAPPER.readTree(exchange.getRequestPayload());
        } catch (IOException e) {
            exchange.respond(CoAP.ResponseCode.BAD_REQUEST, "Malformed JSON payload");
            return;
        }

        if (!payload.hasNonNull("PATIENT_ID") || !payload.hasNonNull("TRIAGE_CODE")) {
            exchange.respond(CoAP.ResponseCode.BAD_REQUEST, "Missing PATIENT_ID or TRIAGE_CODE");
            return;
        }

        int patientId = payload.get("PATIENT_ID").asInt();
        int triageNumber = payload.get("TRIAGE_CODE").asInt();

        String triageCode;
        try {
            triageCode = TriageCodeMapper.toCode(triageNumber);
        } catch (IllegalArgumentException e) {
            exchange.respond(CoAP.ResponseCode.BAD_REQUEST, "Invalid TRIAGE_CODE: " + triageNumber);
            return;
        }

        // 1. Update MySQL
        boolean updated;
        try {
            updated = patientRepository.updateTriageCode(patientId, triageCode);
        } catch (SQLException e) {
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR, "Database error: " + e.getMessage());
            return;
        }

        if (!updated) {
            exchange.respond(CoAP.ResponseCode.NOT_FOUND, "Unknown PATIENT_ID " + patientId);
            return;
        }

        // Respond to the device BEFORE attempting the nurse notification:
        // the device only needs to know its report was accepted, and
        // shouldn't wait on a second CoAP round-trip to a different host.
        exchange.respond(CoAP.ResponseCode.CHANGED);

        // 2. Notify the nurse device.
        Nurse nurse = nurseAssignmentService.getNurseForPatient(patientId);
        if (nurse != null) {
            try {
                nurseTriageUpdateClient.notifyTriageChanged(nurse, patientId, triageNumber);
            } catch (IOException | ConnectorException e) {
                System.err.println("Warning: nurse " + nurse.getNurseId()
                        + " was not notified of autonomous triage change: " + e.getMessage());
            }
        } else {
            System.err.println("Warning: no nurse on record for patient " + patientId
                    + " (autonomous triage report)");
        }
    }
}