CONTIKI_PROJECT = mqtt_client
all: $(CONTIKI_PROJECT)

CONTIKI = ../../../contiki-ng

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt

include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

PLATFORMS_ONLY = cc26x0-cc13x0 cc2538dk openmote zoul native simplelink cooja

# Patient ID, passed through command line:
# make TARGET=cooja PATIENT_ID=1
PATIENT_ID ?= 0
CFLAGS += -DPATIENT_ID=$(PATIENT_ID)

include $(CONTIKI)/Makefile.include