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
}