package it.unipi.Nurse;

import java.util.List;

public class NurseAssignmentService {

    private final NurseConfig nurseConfig;
    private final NurseAssignmentStore store;

    public NurseAssignmentService(NurseConfig nurseConfig, NurseAssignmentStore store) {
        this.nurseConfig = nurseConfig;
        this.store = store;
    }

    /**
     * @throws IllegalStateException if the department has no configured nurses
     */
    public Nurse assignNurseForPatient(int deptId, int patientId) {
        List<Nurse> candidates = nurseConfig.forDepartment(deptId);
        if (candidates.isEmpty()) {
            throw new IllegalStateException("No nurses configured for department " + deptId);
        }
        return store.pickAndAssign(candidates, patientId);
    }

    /**
     * @return the nurse currently assigned to this patient, or null if
     *         none is on record (e.g. patient never registered, or the
     *         Cloud restarted since - see NurseAssignmentStore's
     *         in-memory trade-off)
     */
    public Nurse getNurseForPatient(int patientId) {
        String nurseId = store.getNurseIdForPatient(patientId);
        if (nurseId == null) {
            return null;
        }
        return nurseConfig.getById(nurseId);
    }

    /**
     * Releases the patient from whichever nurse currently has them
     * (used on discharge). No-op if the patient has no nurse on record.
     */
    public void releasePatient(int patientId) {
        String nurseId = store.getNurseIdForPatient(patientId);
        if (nurseId != null) {
            store.release(nurseId, patientId);
        }
    }
}