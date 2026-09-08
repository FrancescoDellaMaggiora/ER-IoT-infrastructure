package it.unipi.CoAP;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import it.unipi.Nurse.Nurse;
import it.unipi.Nurse.NurseAssignmentStore;
import it.unipi.Nurse.NurseConfig;
import it.unipi.Patient.TriageCodeMapper;
import it.unipi.Patient.ActivePatientInfo;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.core.CoapResource;
import org.eclipse.californium.core.coap.CoAP;
import org.eclipse.californium.core.coap.MediaTypeRegistry;
import org.eclipse.californium.core.server.resources.CoapExchange;

import java.sql.SQLException;
import java.util.List;

/**
 * GET /er/patient/registration/{DEVICE_ID}
 *
 * Device bootstrap: the patient sensor node calls this once
 * to learn which patient it has been attached to.
 *
 * Response payload, field names EXACTLY as required by the firmware
 * parser:
 *   {
 *     "PATIENT_ID":    <int>,
 *     "TRIAGE_CODE":   <int 1-5, see TriageCodeMapper>,
 *     "NURSE_ADDRESS": "<bare IPv6 address, no brackets, no scheme>"
 *   }
 */
public class RegisterDeviceResource extends CoapResource {

    private static final ObjectMapper MAPPER = new ObjectMapper();

    private final PatientRepository patientRepository;
    private final NurseAssignmentStore nurseStore;
    private final NurseConfig nurseConfig;

    public RegisterDeviceResource(PatientRepository patientRepository,
                                  NurseAssignmentStore nurseStore,
                                  NurseConfig nurseConfig) {
        super("registration");
        this.patientRepository = patientRepository;
        this.nurseStore = nurseStore;
        this.nurseConfig = nurseConfig;
    }

    @Override
    public void handleGET(CoapExchange exchange) {
        List<String> path = exchange.getRequestOptions().getUriPath();
        String deviceId = path.getLast();   // last segment = DEVICE_ID

        if (deviceId.isBlank()) {
            exchange.respond(CoAP.ResponseCode.BAD_REQUEST, "Missing DEVICE_ID");
            return;
        }

        ActivePatientInfo info;
        try {
            info = patientRepository.findActivePatientByDevice(deviceId);
        } catch (SQLException e) {
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR,
                    "Database error: " + e.getMessage());
            return;
        }

        if (info == null) {
            exchange.respond(CoAP.ResponseCode.NOT_FOUND,
                    "No active patient for device " + deviceId);
            return;
        }

        String nurseId = nurseStore.getNurseIdForPatient(info.getPatientId());
        if (nurseId == null) {
            // Should not happen if registration always succeeds in
            // assigning a nurse - reported anyway, not silently ignored.
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR,
                    "No nurse assigned to patient " + info.getPatientId());
            return;
        }

        Nurse nurse = nurseConfig.getById(nurseId);
        if (nurse == null) {
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR,
                    "Unknown nurse id " + nurseId);
            return;
        }

        int triageNumber;
        try {
            triageNumber = TriageCodeMapper.toNumber(info.getTriageCode());
        } catch (IllegalArgumentException e) {
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR, e.getMessage());
            return;
        }

        ObjectNode payload = MAPPER.createObjectNode();
        payload.put("PATIENT_ID", info.getPatientId());
        payload.put("TRIAGE_CODE", triageNumber);
        payload.put("NURSE_ADDRESS", nurse.getIpAddress());

        try {
            byte[] body = MAPPER.writeValueAsBytes(payload);
            exchange.respond(CoAP.ResponseCode.CONTENT, body, MediaTypeRegistry.APPLICATION_JSON);
        } catch (Exception e) {
            exchange.respond(CoAP.ResponseCode.INTERNAL_SERVER_ERROR, "Serialization error");
        }
    }

    @Override
    public org.eclipse.californium.core.server.resources.Resource getChild(String name) {
        // Accepts any extra segment after "registration" (the DEVICE_ID)
        // as part of this same resource, instead of looking for an explicitly
        // declared child - handleGET() then reads the segment
        // from the full URI-Path.
        return this;
    }
}