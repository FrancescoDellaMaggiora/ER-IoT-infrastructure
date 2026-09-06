package it.unipi.doctorapp.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/** Body returned by the Cloud on any 4xx/5xx response. */
@JsonIgnoreProperties(ignoreUnknown = true)
public class ErrorResponse {
    public String error;
}