CONTIKI_PROJECT = patient
all: $(CONTIKI_PROJECT)

CONTIKI = ../../../../..

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt

include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

# The MQTT transport is a separate module 
# patient.c holds only the application logic.
PROJECT_SOURCEFILES += mqtt-service.c

# Patient ID, passed through command line:
# make TARGET=cooja PATIENT_ID=1
PATIENT_ID ?= 1
CFLAGS += -DPATIENT_ID=$(PATIENT_ID)

include $(CONTIKI)/Makefile.include
