package it.unipi.doctorapp.ui;

import java.util.List;
import java.util.Scanner;

/**
 * Small helpers for reading validated input from the console.
 *
 * Keeps the menu code readable: every prompt re-asks until the input is
 * acceptable, instead of scattering while-loops through the handlers.
 */
public class ConsoleInput {

    private final Scanner scanner;

    public ConsoleInput(Scanner scanner) {
        this.scanner = scanner;
    }

    /** Reads a non-empty line, re-prompting while the input is blank. */
    public String readRequired(String prompt) {
        while (true) {
            System.out.print(prompt);
            String line = scanner.nextLine().trim();
            if (!line.isEmpty()) {
                return line;
            }
            System.out.println("This field is required.");
        }
    }

    /** Reads a line that may be left empty (returns null in that case). */
    public String readOptional(String prompt) {
        System.out.print(prompt);
        String line = scanner.nextLine().trim();
        return line.isEmpty() ? null : line;
    }

    /** Reads an integer, re-prompting while the input isn't a number. */
    public int readInt(String prompt) {
        while (true) {
            String line = readRequired(prompt);
            try {
                return Integer.parseInt(line);
            } catch (NumberFormatException e) {
                System.out.println("Please enter a whole number.");
            }
        }
    }

    /**
     * Reads one of 'allowed', re-prompting otherwise. Used for the
     * triage code so an invalid value is caught locally rather than
     * making a round-trip to the Cloud just to get a 400 back.
     */
    public String readChoice(String prompt, List<String> allowed) {
        while (true) {
            String line = readRequired(prompt).toLowerCase();
            if (allowed.contains(line)) {
                return line;
            }
            System.out.println("Please enter one of: " + String.join(", ", allowed));
        }
    }
}