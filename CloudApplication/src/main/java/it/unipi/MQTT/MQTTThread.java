package it.unipi.MQTT;

import com.fasterxml.jackson.core.JsonProcessingException;
import org.eclipse.paho.client.mqttv3.*;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;
import java.util.Properties;
import java.util.concurrent.BlockingQueue;

public class MQTTThread extends Thread implements MqttCallback {

    private static final String KEY_BROKER = "mqtt.broker";
    private static final String KEY_CLIENT_ID = "mqtt.clientId";
    private static final String KEY_TOPIC_VITALS = "mqtt.topic.vitals";
    private static final String KEY_TOPIC_ALERT = "mqtt.topic.alert";

    private String broker;
    private String clientId;
    private String topicVitals;
    private String topicAlert;
    private MqttClient mqttClient;

    private final BlockingQueue<Vitals> outputQueue;

    /**
     * Fails fast with a clear message instead of a silent null later on.
     *
     * @param props Properties Object containing the values
     * @param key Key of the searched value
     *
     * @return String The searched value
     *
     * @throws IOException if the properties doesn't contain the key
     */
    private static String requireProperty(Properties props, String key) throws IOException {
        String value = props.getProperty(key);
        if (value == null || value.isBlank()) {
            throw new IOException("Missing required property: " + key);
        }
        return value;
    }


    /**
     * Loads configuration from the given properties file.
     *
     * @param configPath path to a .properties file, e.g.
     *                   "config/cloud-mqtt.properties"
     * @throws IOException if the file is missing or a required key
     *                      is absent
     */
    public MQTTThread(String configPath, BlockingQueue<Vitals> outputQueue) throws IOException {

        this.outputQueue = outputQueue;
        Properties props = new Properties();

        try (InputStream in = new FileInputStream(configPath)) {
            props.load(in);
        }

        this.broker = requireProperty(props, KEY_BROKER);
        this.clientId = requireProperty(props, KEY_CLIENT_ID);
        this.topicVitals = requireProperty(props, KEY_TOPIC_VITALS);
        this.topicAlert = requireProperty(props, KEY_TOPIC_ALERT);
        this.mqttClient = null;
    }

    /**
     *
     */
    public  void run()
    {
        try {
            mqttClient = new MqttClient(broker,clientId);
        } catch (MqttException e) {
            throw new RuntimeException(e);
        }

        mqttClient.setCallback( this );

        /* 
         * Paho does NOT reconnect on its own. Now
         * subscriptions are restored
         * automatically along with the connection.
         */
        MqttConnectOptions options = new MqttConnectOptions();
        options.setAutomaticReconnect(true);
        options.setCleanSession(false);
        try {
            mqttClient.connect(options);
            mqttClient.subscribe(this.topicVitals);
            mqttClient.subscribe(this.topicAlert);
        } catch (MqttException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * @param topic e.g. "er/patient/3/vitals"
     * @return the patient id segment, e.g. "3"
     * @throws IllegalArgumentException if the topic doesn't have the
     *                                   expected shape (er/patient/<id>/...)
     */
    static private String extractPatientId(String topic) {
        String[] parts = topic.split("/");
        if (parts.length < 4 || !parts[0].equals("er") || !parts[1].equals("patient")) {
            throw new IllegalArgumentException("Unexpected topic shape: " + topic);
        }
        return parts[2];
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        try {
            String patientId = extractPatientId(topic);
            List<SenMLRecord> records = null;
            try {
                records = SenMLParser.parse(message.getPayload());
            } catch (JsonProcessingException e) {
                System.err.println("Malformed SenML payload on topic " + topic + ": " + e.getMessage());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            Vitals vitals = VitalsMapper.fromSenML(patientId, records);

            // Non-blocking: if the queue is full, drop and log instead of
            // stalling this thread (which is Paho's own delivery thread -
            // blocking it here would delay/queue up every other incoming
            // message too, exactly the bottleneck discussed for the
            // congestion stress test).
            boolean accepted = outputQueue.offer(vitals);
            if (!accepted) {
                System.err.println("Output queue full, dropping reading for patient " + patientId);
            }
        } catch (IllegalArgumentException e) {
            System.err.println("Unexpected topic: " + e.getMessage());
        }
    }

    @Override   
    public void connectionLost(Throwable throwable) {
        System.err.println("[MQTT] Connection lost to " + broker
                + " (clientId=" + clientId + "): "
                + (throwable != null ? throwable.getMessage() : "unknown cause")
                + " - automatic reconnect enabled, waiting for reconnection...");
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken iMqttDeliveryToken) {
        System.err.println("[MQTT] Delivery complete from clientd " + clientId);
    }

    /**
     * Requests a clean disconnect from the broker.
     */
    public void shutdown() {
        if (mqttClient == null) {
            return;
        }
        try {
            if (mqttClient.isConnected()) {
                mqttClient.disconnect();
            }
            mqttClient.close();
        } catch (MqttException e) {
            System.err.println("Error while disconnecting MQTT client: " + e.getMessage());
        }
    }
}
