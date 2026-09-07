CONTIKI_PROJECT = sensor
all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

CONTIKI= ../../contiki-ng

# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

# Device ID (default is 1)
DEVICE_ID ?= 1
CFLAGS += -DDEVICE_ID=$(DEVICE_ID)

#	%TODO: This can be removed if needed. It's used to not treat warnings as errors, otherwise Contiki doesn't compile on my PC (ALE)
CFLAGS += -Wno-error=discarded-qualifiers

include $(CONTIKI)/Makefile.include