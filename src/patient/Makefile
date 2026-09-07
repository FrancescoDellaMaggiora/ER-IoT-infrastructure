CONTIKI_PROJECT = patient
all: $(CONTIKI_PROJECT)

# Include CoAP resources
MODULES_REL += ./resources

CONTIKI = ../../../contiki-ng

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

# The MQTT transport is a separate module 
# patient.c holds only the application logic.
PROJECT_SOURCEFILES += mqtt-service.c

# Patient ID, passed through command line:
# make TARGET=cooja PATIENT_ID=1
PATIENT_ID ?= 1
CFLAGS += -DPATIENT_ID=$(PATIENT_ID)

#	%TODO: This can be removed if needed. It's used to not treat warnings as errors, otherwise Contiki doesn't compile on my PC (ALE)
CFLAGS += -Wno-error=discarded-qualifiers

include $(CONTIKI)/Makefile.include
