package it.unipi.doctorapp;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

/**
 * Application configuration, read from an external properties file.
 *
 * Kept outside the jar (config/doctorapp.properties)
 * so the Cloud address, department and Grafana URL can be changed
 * without recompiling.
 */
public class AppConfig {

    private static final String KEY_BASE_URL = "cloud.baseUrl";
    private static final String KEY_DEPT_ID = "doctor.deptId";
    private static final String KEY_GRAFANA_URL = "grafana.dashboardUrl";

    private final String baseUrl;
    private final String deptId;
    private final String grafanaDashboardUrl;

    public AppConfig(String configPath) throws IOException {
        Properties props = new Properties();
        try (InputStream in = new FileInputStream(configPath)) {
            props.load(in);
        }

        this.baseUrl = requireProperty(props, KEY_BASE_URL);
        this.deptId = requireProperty(props, KEY_DEPT_ID);
        this.grafanaDashboardUrl = requireProperty(props, KEY_GRAFANA_URL);
    }

    /** Fails fast with a clear message instead of a silent null later on. */
    private static String requireProperty(Properties props, String key) throws IOException {
        String value = props.getProperty(key);
        if (value == null || value.isBlank()) {
            throw new IOException("Missing required property: " + key);
        }
        return value;
    }

    public String getBaseUrl() { return baseUrl; }
    public String getDeptId() { return deptId; }
    public String getGrafanaDashboardUrl() { return grafanaDashboardUrl; }
}