package it.unipi.Nurse;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;

public class Nurse {
    private final String nurseId;
    private final int deptId;
    private final String coapAddress;

    @JsonCreator
    public Nurse(
            @JsonProperty("nurseId") String nurseId,
            @JsonProperty("deptId") int deptId,
            @JsonProperty("coapAddress") String coapAddress) {

        this.nurseId = nurseId;
        this.deptId = deptId;
        this.coapAddress = coapAddress;
    }

    public String getNurseId() { return nurseId; }
    public int getDeptId() { return deptId; }
    public String getCoapAddress() { return coapAddress; }

    /**
     * Extracts the bare IPv6 address from a coapAddress like
     * "coap://[fd00:1::abcd:1]" -> "fd00:1::abcd:1".
     * Used when sending the address in a JSON payload (NURSE_ADDRESS),
     * where the device firmware expects the address alone, not a full
     * CoAP URI.
     */
    public String getIpAddress() {
        int start = coapAddress.indexOf('[');
        int end = coapAddress.indexOf(']');
        if (start < 0 || end < 0) {
            return coapAddress;   // unexpected format, return as-is
        }
        return coapAddress.substring(start + 1, end);
    }
}