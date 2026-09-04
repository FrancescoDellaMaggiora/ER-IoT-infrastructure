package it.unipi.Nurse;

import java.util.Map;

public final class TriageCodeMapper {

    private static final Map<String, Integer> CODE_TO_NUMBER = Map.of(
            "red", 1,
            "orange", 2,
            "blue", 3,
            "green", 4,
            "white", 5
    );

    private TriageCodeMapper() {}

    public static int toNumber(String triageCode) {
        Integer value = CODE_TO_NUMBER.get(triageCode);
        if (value == null) {
            throw new IllegalArgumentException("Unknown triage code: " + triageCode);
        }
        return value;
    }

    public static String toString(int triageCode) {

        return switch (triageCode) {
            case 1 -> "red";
            case 2 -> "orange";
            case 3 -> "blue";
            case 4 -> "green";
            case 5 -> "white";
            default -> throw new IllegalArgumentException("Unknown triage code: " + triageCode);
        };
    }
}