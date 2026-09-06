package it.unipi.CoAP;

import org.eclipse.californium.core.CoapClient;
import org.eclipse.californium.core.CoapResponse;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.File;
import java.io.IOException;

/**
 * Notifies the PATIENT device that the patient has been discharged.
 *
 * This does NOT power the device off - staff still has to physically
 * do that when they retrieve the device. It exists to close the gap
 * between "discharged in the Cloud" and "physically unplugged": without
 * it, the device would keep publishing vitals and showing a stale
 * triage LED for a patient the Cloud no longer considers active.
 */
public class PatientDischargeClient {

    static {
        CoapConfig.register();
        File configFile = new File("config/californium.properties");
        Configuration config = Configuration.createStandardWithFile(configFile);
        Configuration.setStandard(config);
    }

    private static final String RESOURCE_PATH = "/er/patient/discharge";

    /**
     * @throws IOException if the request fails at the transport level
     * @throws ConnectorException if Californium reports a connector-level error
     */
    public void notifyDischarged(String deviceCoapAddress) throws IOException, ConnectorException {
        String uri = deviceCoapAddress + RESOURCE_PATH;
        CoapClient client = new CoapClient(uri);

        CoapResponse response = client.put(new byte[0], 0);

        if (response == null || !response.isSuccess()) {
            throw new IOException("Patient device did not acknowledge discharge (uri=" + uri + ")");
        }
    }
}