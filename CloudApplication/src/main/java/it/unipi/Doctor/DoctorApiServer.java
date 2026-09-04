package it.unipi.Doctor;

import io.javalin.Javalin;
import io.javalin.http.Context;
import it.unipi.CoAP.NurseTriageUpdateClient;
import it.unipi.CoAP.PatientAssociationClient;
import it.unipi.CoAP.PatientCallClient;
import it.unipi.CoAP.PatientTriageUpdateClient;
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
import it.unipi.Doctor.UpdateTriageResponse;

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
    private final NurseTriageUpdateClient nurseTriageUpdateClient;
    private final PatientAssociationClient patientAssociationClient;
    private final PatientTriageUpdateClient patientTriageUpdateClient;


    public DoctorApiServer(int port,
                           PatientRepository patientRepository,
                           NurseAssignmentService nurseAssignmentService,
                           PatientAssociationClient patientAssociationClient,
                           NextPatientSelector nextPatientSelector,
                           DeviceConfig deviceConfig,
                           PatientCallClient patientCallClient,
                           PatientTriageUpdateClient patientTriageUpdateClient,
                           NurseTriageUpdateClient nurseTriageUpdateClient) {
        this.port = port;
        this.patientRepository = patientRepository;
        this.nurseAssignmentService = nurseAssignmentService;
        this.patientAssociationClient = patientAssociationClient;
        this.nextPatientSelector = nextPatientSelector;
        this.deviceConfig = deviceConfig;
        this.patientCallClient = patientCallClient;
        this.patientTriageUpdateClient = patientTriageUpdateClient;
        this.nurseTriageUpdateClient = nurseTriageUpdateClient;

        this.app = Javalin.create(config -> {
            config.routes.post("/er/doctor/register", this::handleRegister);
            config.routes.post("/er/doctor/next-patient", this::handleNextPatient);
            config.routes.post("/er/doctor/update-triage", this::handleUpdateTriage);
        });
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

    private void handleUpdateTriage(Context ctx) {
        UpdateTriageRequest req = ctx.bodyAsClass(UpdateTriageRequest.class);

        if (req.getPatientId() == null || req.getTriageCode() == null) {
            ctx.status(400).json(new ErrorResponse("Missing id_paziente or triage_code"));
            return;
        }

        int patientId;
        int triageNumber;
        try {
            patientId = Integer.parseInt(req.getPatientId());
            triageNumber = TriageCodeMapper.toNumber(req.getTriageCode());
        } catch (NumberFormatException e) {
            ctx.status(400).json(new ErrorResponse("Invalid id_paziente, expected an integer"));
            return;
        } catch (IllegalArgumentException e) {
            ctx.status(400).json(new ErrorResponse(e.getMessage()));
            return;
        }

        // 1. Update MySQL. The trg_triage_history_update trigger appends
        //    to triage_history automatically - no application code for
        //    that.
        boolean updated;
        try {
            updated = patientRepository.updateTriageCode(patientId, req.getTriageCode());
        } catch (SQLException e) {
            ctx.status(500).json(new ErrorResponse("Database error: " + e.getMessage()));
            return;
        }

        if (!updated) {
            ctx.status(404).json(new ErrorResponse("Unknown patient_id " + patientId));
            return;
        }

        // 2. Notify the PATIENT device. Warning-only on failure: the DB is already the source of truth.
        String deviceId;
        try {
            deviceId = patientRepository.findActiveDeviceForPatient(patientId);
        } catch (SQLException e) {
            deviceId = null;
            System.err.println("Warning: could not look up device for patient "
                    + patientId + ": " + e.getMessage());
        }

        if (deviceId != null) {
            String deviceAddress = deviceConfig.getCoapAddress(deviceId);
            if (deviceAddress != null) {
                try {
                    patientTriageUpdateClient.notifyTriageChanged(deviceAddress, triageNumber);
                } catch (IOException | ConnectorException e) {
                    System.err.println("Warning: patient device " + deviceId
                            + " was not notified of triage change: " + e.getMessage());
                }
            } else {
                System.err.println("Warning: unknown device address for device " + deviceId);
            }
        } else {
            System.err.println("Warning: patient " + patientId + " has no active device assignment");
        }

        // 3. Notify the NURSE device, dedicated resource
        Nurse nurse = nurseAssignmentService.getNurseForPatient(patientId);
        if (nurse != null) {
            try {
                nurseTriageUpdateClient.notifyTriageChanged(nurse, patientId, triageNumber);
            } catch (IOException | ConnectorException e) {
                System.err.println("Warning: nurse " + nurse.getNurseId()
                        + " was not notified of triage change: " + e.getMessage());
            }
        } else {
            System.err.println("Warning: no nurse on record for patient " + patientId);
        }

        ctx.status(200).json(new UpdateTriageResponse(patientId, req.getTriageCode()));
    }

}