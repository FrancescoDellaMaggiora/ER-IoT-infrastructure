package it.unipi.Storage;

import com.influxdb.client.InfluxDBClient;
import com.influxdb.client.InfluxDBClientFactory;
import com.influxdb.client.WriteApiBlocking;
import com.influxdb.client.domain.WritePrecision;
import com.influxdb.client.write.Point;
import it.unipi.MQTT.Vitals;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;
import java.util.concurrent.BlockingQueue;

/**
 * Consumes parsed Vitals from a shared queue and writes them to InfluxDB.
 *
 * Deliberately the ONLY consumer of the queue and the ONLY thread that
 * talks to InfluxDB: producers don't know or care that InfluxDB
 * exists - they just push onto the queue. This keeps the storage
 * backend swappable without touching anything upstream.
 */
public class StorageThread extends Thread {

    private static final String KEY_URL = "influx.url";
    private static final String KEY_TOKEN = "influx.token";
    private static final String KEY_ORG = "influx.org";
    private static final String KEY_BUCKET = "influx.bucket";

    private static final String MEASUREMENT_VITALS = "vitals";

    private final BlockingQueue<Vitals> inputQueue;

    private final String url;
    private final char[] token;
    private final String org;
    private final String bucket;

    // Set by shutdown(): checked at the top of every loop iteration so
    // the thread can be stopped cleanly instead of killed.
    private volatile boolean running = true;

    private InfluxDBClient client;

    public StorageThread(String configPath, BlockingQueue<Vitals> inputQueue) throws IOException {
        this.inputQueue = inputQueue;

        Properties props = new Properties();
        try (InputStream in = new FileInputStream(configPath)) {
            props.load(in);
        }

        this.url = requireProperty(props, KEY_URL);
        this.token = requireProperty(props, KEY_TOKEN).toCharArray();
        this.org = requireProperty(props, KEY_ORG);
        this.bucket = requireProperty(props, KEY_BUCKET);
    }

    private static String requireProperty(Properties props, String key) throws IOException {
        String value = props.getProperty(key);
        if (value == null || value.isBlank()) {
            throw new IOException("Missing required property: " + key);
        }
        return value;
    }

    @Override
    public void run() {
        client = InfluxDBClientFactory.create(url, token, org, bucket);
        WriteApiBlocking writeApi = client.getWriteApiBlocking();

        while (running) {
            Vitals vitals;
            try {
                // Blocks until something is available, or shutdown()
                // interrupts this thread.
                vitals = inputQueue.take();
            } catch (InterruptedException e) {
                // Expected on shutdown(): re-check 'running' and exit
                // the loop instead of propagating.
                Thread.currentThread().interrupt();
                continue;
            }

            try {
                writeApi.writePoint(toPoint(vitals));
            } catch (RuntimeException e) {
                // A single bad/unreachable write must not kill the whole
                // thread: log and keep consuming, or the queue backs up
                // silently behind a dead consumer.
                System.err.println("Failed to write vitals for patient "
                        + vitals.getPatientId() + ": " + e.getMessage());
            }
        }

        client.close();
    }

    /**
     * Converts a Vitals reading into an InfluxDB Point.
     *
     * patientId is a TAG (indexed, used to filter/group in queries, e.g.
     * "show me patient 3's readings"), the individual vitals are
     * FIELDS (the actual measured values). Fields that are null
     * (sensor not attached on that patient) are simply not added,
     * rather than writing a fake 0.
     */
    private static Point toPoint(Vitals vitals) {
        Point point = Point.measurement(MEASUREMENT_VITALS)
                .addTag("patientId", vitals.getPatientId())
                .addTag("source", vitals.getSource())
                .time(vitals.getTimestamp(), WritePrecision.MS);

        if (vitals.getHeartRate() != null) {
            point.addField("heartRate", vitals.getHeartRate());
        }
        if (vitals.getSpo2() != null) {
            point.addField("spo2", vitals.getSpo2());
        }
        if (vitals.getTemperature() != null) {
            point.addField("temperature", vitals.getTemperature());
        }
        if (vitals.getPressureSystolic() != null) {
            point.addField("pressureSystolic", vitals.getPressureSystolic());
        }
        if (vitals.getPressureDiastolic() != null) {
            point.addField("pressureDiastolic", vitals.getPressureDiastolic());
        }
        if (vitals.getResprationRate() != null) {
            point.addField("resprationRate", vitals.getPressureDiastolic());
        }

        return point;
    }

    /**
     * Requests a clean shutdown: the loop exits after the current
     * take()/write() completes, and the InfluxDB client is closed.
     * Call this instead of Thread.stop() (deprecated, unsafe).
     */
    public void shutdown() {
        running = false;
        this.interrupt();   // unblocks a pending take()
    }
}