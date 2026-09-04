package it.unipi.Doctor;

import io.javalin.Javalin;
import io.javalin.http.Context;
import it.unipi.CoAP.PatientAssociationClient;
import it.unipi.Nurse.Nurse;
import it.unipi.Nurse.NurseAssignmentService;
import it.unipi.Repository.PatientRepository;
import org.eclipse.californium.elements.exception.ConnectorException;

import java.io.IOException;
import java.sql.Connection;
import java.sql.SQLException;
import java.time.LocalDateTime;
import java.time.format.DateTimeParseException;

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

    private final PatientRepository patientRepository;
    private final NurseAssignmentService nurseAssignmentService;
    private final PatientAssociationClient patientAssociationClient;

    public DoctorApiServer(int port, PatientRepository patientRepository,
                           NurseAssignmentService nurseAssignmentService,
                           PatientAssociationClient patientAssociationClient) {
        this.port = port;
        this.patientRepository = patientRepository;
        this.nurseAssignmentService = nurseAssignmentService;
        this.patientAssociationClient = patientAssociationClient;

        this.app = Javalin.create(config -> {
            config.routes.post("/er/doctor/register", this::handleRegister);
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

        // Notifica al device dell'infermiere PRIMA di rispondere al medico,
        // come richiesto - se questa fallisce, il paziente resta comunque
        // registrato: si sceglie di non fare rollback (l'infermiere può
        // essere ri-notificata manualmente), ma si segnala l'errore.
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
}