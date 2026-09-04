package it.unipi.Nurse;

import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.stream.Collectors;

public class NurseConfig {

    private final List<Nurse> nurses;

    public NurseConfig(String jsonPath) throws IOException {
        ObjectMapper mapper = new ObjectMapper();
        this.nurses = mapper.readValue(new File(jsonPath),
                mapper.getTypeFactory().constructCollectionType(List.class, Nurse.class));
    }

    public List<Nurse> forDepartment(int deptId) {
        return nurses.stream()
                .filter(n -> n.getDeptId() == deptId)
                .collect(Collectors.toList());
    }
}