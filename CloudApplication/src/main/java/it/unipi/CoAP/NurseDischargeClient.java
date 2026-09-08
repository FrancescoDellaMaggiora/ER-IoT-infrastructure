package it.unipi.CoAP;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import it.unipi.Nurse.Nurse;
import org.eclipse.californium.core.CoapClient;
import org.eclipse.californium.core.CoapResponse;
import org.eclipse.californium.core.coap.MediaTypeRegistry;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.File;
import java.io.IOException;

/**
 * Notifies the NURSE device that a patient has been discharged, on a
 * resource DEDICATED to this event (not the association or
 * triage-update ones - discharge removes the patient from the nurse's
 * list entirely, a different kind of event).
 *
 * Resource path, method, payload:
 *   PUT /er/patient/discharge
 *   { "PATIENT_ID": <int> }
 */
public class NurseDischargeClient {

    static {
        CoapConfig.register();
        File configFile = new File("config/californium.properties");
        Configuration config = Configuration.createStandardWithFile(configFile);
        Configuration.setStandard(config);
    }

    private static final ObjectMapper MAPPER = new ObjectMapper();
    private static final String RESOURCE_PATH = "/er/patient/discharge";

    /**
     * @throws IOException if the request fails at the transport level
     * @throws ConnectorException if Californium reports a connector-level error
     */
    public void notifyDischarged(Nurse nurse, int patientId) throws IOException, ConnectorException {
        ObjectNode payload = MAPPER.createObjectNode();
        payload.put("PATIENT_ID", patientId);

        String uri = nurse.getCoapAddress() + RESOURCE_PATH;
        CoapClient client = new CoapClient(uri);

        CoapResponse response = client.put(MAPPER.writeValueAsBytes(payload),
                MediaTypeRegistry.APPLICATION_JSON);

        if (response == null || !response.isSuccess()) {
            throw new IOException("Nurse device did not acknowledge discharge (uri=" + uri + ")");
        }
    }
}