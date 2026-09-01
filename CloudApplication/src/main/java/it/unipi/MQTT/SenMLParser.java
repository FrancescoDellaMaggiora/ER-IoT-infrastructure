package it.unipi.MQTT;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.IOException;
import java.util.List;

/**
 * Parses a SenML pack (RFC 8428) - a JSON array of records - as
 * produced by the patient nodes' build_payload_vitals()/build_payload_alert().
 *
 * Deliberately generic: it does not know about "heart rate" or "spo2"
 * specifically, it just turns JSON into a List<SenMLRecord>. Mapping
 * those records onto a domain object (Vitals) is a SEPARATE step,
 * done elsewhere - this keeps the parser correct even if the patient
 * firmware adds/removes fields, and lets you test each step on its own.
 */
public class SenMLParser {

    // Reused across all calls: ObjectMapper is thread-safe once
    // configured, and non-trivial to create - important here since
    // messageArrived() may fire rapidly for many patients at once.
    private static final ObjectMapper MAPPER = new ObjectMapper();

    private SenMLParser() {
    }

    /**
     * @param payload raw MQTT payload bytes (a JSON array of SenML records)
     * @return the parsed records, in the same order as the array
     * @throws JsonProcessingException if the payload is not valid JSON
     *                                  or doesn't match the expected shape
     */
    public static List<SenMLRecord> parse(byte[] payload) throws IOException {
        return MAPPER.readValue(payload, new TypeReference<List<SenMLRecord>>() {});
    }
}