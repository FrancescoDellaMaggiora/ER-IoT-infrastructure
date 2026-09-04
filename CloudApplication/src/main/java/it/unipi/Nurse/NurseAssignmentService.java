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
}