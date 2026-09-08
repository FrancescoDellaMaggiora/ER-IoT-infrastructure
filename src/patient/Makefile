CONTIKI_PROJECT = patient
all: $(CONTIKI_PROJECT)

CONTIKI = ../../../../..

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt
MODULES += os/net/app-layer/coap

MODULES_REL += /home/studenti/contiki-ng/.venv/lib/python3.12/site-packages/emlearn
INC         += /home/studenti/contiki-ng/.venv/lib/python3.12/site-packages/emlearn
TARGET_LIBFILES += -lm

include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

# The MQTT transport is a separate module 
# patient.c holds only the application logic.
PROJECT_SOURCEFILES += mqtt-service.c
PROJECT_SOURCEFILES += vitals-buffer.c
PROJECT_SOURCEFILES += triage-report.c
PROJECT_SOURCEFILES += triage-model.c


# Patient ID, passed through command line:
# make TARGET=cooja PATIENT_ID=1
PATIENT_ID ?= 0
CFLAGS += -DPATIENT_ID=$(PATIENT_ID)

include $(CONTIKI)/Makefile.include
