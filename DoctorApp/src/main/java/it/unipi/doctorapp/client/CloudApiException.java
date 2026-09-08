package it.unipi.doctorapp.client;

/**
 * Thrown when the Cloud Application answers with a 4xx/5xx status.
 *
 * Carries both the status code and the message the Cloud put in its
 * ErrorResponse body, so the CLI can show the doctor something
 * meaningful ("No active patients in department 0") rather than a bare
 * status number.
 */
public class CloudApiException extends Exception {

    private final int statusCode;

    public CloudApiException(int statusCode, String message) {
        super(message);
        this.statusCode = statusCode;
    }

    public int getStatusCode() {
        return statusCode;
    }
}