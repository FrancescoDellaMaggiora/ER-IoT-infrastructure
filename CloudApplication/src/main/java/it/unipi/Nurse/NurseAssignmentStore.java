package it.unipi.Nurse;

import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArraySet;

/**
 * In-memory storage.
 *
 * TRADE-OFF accepted knowingly: this state does NOT survive a restart
 * of the Cloud Application. If the process is killed/restarted, all
 * load-balancing counts reset to zero and any "which nurse has patient
 * X" lookup for previously-registered patients is lost. Fine for
 * our exam, but it would need to move back to a persistent
 * store (MySQL, as before) if the Cloud needs to survive restarts
 * mid-operation.
 */
public class NurseAssignmentStore {

    // nurseId -> set of currently active (not yet discharged) patient ids
    private final Map<String, Set<Integer>> activeAssignments = new ConcurrentHashMap<>();

    /**
     * Picks the least-loaded nurse among 'candidates' and records the
     * assignment, atomically
     *
     * without the 'synchronized' here, two concurrent registrations
     * could both read "nurse X has the fewest  patients" before either
     * has recorded theirs, and both pick the same nurse - defeating
     * the whole point of load balancing. The lock is on the whole store
     * (not just per-nurse) because the decision itself depends on
     * comparing ALL candidates together.
     *
     * Contention is a non-issue here: patient registration is a very
     * low-frequency event compared to the vitals stream.
     */
    public synchronized Nurse pickAndAssign(List<Nurse> candidates, int patientId) {
        Nurse chosen = candidates.stream()
                .min(Comparator
                        .comparingInt((Nurse n) -> activeCountFor(n.getNurseId()))
                        .thenComparing(Nurse::getNurseId))
                .orElseThrow(() -> new IllegalStateException("No candidate nurses"));

        activeAssignments
                .computeIfAbsent(chosen.getNurseId(), k -> new CopyOnWriteArraySet<>())
                .add(patientId);

        return chosen;
    }

    private int activeCountFor(String nurseId) {
        Set<Integer> assigned = activeAssignments.get(nurseId);
        return assigned == null ? 0 : assigned.size();
    }

    /**
     * Releases a patient from their nurse (e.g. on discharge).
     */
    public void release(String nurseId, int patientId) {
        Set<Integer> assigned = activeAssignments.get(nurseId);
        if (assigned != null) {
            assigned.remove(patientId);
        }
    }
}