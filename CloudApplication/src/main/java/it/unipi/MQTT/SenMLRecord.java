package it.unipi.MQTT;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * One SenML record (RFC 8428), e.g.:
 *   {"n":"heart-rate","u":"beat/min","v":70}
 *
 * Wire field names are single letters per the RFC; mapped here to
 * readable Java names so the rest of the codebase never touches n/u/v.
 */
public class SenMLRecord {

    @JsonProperty("n")
    private String name;

    @JsonProperty("u")
    private String unit;

    @JsonProperty("v")
    private double value;

    // Jackson requires a no-arg constructor to instantiate the object
    // before populating the fields.
    public SenMLRecord() {}

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public String getUnit() { return unit; }
    public void setUnit(String unit) { this.unit = unit; }

    public double getValue() { return value; }
    public void setValue(double value) { this.value = value; }

    @Override
    public String toString() {
        return "SenMLRecord{name='" + name + "', unit='" + unit + "', value=" + value + "}";
    }
}