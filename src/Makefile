# Simple Makefile
#
#  Author: Jesper Dangaard Brouer <brouer@redhat.com>
#
SRCS = ipv6_example01.c udp_example02.c

TARGETS = ${SRCS:.c=}

all: $(TARGETS)

help:
	@echo Possible targets: $(TARGETS)

.c: $<
	gcc -o $@ $<
