package it.unipi.CoAP;

import it.unipi.Nurse.NurseAssignmentService;
import it.unipi.Nurse.NurseAssignmentStore;
import it.unipi.Nurse.NurseConfig;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.core.CoapResource;
import org.eclipse.californium.core.CoapServer;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;

import java.io.File;

/**
 * CoAP Server: memorize the resources to the device.
 * Resources:
 *   er/patient/registration/{DEVICE_ID}  (GET)  -> RegisterDeviceResource
 *   er/patient/triage-report             (PUT)  -> TriageReportResource
 *                                                    (device reports an
 *                                                     autonomous triage
 *                                                     change)
 */
public class CloudCoapServer {

    static {
        CoapConfig.register();
        File configFile = new File("config/californium.properties");
        Configuration config = Configuration.createStandardWithFile(configFile);
        Configuration.setStandard(config);
    }

    private final CoapServer server;

    public CloudCoapServer(PatientRepository patientRepository,
                           NurseAssignmentStore nurseStore,
                           NurseConfig nurseConfig,
                           NurseAssignmentService nurseAssignmentService,
                           NurseTriageUpdateClient nurseTriageUpdateClient) {
        server = new CoapServer();

        CoapResource er = new CoapResource("er");
        CoapResource patient = new CoapResource("patient");
        RegisterDeviceResource registration = new RegisterDeviceResource(
                patientRepository, nurseStore, nurseConfig);
        TriageReportResource triageReport = new TriageReportResource(
                patientRepository, nurseAssignmentService, nurseTriageUpdateClient);
        patient.add(registration);
        patient.add(triageReport);
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