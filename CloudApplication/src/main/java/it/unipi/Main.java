package it.unipi;

import it.unipi.MQTT.MQTTThread;
import it.unipi.MQTT.Vitals;
import it.unipi.Storage.StorageThread;
import org.eclipse.paho.client.mqttv3.MqttException;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.stream.StreamSupport;

//TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or
// click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
public class Main {

    private static final String MQTT_CONFIG_PATH = "config/mqtt.properties";
    private static final String STORAGE_CONFIG_PATH = "config/storage.properties";

    private static final int QUEUE_CAPACITY = 1000;

    public static void main(String[] args) {

        BlockingQueue<Vitals> vitalsQueue = new LinkedBlockingQueue<>(QUEUE_CAPACITY);

        final MQTTThread mqttThread;
        final StorageThread storageThread;

        System.out.println("Starting MQTTThread");
        try {
            mqttThread = new MQTTThread(MQTT_CONFIG_PATH, vitalsQueue);
            mqttThread.start();
        } catch (IOException e) {
            System.out.println("MQTTThread: Error reading config file");
            e.printStackTrace();
            System.exit(1);
            return;
        } catch (RuntimeException mqttException) {
            System.out.println("MQTT Exception: " + mqttException.getMessage());
            mqttException.printStackTrace();
            System.exit(1);
            return;
        }

        System.out.println("Starting StorageThread");
        try {
            storageThread = new StorageThread(STORAGE_CONFIG_PATH, vitalsQueue);
            storageThread.start();
        } catch (IOException e) {
            mqttThread.shutdown();
            System.out.println("StorageThread: Error reading config file");
            e.printStackTrace();
            System.exit(1);
            return;
        }

        //Adding the call to shut down the system
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("Shutting down...");
            if (storageThread != null) {
                storageThread.shutdown();
                try {
                    storageThread.join();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
            if (mqttThread != null)
                mqttThread.shutdown();
        }));
    }
}