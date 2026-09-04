package it.unipi.Nurse;

import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Static, in-memory registry of nurses, loaded once from a JSON file at
 * startup.
 */
public class NurseConfig {

    private final List<Nurse> nurses;

    public NurseConfig(String jsonPath) throws IOException {
        ObjectMapper mapper = new ObjectMapper();
        this.nurses = mapper.readValue(new File(jsonPath),
                mapper.getTypeFactory().constructCollectionType(List.class, Nurse.class));
    }

    /** All nurses belonging to a given department. */
    public List<Nurse> forDepartment(int deptId) {
        return nurses.stream()
                .filter(n -> n.getDeptId() == deptId)
                .collect(Collectors.toList());
    }

    /** @return the nurse with this id, or null if unknown. */
    public Nurse getById(String nurseId) {
        return nurses.stream()
                .filter(n -> n.getNurseId().equals(nurseId))
                .findFirst()
                .orElse(null);
    }
}