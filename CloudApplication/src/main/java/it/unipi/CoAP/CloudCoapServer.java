package it.unipi.CoAP;

import it.unipi.Nurse.NurseAssignmentStore;
import it.unipi.Nurse.NurseConfig;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.core.CoapResource;
import org.eclipse.californium.core.CoapServer;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;

/**
 * CoAP Server: memorize the resources to the device.
 * Resources:
 *   er/patient/registration/{DEVICE_ID}  (GET)  -> RegisterDeviceResource
 */
public class CloudCoapServer {

    static {
        Configuration.setStandard(Configuration.createStandardWithoutFile());
        CoapConfig.register();
    }

    private final CoapServer server;

    public CloudCoapServer(PatientRepository patientRepository,
                           NurseAssignmentStore nurseStore,
                           NurseConfig nurseConfig) {
        server = new CoapServer();

        CoapResource er = new CoapResource("er");
        CoapResource patient = new CoapResource("patient");
        RegisterDeviceResource registration = new RegisterDeviceResource(
                patientRepository, nurseStore, nurseConfig);

        patient.add(registration);
        er.add(patient);
        server.add(er);
    }

    public void start() {
        server.start();
    }

    public void stop() {
        server.stop();
    }
}