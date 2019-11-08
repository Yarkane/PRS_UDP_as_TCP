#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define DOMAIN AF_INET
#define TYPE SOCK_DGRAM
#define PROTOCOL 0
#define BUFFER_TAILLE 128
#define TIMEOUT_SECONDS 1
#define TIMEOUT_MICRO 0

/*
  DEUX REGLEs SUR LE FICHIER A envoyer
    - UNE LIGNE NE DOIT PAS COMMENCER PAR END !!!
    - PAS DE LIGNE  à moins de 2 caractères
*/

int get_numSequence(char* buffer, char* buffer2){
  //Récupération num séquence
  memcpy(buffer2,buffer,6);
  int numSequence = atoi(buffer2);
  return numSequence;
}

void fError(int type){
  //Fonction se déclenchant lors d'un timeout ou erreur de numSequence lors de l'ack.
  //Utilisée pour la congestion lors de la réception d'un fichier.

}

void fSuccess(){
  //Fonction se déclenchant lors de la reception d'un Ack.
  //Utilisée pour la congestion lors de la réception d'un fichier.
  printf("ACK received.\n");
}

int main(int argc, char *argv[])
{
  //Arguments :
  if (argc != 4)
  {
    printf("Mauvais usage : client <ip_serveur> <port_serveur> <nom_du_fichier>\n");
    return 0;
  }

  char nomfichier[64];
  sprintf(nomfichier,"%s",argv[3]);

  char nomfichierserverside[69];
  sprintf(nomfichierserverside,"recp_%s",nomfichier);

  //Définition adresse :
  struct sockaddr_in adresse; //Structure contenant addresse serveur
  memset((char*)&adresse,0,sizeof(adresse));
  adresse.sin_family = DOMAIN;
  adresse.sin_port = htons(atoi(argv[2])); //Port du serveur, converti en valeur réseau
  inet_aton(argv[1], &adresse.sin_addr); //Conversion de la chaine en valeur réseau, et attribution à la structure adresse
  socklen_t taille_adresse = sizeof(adresse); //Taille de l'adresse
  //Définition socket :
  int socketClient;
  if( (socketClient = socket(DOMAIN,TYPE,PROTOCOL)) == -1)
  {
    perror("Creation de socket impossible\n");
    return -1;
  }
  printf("Descripteur : %i\n",socketClient);

  //Connexion
  bind(socketClient,(struct sockaddr*)&adresse,sizeof(adresse));

  //Ouverture fichier client
  FILE* fichier = fopen(nomfichierserverside, "w");
  if (fichier==NULL)
  {
    printf("Erreur ouverture fichier !!");
    return 0;
  }

  //Descripteur
  fd_set socket_set;
  FD_ZERO(&socket_set);
  FD_SET(socketClient, &socket_set); //Activation du bit associé à au socket UDP
  //timeoute
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_SECONDS;
  timeout.tv_usec = TIMEOUT_MICRO;

  //Envoi Port
  /*
  int port;
  socklen_t size_adresse = sizeof(adresse);
  if (getsockname(socketClient, (struct sockaddr *)&adresse, &size_adresse) == -1)
      perror("getsockname");
  else port = ntohs(adresse.sin_port);
  sprintf(msg,"%i",port);
  write(socketClient,msg,BUFFER_TAILLE);
  */

  //CONNEXIONchar *ptr = buffer
  int online = 1;
  int selret;
  char sendBuffer[BUFFER_TAILLE];
  char recvBuffer[BUFFER_TAILLE];
  char bloc[BUFFER_TAILLE-6];
  char bufferType[9];

  int unreceived;

  srand(time(NULL));
  int numSequence;

  //write(socketClient,"SYN 4545",strlen("SYN 4545"));
  while(online){
    //Procédure de connexion au serveur
    //Envoi de SYN et attente de SYNACK
    memset(sendBuffer, 0, sizeof(sendBuffer));
    sprintf(sendBuffer, "SYN");


    unreceived = 1;
    while(unreceived){
      sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
      //On attend un SYNACK
      timeout.tv_sec = TIMEOUT_SECONDS;
      timeout.tv_usec = TIMEOUT_MICRO;
      FD_SET(socketClient, &socket_set); //Activation du bit associé à au socket UDP
      selret = select(4,&socket_set,NULL,NULL,&timeout);
      if (selret<0)
      {
        perror("Erreur de select");
        return -1;
      }
      else if (selret == 0) fError(1);
      else if (selret != 0){
        recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        //Vérification message reçu
        memcpy(bufferType,recvBuffer,7);
        if(strcmp(bufferType, "SYN-ACK") == 0){
          //Message valide
          printf("Reçu UDP : %s\n",recvBuffer);
          unreceived = 0;
          //Pas en procédure de réception : pas d'appel à fSuccess ou fError.
        }
      }
    }

    //envoi ACK
    memset(sendBuffer, 0, BUFFER_TAILLE);
    sprintf(sendBuffer, "ACK");
    sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
    //On récupère le port de data

    char *ptr = recvBuffer + 7; //pointeur vers num du port
    int port = atoi(ptr);
    printf("Port de data : %i\n",port);
    //CHANGEMENT DE port
    adresse.sin_port = htons(port); //Port du serveur data
    while(online){
      //Envoi premier MESSAGE : nom du fichier désiré
      memset(sendBuffer, 0, sizeof(sendBuffer));
      sprintf(sendBuffer, "%s",nomfichier);
      printf("envoi nom fichier : %s\n",sendBuffer);
      sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
      //Reception premier message
      recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
      //premier message : on récupère numseq
      numSequence = get_numSequence(recvBuffer,bufferType);
      while(strcmp(recvBuffer,"FIN")) //JUsqu'à ce qu'on reçoive FIN
      {
        printf("ligne reçue\n");
        printf("Reçu UDP : %s\n",recvBuffer);
        while(!(get_numSequence(recvBuffer,bufferType)==numSequence)){
          //printf("numSequence invalide : %i\n",numSequence);
          sendto(socketClient, &sendBuffer, strlen(sendBuffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
          recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        }
        char *ptr = recvBuffer + 6;
        sprintf(bloc,"%s",ptr);
        //Traitement de la ligne : pour le bien de l'envoi, les espaces du message ont été convertis en underscore
        fputs(bloc, fichier);
        printf("Ecriture.\n");
        memset(recvBuffer, 0, sizeof(recvBuffer));
        //envoi ack
        //numSequence++; -> on n'incrémente pas, on acknowledge le paquet précédent
        memset(sendBuffer, 0, sizeof(sendBuffer));
        sprintf(sendBuffer, "ACK%i",numSequence);
        sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
        //reception prochain segment
        numSequence++;
        recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
      }

      printf("FIN\n");

      fclose(fichier); // On ferme le fichier qui a été ouvert
      //fin de la transmission
      online = 0;
    }
  }
  return 0;
}
