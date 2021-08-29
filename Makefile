CC=gcc
CFLAGS=-lzmq
all:
	${CC} ${CFLAGS} src/sps30_pi.c
