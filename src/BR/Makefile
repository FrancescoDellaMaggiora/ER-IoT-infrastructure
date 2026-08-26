CONTIKI_PROJECT = border-router
all: $(CONTIKI_PROJECT)

CONTIKI = ../../../../..

# Department identity of this BR, one firmware for all departments:
#   make TARGET=cooja DEPT_ID=2
#   0 pediatrico, 1 ostetrico-ginecologico, 2 agitazione psico-motoria,
#   3 disabilita' complessa, 4 vittime di violenza, 5 malato infettivo
DEPT_ID ?= 0
CFLAGS += -DMY_DEPT_ID=$(DEPT_ID)


MODULES += os/services/rpl-border-router
MODULES += os/net/app-layer/coap
MODULES_REL += ./resources

include $(CONTIKI)/Makefile.include
