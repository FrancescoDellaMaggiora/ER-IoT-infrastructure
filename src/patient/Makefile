CONTIKI_PROJECT = patient
all: $(CONTIKI_PROJECT)

# Include CoAP resources
MODULES_REL += ./resources

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
PROJECT_SOURCEFILES += triage_model.c


# Device ID, passed through command line:
# make TARGET=cooja DEVICE_ID=1
DEVICE_ID ?= 1
CFLAGS += -DDEVICE_ID=$(DEVICE_ID)

#	%TODO: This can be removed if needed. It's used to not treat warnings as errors, otherwise Contiki doesn't compile on my PC (ALE)
CFLAGS += -Wno-error=discarded-qualifiers

include $(CONTIKI)/Makefile.include
