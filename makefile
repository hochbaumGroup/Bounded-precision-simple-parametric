CFLAGS=-O4 -DBREAKPOINTS
BINDIR=bin
CC=gcc

all: ${BINDIR}/pseudo_par

pseudo_par: ${BINDIR}/pseudo_par

${BINDIR}/pseudo_par: ${BINDIR}
	${CC} ${CFLAGS} src/1.0/pseudopar.c -lm -o ${BINDIR}/pseudo_par
clean:
	rm -f ${BINDIR}/*
${BINDIR}:
	mkdir ${BINDIR}
