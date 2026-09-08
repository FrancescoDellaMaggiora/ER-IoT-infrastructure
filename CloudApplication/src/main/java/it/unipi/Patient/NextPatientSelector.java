package it.unipi.Patient;

import it.unipi.Repository.PatientRepository;

import java.sql.SQLException;
import java.time.Duration;
import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;

/**
 * Selects the next patient to be seen in a ward: the one whose
 * time elapsed since their last check-up most exceeds the
 * revisit target of their triage code.
 */
public class NextPatientSelector {

    private final PatientRepository patientRepository;

    public NextPatientSelector(PatientRepository patientRepository) {
        this.patientRepository = patientRepository;
    }

    /**
     * Method to select, if exists, the next patient to visit
     * @param deptId Department to check
     * @return An Optional tha can contain the data (in WaitingPatient) if exists
     * @throws SQLException throws whenever there's a problem
     */
    public Optional<WaitingPatient> selectNext(int deptId) throws SQLException {
        List<WaitingPatient> candidates = patientRepository.findActivePatientsInDept(deptId);
        LocalDateTime now = LocalDateTime.now();

        return candidates.stream()
                .max((a, b) -> Long.compare(overdueMinutes(a, now), overdueMinutes(b, now)));
    }

    private long overdueMinutes(WaitingPatient p, LocalDateTime now) {
        long elapsed = Duration.between(p.getLastCheckTime(), now).toMinutes();
        int target = TriageCodeMapper.targetRevisitMinutes(p.getTriageCode());
        return elapsed - target;
    }
}