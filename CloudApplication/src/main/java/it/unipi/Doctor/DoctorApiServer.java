package it.unipi.Doctor;

import io.javalin.Javalin;
import io.javalin.http.Context;
import it.unipi.CoAP.PatientAssociationClient;
import it.unipi.CoAP.PatientCallClient;
import it.unipi.Nurse.Nurse;
import it.unipi.Nurse.NurseAssignmentService;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.IOException;
import java.sql.Connection;
import java.sql.SQLException;
import java.time.LocalDateTime;
import java.time.format.DateTimeParseException;

import it.unipi.Patient.*;

/**
 * HTTP server exposing the doctor-facing REST endpoints.
 *
 * Owns its own port/routing, but delegates all actual database work to
 * PatientRepository - this class is only responsible for HTTP concerns
 * (parsing the request, choosing status codes, shaping the response),
 * not for SQL.
 */
public class DoctorApiServer {

    private final int port;
    private final Javalin app;

    private final DeviceConfig deviceConfig;
    private final PatientRepository patientRepository;
    private final PatientCallClient patientCallClient;
    private final NextPatientSelector nextPatientSelector;
    private final NurseAssignmentService nurseAssignmentService;
    private final PatientAssociationClient patientAssociationClient;


    public DoctorApiServer(int port, PatientRepository patientRepository,
                           NurseAssignmentService nurseAssignmentService,
                           PatientAssociationClient patientAssociationClient,
                           NextPatientSelector nextPatientSelector,
                           DeviceConfig deviceConfig,
                           PatientCallClient patientCallClient) {
        this.port = port;
        this.patientRepository = patientRepository;
        this.nurseAssignmentService = nurseAssignmentService;
        this.patientAssociationClient = patientAssociationClient;
        this.nextPatientSelector = nextPatientSelector;
        this.deviceConfig = deviceConfig;
        this.patientCallClient = patientCallClient;

        this.app = Javalin.create(config -> {
            config.routes.post("/er/doctor/register", this::handleRegister);
            config.routes.post("/er/doctor/next-patient", this::handleNextPatient);
        });
        ;
    }

    public void start() {
        app.start(port);
    }

    public void stop() {
        app.stop();
    }

    private void handleRegister(Context ctx) {
        RegisterPatientRequest req = ctx.bodyAsClass(RegisterPatientRequest.class);

        // Basic validation: fail fast with 400, don't let a malformed
        // request reach the database layer at all.
        if (req.getName() == null || req.getSurname() == null
                || req.getSsn() == null || req.getTriageCode() == null
                || req.getArrivalTime() == null || req.getDeviceId() == null
                || req.getDeptId() == null) {
            ctx.status(400).json(new ErrorResponse("Missing required field"));
            return;
        }

        int deptId;
        LocalDateTime arrivalTime;
        LocalDateTime lastVisitTime = null;
        try {
            deptId = Integer.parseInt(req.getDeptId());
            arrivalTime = LocalDateTime.parse(req.getArrivalTime());
            if (req.getLastVisitTime() != null) {
                lastVisitTime = LocalDateTime.parse(req.getLastVisitTime());
            }
        } catch (NumberFormatException e) {
            ctx.status(400).json(new ErrorResponse("Invalid id_dept, expected an integer"));
            return;
        } catch (DateTimeParseException e) {
            ctx.status(400).json(new ErrorResponse("Invalid date format, expected ISO-8601"));
            return;
        }

        int patientId;
        try {
            patientId = patientRepository.registerPatient(
                    deptId, req.getName(), req.getSurname(), req.getSsn(),
                    req.getTriageCode(), arrivalTime, lastVisitTime, req.getDeviceId());
        } catch (SQLException e) {
            // TODO: distinguish "SSN already exists" / "device already
            // assigned" (constraint violations -> 409 Conflict) from
            // genuine DB failures (-> 500). For now, everything is 500.
            ctx.status(500).json(new ErrorResponse("Database error: " + e.getMessage()));
            return;
        }

        Nurse nurse;
        try {
            nurse = nurseAssignmentService.assignNurseForPatient(deptId, patientId);
        } catch (IllegalStateException e) {
            // "No nurses configured for department X" - the patient IS
            // already registered in MySQL at this point (no rollback,
            // same trade-off as the CoAP notification below).
            ctx.status(500).json(new ErrorResponse(e.getMessage()));
            return;
        }

        // Notifies the nurse's device BEFORE responding to the doctor, as requested
        try {
            patientAssociationClient.notifyAssociation(nurse, patientId, req.getSsn(),
                    req.getName(), req.getSurname(), req.getTriageCode(),
                    arrivalTime, lastVisitTime);
        } catch (IOException | ConnectorException e) {
            System.err.println("Warning: nurse " + nurse.getNurseId()
                    + " was not notified: " + e.getMessage());
            // TODO: decidere se questo deve far fallire la registrazione
            // (409/500) o essere solo un warning - per ora, warning.
        }

        ctx.status(201).json(new RegisterPatientResponse(patientId, nurse.getNurseId()));
    }

    private void handleNextPatient(Context ctx) {
        NextPatientRequest req = ctx.bodyAsClass(NextPatientRequest.class);

        if (req.getDeptId() == null) {
            ctx.status(400).json(new ErrorResponse("Missing id_dept"));
            return;
        }

        int deptId;
        try {
            deptId = Integer.parseInt(req.getDeptId());
        } catch (NumberFormatException e) {
            ctx.status(400).json(new ErrorResponse("Invalid id_dept, expected an integer"));
            return;
        }

        WaitingPatient next;
        try {
            next = nextPatientSelector.selectNext(deptId).orElse(null);
        } catch (SQLException e) {
            ctx.status(500).json(new ErrorResponse("Database error: " + e.getMessage()));
            return;
        }

        if (next == null) {
            ctx.status(404).json(new ErrorResponse("No active patients in department " + deptId));
            return;
        }

        String deviceAddress = deviceConfig.getCoapAddress(next.getDeviceId());
        if (deviceAddress == null) {
            ctx.status(500).json(new ErrorResponse("Unknown device address for device " + next.getDeviceId()));
            return;
        }

        try {
            patientCallClient.notifyCalled(deviceAddress);
        } catch (IOException | ConnectorException e) {
            System.err.println("Warning: patient device " + next.getDeviceId() + " was not notified: " + e.getMessage());
        }

        try {
            patientRepository.updateLastVisitTime(next.getPatientId(), LocalDateTime.now());
        } catch (SQLException e) {
            ctx.status(500).json(new ErrorResponse("Database error updating visit time: " + e.getMessage()));
            return;
        }

        ctx.status(200).json(new NextPatientResponse(next.getPatientId(), next.getDeviceId()));
    }
}