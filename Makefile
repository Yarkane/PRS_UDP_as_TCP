CFLAGS = -g -Wall -DDEBUG
LDFLAGS = -g -Wall -DDEBUG -lm

all: clientUDP serveur

clientUDP.o: clientUDP.c
	gcc ${CFLAGS} -c clientUDP.c -o clientUDP.o

clientUDP: clientUDP.o
	gcc ${LDFLAGS} clientUDP.o -o clientUDP

serveur.o:	serveur.c
	gcc ${CFLAGS} -c serveur.c -o serveur.o

serveur: serveur.o
	gcc ${LDFLAGS} serveur.o -o serveur


clean:
	\rm -rf *.o main  *~ client serveur serveur_fork clientUDP
