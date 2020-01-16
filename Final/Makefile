CFLAGS = -g -Wall -DDEBUG
LDFLAGS = -g -Wall -DDEBUG -lm

all: serveur_SlowStart_FastRetransmit serveur_NoSlowStart

serveur_SlowStart_FastRetransmit.o: serveur_SlowStart_FastRetransmit.c
	gcc ${CFLAGS} -c serveur_SlowStart_FastRetransmit.c -o serveur_SlowStart_FastRetransmit.o

serveur_SlowStart_FastRetransmit: serveur_SlowStart_FastRetransmit.o
	gcc ${LDFLAGS} serveur_SlowStart_FastRetransmit.o -o serveur_SlowStart_FastRetransmit

serveur_NoSlowStart.o: serveur_NoSlowStart.c
		gcc ${CFLAGS} -c serveur_NoSlowStart.c -o serveur_NoSlowStart.o

serveur_NoSlowStart: serveur_NoSlowStart.o
		gcc ${LDFLAGS} serveur_NoSlowStart.o -o serveur_NoSlowStart

clean:
	\rm -rf *.o main  *~ serveur_NoSlowStart serveur_SlowStart_FastRetransmit
