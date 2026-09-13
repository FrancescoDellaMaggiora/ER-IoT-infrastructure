package it.unipi.CoAP;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import it.unipi.Nurse.Nurse;
import it.unipi.Patient.TriageCodeMapper;
import org.eclipse.californium.core.CoapClient;
import org.eclipse.californium.core.CoapResponse;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.File;
import java.io.IOException;
import java.time.LocalDateTime;
import java.time.ZoneOffset;

/**
 * Sends the patient/nurse association (message towards the nurse device,
 * resource /er/patient/association) via CoAP.
 */
public class PatientAssociationClient {

    private static final ObjectMapper MAPPER = new ObjectMapper();
    private static final String RESOURCE_PATH = "/er/patient/association";

    static {
        CoapConfig.register();
        File configFile = new File("config/californium.properties");
        Configuration config = Configuration.createStandardWithFile(configFile);
        Configuration.setStandard(config);
    }

    private static int triageCodeToNumber(String triageCode) {
        switch (triageCode) {
            case "red":    return 0;
            case "yellow": return 1;
            case "green":  return 2;
            case "blue":   return 3;
            case "white":  return 4;
            default: throw new IllegalArgumentException("Unknown triage code: " + triageCode);
        }
    }

    private static long toEpochSeconds(LocalDateTime dt) {
        return dt.toEpochSecond(ZoneOffset.UTC);
    }

    /**
     * @throws IOException if the CoAP request fails or times out
     * @throws ConnectorException if the CoAP request fails or times out
     */
    public void notifyAssociation(Nurse nurse, int patientId, String ssn, String name,
                                  String surname, String triageCode,
                                  LocalDateTime arrivalTime, LocalDateTime lastVisitTime)
            throws IOException, ConnectorException {

        ObjectNode payload = MAPPER.createObjectNode();
        payload.put("PATIENT_ID", patientId);
        payload.put("SSN", ssn);
        payload.put("NAME", name);
        payload.put("SURNAME", surname);
        payload.put("TRIAGE_CODE", TriageCodeMapper.toNumber(triageCode));
        payload.put("RECEPTION_TIMESTAMP", toEpochSeconds(arrivalTime));
        payload.put("LAST_VISIT_TIMESTAMP",
                lastVisitTime != null ? toEpochSeconds(lastVisitTime) : null);

        String uri = nurse.getCoapAddress() + RESOURCE_PATH;
        CoapClient client = new CoapClient(uri);

        CoapResponse response = client.post(MAPPER.writeValueAsBytes(payload),
                org.eclipse.californium.core.coap.MediaTypeRegistry.APPLICATION_JSON);

        if (response == null || !response.isSuccess()) {
            throw new java.io.IOException("Nurse device did not acknowledge association (uri=" + uri + ")");
        }
    }
}