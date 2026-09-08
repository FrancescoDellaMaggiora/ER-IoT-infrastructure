package it.unipi.Patient;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;

public class DeviceEntry {
    private final String deviceId;
    private final String coapAddress;

    @JsonCreator
    public DeviceEntry(@JsonProperty("deviceId") String deviceId,
                       @JsonProperty("coapAddress") String coapAddress) {
        this.deviceId = deviceId;
        this.coapAddress = coapAddress;
    }

    public String getDeviceId() { return deviceId; }
    public String getCoapAddress() { return coapAddress; }
}