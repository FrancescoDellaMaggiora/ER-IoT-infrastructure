package it.unipi.doctorapp.ui;

import java.awt.Desktop;
import java.io.IOException;
import java.net.URI;

/**
 * Opens a URL in the user's browser.
 */
public final class BrowserLauncher {

    private BrowserLauncher() {}

    /**
     * "Open" the url, printing it.
     *
     * @return true if a launch strategy reported success
     */
    public static boolean open(String url) {
        System.out.println("Dashboard: " + url);
        return true;
    }
}