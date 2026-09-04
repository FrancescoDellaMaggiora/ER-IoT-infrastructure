package it.unipi.CoAP;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import it.unipi.Nurse.Nurse;
import it.unipi.Nurse.NurseAssignmentStore;
import it.unipi.Nurse.NurseConfig;
import it.unipi.Nurse.TriageCodeMapper;
import it.unipi.Repository.ActivePatientInfo;
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
 * Device bootstrap: the patient sensor node calls this once, ~1s after
 * boot, to learn which patient it has been attached to.
 *
 * Response payload, field names EXACTLY as required by the firmware
 * parser (parse_registration() in the device's client code):
 *   {
 *     "PATIENT_ID":    <int>,
 *     "TRIAGE_CODE":   <int 1-5, see TriageCodeMapper / sensor.h>,
 *     "NURSE_ADDRESS": "<bare IPv6 address, no brackets, no scheme>"
 *   }
 *
 * DEVICE_ID in the firmware is a plain integer (see snprintf("%i", ...)
 * in the client code); by decision, doctors register patients using
 * that same plain numeric string as id_device, so no normalization is
 * needed here - the device_id column comparison is a direct string match.
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
        // Accetta qualunque segmento extra dopo "registration" (il DEVICE_ID)
        // come parte di questa stessa risorsa, invece di cercare un figlio
        // dichiarato esplicitamente - handleGET() legge poi il segmento
        // dall'URI-Path completo.
        return this;
    }
}