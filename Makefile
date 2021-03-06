CFLAGS = -g -Wall -DDEBUG
LDFLAGS = -g -Wall -DDEBUG -lm

all: serveur_FAST_RETRANSMIT_REAL_TIME serveur_FAST_RETRANSMIT serveur_NO_SLOW_START serveur

serveur_FAST_RETRANSMIT_REAL_TIME.o: serveur_FAST_RETRANSMIT_REAL_TIME.c
	gcc ${CFLAGS} -c serveur_FAST_RETRANSMIT_REAL_TIME.c -o serveur_FAST_RETRANSMIT_REAL_TIME.o

serveur_FAST_RETRANSMIT_REAL_TIME: serveur_FAST_RETRANSMIT_REAL_TIME.o
	gcc ${LDFLAGS} serveur_FAST_RETRANSMIT_REAL_TIME.o -o serveur_FAST_RETRANSMIT_REAL_TIME

serveur_FAST_RETRANSMIT.o: serveur_FAST_RETRANSMIT.c
	gcc ${CFLAGS} -c serveur_FAST_RETRANSMIT.c -o serveur_FAST_RETRANSMIT.o

serveur_FAST_RETRANSMIT: serveur_FAST_RETRANSMIT.o
	gcc ${LDFLAGS} serveur_FAST_RETRANSMIT.o -o serveur_FAST_RETRANSMIT

serveur_SLOW_START.o: serveur_SLOW_START.c
		gcc ${CFLAGS} -c serveur_SLOW_START.c -o serveur_SLOW_START.o

serveur_SLOW_START: serveur_SLOW_START.o
		gcc ${LDFLAGS} serveur_SLOW_START.o -o serveur_SLOW_START

serveur_NO_SLOW_START.o: serveur_NO_SLOW_START.c
		gcc ${CFLAGS} -c serveur_NO_SLOW_START.c -o serveur_NO_SLOW_START.o

serveur_NO_SLOW_START: serveur_NO_SLOW_START.o
		gcc ${LDFLAGS} serveur_NO_SLOW_START.o -o serveur_NO_SLOW_START

serveur.o:	serveur.c
	gcc ${CFLAGS} -c serveur.c -o serveur.o

serveur: serveur.o
	gcc ${LDFLAGS} serveur.o -o serveur

clean:
	\rm -rf *.o main  *~ client serveur serveur_FAST_RETRANSMIT serveur_SLOW_START

clocktest: clocktest.c
	gcc ${CFLAGS} -c clocktest.c -o clocktest.o
	gcc ${LDFLAGS} clocktest.o -o clocktest
