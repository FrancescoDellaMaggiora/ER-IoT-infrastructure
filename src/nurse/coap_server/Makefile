CONTIKI_PROJECT = nurse
all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

# Include CoAP resources
MODULES_REL += ./resources

CONTIKI= ../../../../../..

# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

# Project source files
PROJECT_SOURCEFILES += common.c
PROJECT_SOURCEFILES += nurse-patients.c
PROJECT_SOURCEFILES += JSON-parser.c

# If set to 1 enables debug (mostly used to print stuff)
NURSE_DBG ?= 0
CFLAGS += -DNURSE_DBG=$(NURSE_DBG)

# Associate this number of patients to the nurse with incremental IDs
# 	make TARGET=<target> ADD_PATIENT=<number of patients to add>
DBG_ADD_PATIENT ?= 0
CFLAGS += -DDBG_ADD_PATIENT=$(DBG_ADD_PATIENT)

CFLAGS += -Wno-error=discarded-qualifiers

include $(CONTIKI)/Makefile.include