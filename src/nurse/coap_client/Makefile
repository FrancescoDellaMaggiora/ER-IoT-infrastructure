CONTIKI_PROJECT = client
all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

CONTIKI= ../../../../..

# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

#	%TODO: This can be removed if needed. It's used to not treat warnings as errors, otherwise Contiki doesn't compile on my PC (ALE)
CFLAGS += -Wno-error=discarded-qualifiers

include $(CONTIKI)/Makefile.include
