package it.unipi.doctorapp.client;

import com.fasterxml.jackson.databind.ObjectMapper;
import it.unipi.doctorapp.model.*;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;

/**
 * Thin client over the Cloud Application's doctor-facing REST API.
 *
 * Uses java.net.http.HttpClient.
 *
 * Every method either returns the parsed success body, or throws
 * CloudApiException carrying the Cloud's own error message.
 */
public class CloudApiClient {

    private static final ObjectMapper MAPPER = new ObjectMapper();

    private final HttpClient http;
    private final String baseUrl;

    public CloudApiClient(String baseUrl) {
        // Trailing slash would produce double slashes in the paths below.
        this.baseUrl = baseUrl.endsWith("/")
                ? baseUrl.substring(0, baseUrl.length() - 1)
                : baseUrl;

        this.http = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(5))
                .build();
    }

    public RegisterPatientResponse registerPatient(RegisterPatientRequest request)
            throws IOException, InterruptedException, CloudApiException {
        return post("/er/doctor/register", request, RegisterPatientResponse.class);
    }

    public NextPatientResponse callNextPatient(String deptId)
            throws IOException, InterruptedException, CloudApiException {
        return post("/er/doctor/next-patient", new NextPatientRequest(deptId),
                NextPatientResponse.class);
    }

    public UpdateTriageResponse updateTriage(String patientId, String triageCode)
            throws IOException, InterruptedException, CloudApiException {
        return post("/er/doctor/update-triage", new UpdateTriageRequest(patientId, triageCode),
                UpdateTriageResponse.class);
    }

    public DischargeResponse dischargePatient(String patientId)
            throws IOException, InterruptedException, CloudApiException {
        return post("/er/doctor/discharge", new DischargeRequest(patientId),
                DischargeResponse.class);
    }

    /**
     * Sends 'body' as JSON to 'path' and deserializes the response into
     * responseType.
     *
     * @throws CloudApiException if the Cloud answered 4xx/5xx
     */
    private <T> T post(String path, Object body, Class<T> responseType)
            throws IOException, InterruptedException, CloudApiException {

        String json = MAPPER.writeValueAsString(body);

        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + path))
                .header("Content-Type", "application/json")
                .timeout(Duration.ofSeconds(60))   // the Cloud may wait on CoAP round-trips
                .POST(HttpRequest.BodyPublishers.ofString(json))
                .build();

        HttpResponse<String> response = http.send(request, HttpResponse.BodyHandlers.ofString());

        if (response.statusCode() >= 400) {
            throw new CloudApiException(response.statusCode(), extractErrorMessage(response.body()));
        }

        return MAPPER.readValue(response.body(), responseType);
    }

    /**
     * Pulls the "error" field out of the Cloud's ErrorResponse body.
     * Falls back to the raw body if it isn't the expected shape (e.g.
     * a plain-text error page from something other than our Cloud).
     */
    private String extractErrorMessage(String body) {
        try {
            ErrorResponse error = MAPPER.readValue(body, ErrorResponse.class);
            return error.error != null ? error.error : body;
        } catch (IOException e) {
            return body;
        }
    }
}