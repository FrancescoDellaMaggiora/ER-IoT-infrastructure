package it.unipi;

import it.unipi.CoAP.*;
import it.unipi.Doctor.DoctorApiServer;
import it.unipi.MQTT.MQTTThread;
import it.unipi.MQTT.Vitals;
import it.unipi.Nurse.NurseAssignmentService;
import it.unipi.Nurse.NurseAssignmentStore;
import it.unipi.Nurse.NurseConfig;
import it.unipi.Repository.PatientRepository;
import it.unipi.Storage.StorageThread;
import org.eclipse.paho.client.mqttv3.MqttException;
import it.unipi.Patient.*;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.stream.StreamSupport;

public class Main {

    private static final int DOCTOR_API_PORT = 7000;
    private static final String NURSES_CONFIG_PATH = "config/nurses.json";
    private static final String DEVICES_CONFIG_PATH = "config/devices.json";
    private static final String MQTT_CONFIG_PATH = "config/mqtt.properties";
    private static final String DB_CONFIG_PATH = "config/database.properties";
    private static final String STORAGE_CONFIG_PATH = "config/storage.properties";


    private static final int QUEUE_CAPACITY = 1000;

    public static void main(String[] args) {

        BlockingQueue<Vitals> vitalsQueue = new LinkedBlockingQueue<>(QUEUE_CAPACITY);

        final MQTTThread mqttThread;
        final StorageThread storageThread;
        final DoctorApiServer doctorApiServer;
        final CloudCoapServer cloudCoapServer;

        System.out.println("Starting MQTTThread");
        try {
            mqttThread = new MQTTThread(MQTT_CONFIG_PATH, vitalsQueue);
            mqttThread.start();
        } catch (IOException e) {
            System.out.println("MQTTThread: Error reading config file");
            e.printStackTrace();
            System.exit(1);
            return;
        } catch (RuntimeException mqttException) {
            System.out.println("MQTT Exception: " + mqttException.getMessage());
            mqttException.printStackTrace();
            System.exit(1);
            return;
        }

        System.out.println("Starting StorageThread");
        try {
            storageThread = new StorageThread(STORAGE_CONFIG_PATH, vitalsQueue);
            storageThread.start();
        } catch (IOException e) {
            mqttThread.shutdown();
            System.out.println("StorageThread: Error reading config file");
            e.printStackTrace();
            System.exit(1);
            return;
        }

        System.out.println("Starting DoctorApiServer + CloudCoapServer");
        try {
            PatientRepository patientRepository = new PatientRepository(DB_CONFIG_PATH);

            // In-memory: no DB config to read, only the static nurses
            // JSON (can still fail if the file is missing/malformed).
            NurseConfig nurseConfig = new NurseConfig(NURSES_CONFIG_PATH);
            NurseAssignmentStore nurseStore = new NurseAssignmentStore();
            NurseAssignmentService nurseAssignmentService =
                    new NurseAssignmentService(nurseConfig, nurseStore);

            DeviceConfig deviceConfig = new DeviceConfig(DEVICES_CONFIG_PATH);

            PatientAssociationClient patientAssociationClient = new PatientAssociationClient();
            PatientCallClient patientCallClient = new PatientCallClient();
            PatientTriageUpdateClient patientTriageUpdateClient = new PatientTriageUpdateClient();
            NurseTriageUpdateClient nurseTriageUpdateClient = new NurseTriageUpdateClient();
            NextPatientSelector nextPatientSelector = new NextPatientSelector(patientRepository);
            NurseDischargeClient nurseDischargeClient = new NurseDischargeClient();
            PatientDischargeClient patientDischargeClient = new PatientDischargeClient();

            doctorApiServer = new DoctorApiServer(DOCTOR_API_PORT, patientRepository,
                    nurseAssignmentService, patientAssociationClient,
                    nextPatientSelector, deviceConfig, patientCallClient,
                    patientTriageUpdateClient, nurseTriageUpdateClient,
                    patientDischargeClient, nurseDischargeClient);
            doctorApiServer.start();

            cloudCoapServer = new CloudCoapServer(
                    patientRepository, nurseStore,
                    nurseConfig,nurseAssignmentService,
                    nurseTriageUpdateClient);
            cloudCoapServer.start();

        } catch (IOException e) {
            mqttThread.shutdown();
            storageThread.shutdown();
            System.out.println("DoctorApiServer/CloudCoapServer: Error reading config file");
            e.printStackTrace();
            System.exit(1);
            return;
        }

        //  Adding the call to shut down the system
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("Shutting down...");
            storageThread.shutdown();
            try {
                storageThread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            mqttThread.shutdown();

            doctorApiServer.stop();

            cloudCoapServer.stop();

        }));
    }
}