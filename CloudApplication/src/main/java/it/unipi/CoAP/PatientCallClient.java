package it.unipi.CoAP;

import org.eclipse.californium.core.CoapClient;
import org.eclipse.californium.core.CoapResponse;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.File;
import java.io.IOException;

/**
 * Notifies the patient's device: "you have been called for your visit".
 *
 * CHECK: resource and method.
 * Using PUT on /er/patient/called, empty body.
 */
public class PatientCallClient {

    static {
        CoapConfig.register();
        Configuration config = Configuration.createStandardWithFile(
                new File("config/californium.properties"));
        Configuration.setStandard(config);
    }

    private static final String RESOURCE_PATH = "/er/patient/called";

    /** @throws IOException if the device doesn't confirm the reception  */
    public void notifyCalled(String deviceCoapAddress) throws IOException, ConnectorException {
        String uri = deviceCoapAddress + RESOURCE_PATH;
        CoapClient client = new CoapClient(uri);

        CoapResponse response = client.put(new byte[0], 0);   // Empty PUT

        if (response == null || !response.isSuccess()) {
            throw new IOException("Device did not acknowledge call (uri=" + uri + ")");
        }
    }
}