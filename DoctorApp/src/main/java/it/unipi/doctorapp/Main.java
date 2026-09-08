package it.unipi.doctorapp;

import it.unipi.doctorapp.client.CloudApiClient;
import it.unipi.doctorapp.client.CloudApiException;
import it.unipi.doctorapp.model.*;
import it.unipi.doctorapp.ui.BrowserLauncher;
import it.unipi.doctorapp.ui.ConsoleInput;

import java.io.IOException;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.List;
import java.util.Scanner;

/**
 * DoctorApp - command-line interface for the ER doctor.
 *
 * Talks to the Cloud Application's four doctor endpoints:
 *   POST /er/doctor/register       -register a newly triaged patient
 *   POST /er/doctor/next-patient   -call the next patient due a revisit
 *   POST /er/doctor/update-triage  -change a patient's triage code
 *   POST /er/doctor/discharge      -discharge a patient
 *
 * After a successful "next patient", the Grafana dashboard for that
 * patient is opened in the browser.
 */
public class Main {

    private static final String CONFIG_PATH = "config/doctorapp.properties";

    /** The 5 triage codes the Cloud accepts (must match the DB enum). */
    private static final List<String> TRIAGE_CODES =
            List.of("red", "orange", "blue", "green", "white");

    public static void main(String[] args) {

        AppConfig config;
        try {
            config = new AppConfig(CONFIG_PATH);
        } catch (IOException e) {
            System.err.println("Failed to load " + CONFIG_PATH + ": " + e.getMessage());
            System.exit(1);
            return;
        }

        CloudApiClient client = new CloudApiClient(config.getBaseUrl());
        Scanner scanner = new Scanner(System.in);
        ConsoleInput input = new ConsoleInput(scanner);

        System.out.println("=== ER DoctorApp ===");
        System.out.println("Cloud: " + config.getBaseUrl());
        System.out.println("Department: " + config.getDeptId());

        boolean running = true;
        while (running) {
            printMenu();
            String choice = input.readRequired("> ");

            switch (choice) {
                case "1" -> handleRegister(client, input, config);
                case "2" -> handleNextPatient(client, config);
                case "3" -> handleUpdateTriage(client, input);
                case "4" -> handleDischarge(client, input);
                case "5" -> running = false;
                default -> System.out.println("Unknown option.");
            }
        }

        System.out.println("Bye.");
    }

    private static void printMenu() {
        System.out.println();
        System.out.println("1) Register a patient");
        System.out.println("2) Call next patient");
        System.out.println("3) Update a patient's triage code");
        System.out.println("4) Discharge a patient");
        System.out.println("5) Quit");
    }

    private static void handleRegister(CloudApiClient client, ConsoleInput input, AppConfig config) {
        RegisterPatientRequest request = new RegisterPatientRequest();

        request.name = input.readRequired("Name: ");
        request.surname = input.readRequired("Surname: ");
        request.ssn = input.readRequired("SSN: ");
        request.triageCode = input.readChoice(
                "Triage code (" + String.join("/", TRIAGE_CODES) + "): ", TRIAGE_CODES);
        request.deviceId = input.readRequired("Device id: ");

        // Arrival time defaults to now - typing a full ISO timestamp by
        // hand for the common case (patient just arrived) would be
        // needless friction.
        String arrival = input.readOptional("Arrival time ISO-8601 [empty = now]: ");
        if (arrival == null) {
            request.arrivalTime = LocalDateTime.now().format(DateTimeFormatter.ISO_LOCAL_DATE_TIME);
        } else {
            try {
                // Parse locally so a typo is caught before the round-trip.
                LocalDateTime.parse(arrival);
                request.arrivalTime = arrival;
            } catch (DateTimeParseException e) {
                System.out.println("Invalid date format, expected e.g. 2026-09-06T18:30:00");
                return;
            }
        }

        // last_visit_time stays null: a patient being registered has
        // just been triaged, they have no previous visit yet.
        request.lastVisitTime = null;
        request.deptId = config.getDeptId();

        try {
            RegisterPatientResponse response = client.registerPatient(request);
            System.out.println("Registered. Patient id " + response.patientId
                    + ", assigned to nurse " + response.nurseId + ".");
        } catch (CloudApiException e) {
            System.out.println("Failed (" + e.getStatusCode() + "): " + e.getMessage());
        } catch (IOException | InterruptedException e) {
            System.out.println("Could not reach the Cloud: " + e.getMessage());
        }
    }

    private static void handleNextPatient(CloudApiClient client, AppConfig config) {
        try {
            NextPatientResponse response = client.callNextPatient(config.getDeptId());
            System.out.println("Called patient " + response.patientId
                    + " (device " + response.deviceId + ").");

            // Open that patient's dashboard.
            String url = config.getGrafanaDashboardUrl()
                    .replace("{patientId}", String.valueOf(response.patientId));
            BrowserLauncher.open(url);

        } catch (CloudApiException e) {
            System.out.println("Failed (" + e.getStatusCode() + "): " + e.getMessage());
        } catch (IOException | InterruptedException e) {
            System.out.println("Could not reach the Cloud: " + e.getMessage());
        }
    }

    private static void handleUpdateTriage(CloudApiClient client, ConsoleInput input) {
        int patientId = input.readInt("Patient id: ");
        String triageCode = input.readChoice(
                "New triage code (" + String.join("/", TRIAGE_CODES) + "): ", TRIAGE_CODES);

        try {
            UpdateTriageResponse response = client.updateTriage(
                    String.valueOf(patientId), triageCode);
            System.out.println("Patient " + response.patientId
                    + " is now " + response.triageCode + ".");
        } catch (CloudApiException e) {
            System.out.println("Failed (" + e.getStatusCode() + "): " + e.getMessage());
        } catch (IOException | InterruptedException e) {
            System.out.println("Could not reach the Cloud: " + e.getMessage());
        }
    }

    private static void handleDischarge(CloudApiClient client, ConsoleInput input) {
        int patientId = input.readInt("Patient id to discharge: ");

        // Discharge frees the device and clears the nurse assignment -
        // worth a confirmation before firing it.
        String confirm = input.readRequired("Discharge patient " + patientId + "? (y/n): ");
        if (!confirm.equalsIgnoreCase("y")) {
            System.out.println("Cancelled.");
            return;
        }

        try {
            DischargeResponse response = client.dischargePatient(String.valueOf(patientId));
            System.out.println("Patient " + response.patientId + " discharged. Device "
                    + response.deviceId + " is free again (remember to collect and power it off).");
        } catch (CloudApiException e) {
            System.out.println("Failed (" + e.getStatusCode() + "): " + e.getMessage());
        } catch (IOException | InterruptedException e) {
            System.out.println("Could not reach the Cloud: " + e.getMessage());
        }
    }
}