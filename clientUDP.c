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
#define BUFFER_TAILLE 63
#define TIMEOUT_SECONDS 1
#define TIMEOUT_MICRO 0

/*
  DEUX REGLEs SUR LE FICHIER A envoyer
    - UNE LIGNE NE DOIT PAS COMMENCER PAR END !!!
    - PAS DE LIGNE  à moins de 2 caractères
*/

int get_numSequence(char* buffer){
  //Récupération num séquence
  //note : effectuer strtok après l'exécution de la fonction permettra de
  //faire pointer le pointeur sur la suite du message
  char *ptr = strtok(buffer, " "); //pointeur vers "numSequence"
  int numSequence = atoi(ptr);
  return numSequence;
}

void fError(int type){
  //Fonction se déclenchant lors d'un timeout ou erreur de numSequence lors de l'ack.
  //Utilisée pour la congestion lors de la réception d'un fichier.
  if (type == 1) printf("Timeout !\n");
  else if (type == 2) printf("Numéro de séquence invalide !\n");
}

void fSuccess(){
  //Fonction se déclenchant lors de la reception d'un Ack.
  //Utilisée pour la congestion lors de la réception d'un fichier.
  printf("ACK received.\n");
}

void *replace_str(char *str, const char *orig, const char *rep)
{
  //FONCTION REMPLACANT CERTAINS CARACTERES DANS CHAINE DE CARACTERES
  //Utilisé pour convertir ligne reçue
  char *p;
  while((p = strstr(str,orig)))
  {
    *p = *rep;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  //Arguments :
  if (argc != 3)
  {
    printf("Mauvais usage : client <ip_serveur> <port_serveur>\n");
    return 0;
  }
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
  FILE* fichier = fopen("fichier_client.txt", "w");
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

  //CONNEXION
  int online = 1;
  int selret;
  char sendBuffer[BUFFER_TAILLE];
  char recvBuffer[BUFFER_TAILLE];
  char ligne[BUFFER_TAILLE-7];

  const char* espace = " ";
  const char* underscore = "_";

  int unreceived;

  srand(time(NULL));
  int numSequence = rand()/10000;

  //write(socketClient,"SYN 4545",strlen("SYN 4545"));
  while(online){
    //Procédure de connexion au serveur
    //Envoi de SYN et attente de SYNACK
    memset(sendBuffer, 0, sizeof(sendBuffer));
    sprintf(sendBuffer, "%i SYN",numSequence);


    unreceived = 1;
    while(unreceived){
      sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
      //On attend un SYNACK
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
        if((strstr(recvBuffer, "SYNACK") != NULL) && (get_numSequence(recvBuffer)==numSequence + 1)){
          //Message valide
          numSequence++;
          printf("Reçu UDP : %s\n",recvBuffer);
          unreceived = 0;
          //Pas en procédure de réception : pas d'appel à fSuccess ou fError.
        }
      }
    }

    //TODO :
    //Créer une fonction "sendWithTimeoutAndMessageCheck" exécutant le bloc plus haut
    //le placer partout où nécessaire
    //répliquer du côté client
    //important pour gagner en clarté !



    //envoi ACK
    numSequence++;
    memset(sendBuffer, 0, BUFFER_TAILLE);
    sprintf(sendBuffer, "%i ACK",numSequence);
    sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
    //On attend le port sur lequel se connecter
    selret = select(4,&socket_set,NULL,NULL,&timeout);
    if (selret<0)
    {
      perror("Erreur de select");
      return -1;
    }
    if (selret != 0){
      recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
      numSequence++;
      while(!(get_numSequence(recvBuffer)==numSequence)){
        printf("numSequence invalide\n");
        sendto(socketClient, &sendBuffer, strlen(sendBuffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
        recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
      }
      char *ptr = strtok(NULL, " "); //pointeur vers num du port
      int port = atoi(ptr);
      printf("Port de data : %i\n",port);
      //CHANGEMENT DE port
      adresse.sin_port = htons(port); //Port du serveur data
      while(online){
        //Envoi premier MESSAGE
        numSequence++;
        memset(sendBuffer, 0, sizeof(sendBuffer));
        sprintf(sendBuffer, "%i BEGIN",numSequence);
        sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
        //Receptions
        recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        while(!strstr(recvBuffer," END"))
        {
          printf("ligne reçue\n");
          printf("Reçu UDP : %s\n",recvBuffer);
          numSequence++;
          while(!(get_numSequence(recvBuffer)==numSequence)){
            printf("numSequence invalide\n");
            sendto(socketClient, &sendBuffer, strlen(sendBuffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
            recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
          }
          char *ptr = strtok(NULL," ");
          sprintf(ligne,"%s",ptr);
          //Traitement de la ligne : pour le bien de l'envoi, les espaces du message ont été convertis en underscore
          replace_str(ligne,underscore,espace);
          printf("%s\n",ligne);
          fputs(ligne, fichier);
          //envoi ack
          //numSequence++; -> on n'incrémente pas, on acknowledge le paquet précédent
          memset(sendBuffer, 0, sizeof(sendBuffer));
          sprintf(sendBuffer, "%i ACK",numSequence);
          sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
          //reception prochain segment
          recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        }
        printf("END reçu\n");
        //procédure END
        //TODO : vérification num sequence du END
        numSequence++;
        numSequence++;
        memset(sendBuffer, 0, sizeof(sendBuffer));
        sprintf(sendBuffer, "%i ENDACK",numSequence);
        sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
        //Reception du ACK final
        recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        printf("Reçu UDP : %s\n",recvBuffer);
        numSequence++;
        while(!((strstr(recvBuffer," ACK")) && (get_numSequence(recvBuffer)==numSequence))){
          printf("numSequence ou TYPE invalide\n");
          sendto(socketClient, &sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse, taille_adresse);
          recvfrom(socketClient, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
        }
        fclose(fichier); // On ferme le fichier qui a été ouvert
        //fin de la transmission
        online = 0;
      }
    }
  }
  return 0;
}
