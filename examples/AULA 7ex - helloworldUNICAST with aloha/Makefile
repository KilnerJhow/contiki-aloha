CONTIKI = ../..

CONTIKI_PROJECT = hello_unicast
all: $(CONTIKI_PROJECT)

CONTIKI_WITH_RIME = 1
DEFINES=NETSTACK_MAC=aloha_driver,NETSTACK_RDC=contikimac_aloha_driver
include $(CONTIKI)/Makefile.include
