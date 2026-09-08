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
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;

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

    /*
    * Notifications to the nurse run here, NOT on the CoAP handler thread.
    * Californium serves this resource on a single-threaded executor, and
    * NurseTriageUpdateClient.notifyTriageChanged() blocks waiting for the
    * nurse's reply - a reply that the very same thread would have to
    * process. Doing it inline deadlocks: the device gets its 2.04 and the
    * database is updated, but the nurse is never told and no warning is
    * ever printed, because the code never gets past the blocking call.
    */
    private final ExecutorService notifier;

    public TriageReportResource(PatientRepository patientRepository,
                                NurseAssignmentService nurseAssignmentService,
                                NurseTriageUpdateClient nurseTriageUpdateClient) {
        super("triage-report");
        this.patientRepository = patientRepository;
        this.nurseAssignmentService = nurseAssignmentService;
        this.nurseTriageUpdateClient = nurseTriageUpdateClient;

        this.notifier = Executors.newSingleThreadExecutor();
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

        // Respond to the device BEFORE attempting the nurse notification.
        exchange.respond(CoAP.ResponseCode.CHANGED);

        // 2. Hand the notification off to another thread (see the comment
        // on 'notifier' above).
        final int reportedPatientId = patientId;
        final int reportedTriageNumber = triageNumber;

        this.notifier.submit(() -> {
            // Notify the nurse device.
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
        });
    }
}