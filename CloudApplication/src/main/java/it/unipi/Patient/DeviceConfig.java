package it.unipi.Patient;

import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.File;
import java.io.IOException;
import java.util.List;

public class DeviceConfig {

    private final List<DeviceEntry> devices;

    public DeviceConfig(String jsonPath) throws IOException {
        ObjectMapper mapper = new ObjectMapper();
        this.devices = mapper.readValue(new File(jsonPath),
                mapper.getTypeFactory().constructCollectionType(List.class, DeviceEntry.class));
    }

    /** @return l'indirizzo CoAP del device, o null se sconosciuto */
    /**
     *  Get the IPv6 address of the device  deviceId
     * @param deviceId Device's id of witch are we looking for
     * @return The IPv6, if exits, otherwise null
     */
    public String getCoapAddress(String deviceId) {
        return devices.stream()
                .filter(d -> d.getDeviceId().equals(deviceId))
                .findFirst()
                .map(DeviceEntry::getCoapAddress)
                .orElse(null);
    }
}