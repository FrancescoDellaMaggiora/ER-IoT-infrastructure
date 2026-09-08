package it.unipi.CoAP;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.eclipse.californium.core.CoapClient;
import org.eclipse.californium.core.CoapResponse;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.File;
import java.io.IOException;

/**
 * Notifies the PATIENT device that its triage code has been changed
 *
 * CHECK RESOURCE
 *   PUT /er/patient/triage
 *   { "TRIAGE_CODE": <int 1-5> }
 * (same numeric mapping as the registration endpoint, see
 * TriageCodeMapper).
 */
public class PatientTriageUpdateClient {

    static {
        CoapConfig.register();
        File configFile = new File("config/californium.properties");
        Configuration config = Configuration.createStandardWithFile(configFile);
        Configuration.setStandard(config);
    }

    private static final ObjectMapper MAPPER = new ObjectMapper();
    private static final String RESOURCE_PATH = "/er/patient/triage";

    /**
     * @throws IOException if the request fails at the transport level
     * @throws ConnectorException if Californium reports a connector-level error
     */
    public void notifyTriageChanged(String deviceCoapAddress, int triageNumber)
            throws IOException, ConnectorException {

        ObjectNode payload = MAPPER.createObjectNode();
        payload.put("TRIAGE_CODE", triageNumber);

        String uri = deviceCoapAddress + RESOURCE_PATH;
        CoapClient client = new CoapClient(uri);

        CoapResponse response = client.put(MAPPER.writeValueAsBytes(payload),
                org.eclipse.californium.core.coap.MediaTypeRegistry.APPLICATION_JSON);

        if (response == null || !response.isSuccess()) {
            throw new IOException("Patient device did not acknowledge triage update (uri=" + uri + ")");
        }
    }
}