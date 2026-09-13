DROP DATABASE IF EXISTS ER_IOT;
CREATE DATABASE ER_IOT;
use ER_IOT;

CREATE TABLE IF NOT EXISTS patients (
    patient_id      INT AUTO_INCREMENT PRIMARY KEY,
    ssn             VARCHAR(16)  NOT NULL UNIQUE,
    name            VARCHAR(50)  NOT NULL,
    surname         VARCHAR(50)  NOT NULL,
    arrival_time    DATETIME     NOT NULL,
    triage_code     ENUM('red','orange','blue','green','white') NOT NULL,
    last_visit_time DATETIME     NULL,
    created_at      TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS device_assignments (
    assignment_id   INT AUTO_INCREMENT PRIMARY KEY,
    device_id       VARCHAR(32)  NOT NULL,
    patient_id      INT          NOT NULL,
    assigned_at     DATETIME     NOT NULL,
    released_at     DATETIME     NULL,   -- NULL = still active
    FOREIGN KEY (patient_id) REFERENCES patients(patient_id)
);

CREATE TABLE IF NOT EXISTS  triage_history (
    history_id      INT AUTO_INCREMENT PRIMARY KEY,
    patient_id      INT          NOT NULL,
    triage_code     ENUM('red','orange','blue','green','white') NOT NULL,
    changed_at      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (patient_id) REFERENCES patients(patient_id)
);


DELIMITER $$

-- Check if the device is connected to 2 patients at the same time
CREATE TRIGGER trg_device_assignment_unique
BEFORE INSERT ON device_assignments
FOR EACH ROW
BEGIN
    IF NEW.released_at IS NULL AND EXISTS (
        SELECT 1 FROM device_assignments
        WHERE device_id = NEW.device_id
          AND released_at IS NULL
    ) THEN
        SIGNAL SQLSTATE '45000'
        SET MESSAGE_TEXT = 'Device already has an active assignment';
    END IF;
END$$

-- Triggers for the table triage_history. This one memorize the first status
CREATE TRIGGER trg_triage_history_insert
AFTER INSERT ON patients
FOR EACH ROW
BEGIN
    INSERT INTO triage_history (patient_id, triage_code, changed_at)
    VALUES (NEW.patient_id, NEW.triage_code, NOW());
END$$

-- This memorizes all the times the triage code changes.
CREATE TRIGGER trg_triage_history_update
AFTER UPDATE ON patients
FOR EACH ROW
BEGIN
    IF NEW.triage_code <> OLD.triage_code THEN
        INSERT INTO triage_history (patient_id, triage_code, changed_at)
        VALUES (NEW.patient_id, NEW.triage_code, NOW());
    END IF;
END$$

DELIMITER ;