//todo : fork avant envoi synack
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
#include <sys/stat.h>
#include <math.h>

#define DOMAINE AF_INET
#define TYPE SOCK_DGRAM
#define PROTOCOL 0
#define BUFFER_TAILLE 128
#define ALPHA 100 //Facteur arbitraire : timeout = alpha * rtt

/*
  DEUX REGLEs SUR LE FICHIER A envoyer
    - UNE LIGNE NE DOIT PAS COMMENCER PAR END !!!
    - PAS DE LIGNE à moins de 2 caractères
*/

int get_beginWindow(char* buffer, char* buffer2){
  //Récupération num séquence
  memcpy(buffer2,buffer+3,6);
  int beginWindow = atoi(buffer2);
  return beginWindow;
}

int min(int a, int b){
  if (a<b) return a;
  else return b;
}

int main(int argc, char *argv[])
{
  //Arguments :
  if (argc != 3)
  {
    printf("Mauvais usage : serveur <port_UDP_connexion> <port_UDP_donnees>\n");
    printf("argv[0] : %s\n",argv[0]);
    return 0;
  }

  //Définition adresse UDP de connexion :
  struct sockaddr_in adresseUDP; //Structure contenant addresse serveur
  memset((char*)&adresseUDP,0,sizeof(adresseUDP));
  adresseUDP.sin_family = DOMAINE;
  adresseUDP.sin_port = htons(atoi(argv[1])); //Port du serveur, converti en valeur réseau
  adresseUDP.sin_addr.s_addr = htonl(INADDR_ANY);

  //Définition socket connexion udp
  int socketServUDP;
  if( (socketServUDP = socket(DOMAINE,TYPE,PROTOCOL)) == -1)
  {
    perror("Creation de socket impossible\n");
  	return -1;
  }
  printf("Descripteur UDP : %i\n",socketServUDP);
  //Lien entre les deux :
  bind(socketServUDP,(struct sockaddr*)&adresseUDP,sizeof(adresseUDP));

  //Définition adresse UDP de data
  struct sockaddr_in adresseUDP_data; //Structure contenant addresse serveur
  memset((char*)&adresseUDP_data,0,sizeof(adresseUDP_data));
  adresseUDP_data.sin_family = DOMAINE;
  adresseUDP_data.sin_port = htons(atoi(argv[2])); //Port du serveur, converti en valeur réseau
  adresseUDP_data.sin_addr.s_addr = htonl(INADDR_ANY);

  //Définition socket UDP de data
  int socketServUDP_data;
  if( (socketServUDP_data = socket(DOMAINE,TYPE,PROTOCOL)) == -1)
  {
    perror("Creation de socket impossible\n");
    return -1;
  }
  printf("Descripteur UDP : %i\n",socketServUDP_data);
  //Lien entre les deux :
  bind(socketServUDP_data,(struct sockaddr*)&adresseUDP_data,sizeof(adresseUDP_data));

  //Ecoutes (sockets passifs)
  listen(socketServUDP,1);
  listen(socketServUDP_data,1);

  //Initialisation variable adresse client pour socket connexion
  struct sockaddr_in adresse_arrivee; //Structure contenant addresse client
  memset((char*)&adresse_arrivee,0,sizeof(adresse_arrivee));
  socklen_t taille_arrivee = sizeof(adresse_arrivee); //Taille de l'adresse client
  char* ipClient;
  int portClient;

  //Initialisation variable adresse client pour socket data
  struct sockaddr_in adresse_data; //Structure contenant addresse client
  memset((char*)&adresse_data,0,sizeof(adresse_data));
  socklen_t taille_data = sizeof(adresse_data); //Taille de l'adresse client
  char* ipClient_data;
  int portClient_data;

  FILE* fichier;
  int filedescriptor;
  char nomfichier[64];
  int taillefichier;

  int online = 1;
  int unreceived;
  int problem;
  char sendBuffer[BUFFER_TAILLE];
  char recvBuffer[BUFFER_TAILLE];
  char typeBuffer[7];
  char strSequence[7];
  int selret, isInit;

  float timer;
  float RoundTripTime;
  int timeToWait;

  //Définition de la window
  //Window = espace entre beginWindow et endWindow
  int beginWindow = 0; //Numéro de séquence partagé entre client et serveur
  int endWindow = 0; //Numéro de séquence des messages envoyés / à envoyer

  //Controle de congestion
  int windowSize = 1; //S'incrémente avec slowstart
  int nWrongAcks = 0; //Compteur de ACK "inappropriés"

  //Descripteur
  fd_set socket_set;
  FD_ZERO(&socket_set);
  //timeoute
  struct timeval timeout;
  //La valeur du timeout sera renseignée après calcul du RTT


  while(online) //Boucle de connexion
  {
    //TODO : SELECT
    FD_SET(socketServUDP, &socket_set); //Activation du bit associé à au socket UDP de CONNEXION
    FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
    selret = select(5,&socket_set,NULL,NULL,NULL);
    if (selret<0)
    {
      perror("Erreur de select");
      return -1;
    }
    isInit = FD_ISSET(socketServUDP,&socket_set); //SI MESSAGE UDP
    if(isInit){
      //PORT CONNEXION
      FD_CLR(socketServUDP_data,&socket_set);
      recvfrom(socketServUDP, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_arrivee, &taille_arrivee);
      if(strcmp(recvBuffer,"") != 0) printf("Reçu UDP : %s\n",recvBuffer);
      //recuperation de l'adresse
      ipClient = inet_ntoa(adresse_arrivee.sin_addr);
      portClient = ntohs(adresse_arrivee.sin_port);
      printf("Adresse = %s\n",ipClient);
      printf("Port = %i\n",portClient);

      //initialisation de connexion (reception SYN)
      if(strcmp(recvBuffer, "SYN") == 0) {
        //Récupération num séquence
        //Envoi du SYNACK
        unreceived = 1;
        memset(sendBuffer, 0, sizeof(sendBuffer));
        sprintf(sendBuffer,"SYN-ACK%i",atoi(argv[2]));
        while(unreceived){
          sendto(socketServUDP, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_arrivee, taille_arrivee);
          //Message envoyé ! On initialise un timer pour estimer le RTT
          timer = clock();
          //Attente pour le ACK
          FD_SET(socketServUDP, &socket_set); //Activation du bit associé à au socket UDP de CONNEXION
          selret = select(5,&socket_set,NULL,NULL,&timeout);
          if (selret<0)
          {
            perror("Erreur de select");
            return -1;
          }
          else if ((selret != 0)) {
            recvfrom(socketServUDP, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_arrivee, &taille_arrivee);
            if(strcmp(recvBuffer,"ACK")==0) unreceived = 0;
          }
        }
        //Ack bel et bien Reçu : on arrête le timer
        RoundTripTime = (clock() - timer)/CLOCKS_PER_SEC; //CLOCKS_PER_SEC : valeur initialisée dans time.h, nb de ticks dans une seconde
        printf("RTT : %f\n",RoundTripTime);
        //Choix de la valeur du timeout
        timeToWait = round(1000000 * RoundTripTime * ALPHA); //Facteur arbitraire, en microsecondes
        printf("Timeout (microseconds) : %i\n",timeToWait);
        timeout.tv_sec = 0;
        timeout.tv_usec = timeToWait; //Initialisation du timer (à répéter avant chaque select)
      }
    }
    else{
      //PORT DATA
      FD_CLR(socketServUDP, &socket_set); //Activation du bit associé à au socket UDP de CONNEXION
      recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
      if(strcmp(recvBuffer,"") != 0) printf("Reçu UDP : %s\n",recvBuffer);
      //recuperation de l'adresse
      ipClient_data = inet_ntoa(adresse_data.sin_addr);
      portClient_data = ntohs(adresse_data.sin_port);
      printf("Adresse = %s\n",ipClient_data);
      printf("Port = %i\n",portClient_data);
      //beginWindow = rand()/10000;
      beginWindow = 0;
      endWindow = 0;
      //récuperation nom de fichier
      memcpy(nomfichier,recvBuffer,min(BUFFER_TAILLE,64));
      //Ouverture fichier
      fichier = fopen(nomfichier, "r");
      printf("%s\n",nomfichier);
      if (fichier==NULL)
      {
        printf("[!] File not found !\n");
        //On enverra le contenu de "error.txt"
        fichier = fopen("error.txt","r");
      }
      //lecture du fichier en UNE FOIS
      //Récupération informations sur le fichier
      struct stat carac;
      filedescriptor = fileno(fichier);
      fstat(filedescriptor, &carac);
      taillefichier = carac.st_size;
      //attribution espace fichier
      printf("Taille du fichier : %i\n",taillefichier);
      char* file=(char*)malloc(taillefichier+3); //Allocation d'un tableau comportant le contenu du fichier
      if (file==0)
      {
        printf("Erreur d'allocation.\n");
        return(-1);
      }
      else printf("Allocation réussie.\n");
      printf("Taille file : %ld\n",sizeof(file));
      fread(file,1,taillefichier,fichier); //lecture du fichier en une seule FOIS
      printf("Copie réussie.\n");
      fclose(fichier);

      //taillefichier converti en nombre de segments à envoyer
      int restefichier = taillefichier%(BUFFER_TAILLE - 6);
      taillefichier = taillefichier/(BUFFER_TAILLE - 6) + 1;
      printf("Nb de segments : %i\n",taillefichier);

      //Boucle d'envoi du fichier
      int i;
      windowSize = 1;
      while(beginWindow < taillefichier){
        endWindow = beginWindow + windowSize;
        if (endWindow > taillefichier){
          endWindow = taillefichier;
        }
        //Boucle de transmission
        for(int j = beginWindow; j<=endWindow; j++){
          i = (BUFFER_TAILLE - 6) * j;
          //Construction du message
          sprintf(typeBuffer,"000000"); //Base du numéro de séquence
          sprintf(strSequence,"%i",j); //Numéro de séquence en str
          char *ptr = typeBuffer + (6-strlen(strSequence)); //Pointeur du début d'écriture pour le numéro de séquence
          memcpy(ptr,strSequence,strlen(strSequence)); //Ecriture au ponteur
          printf("Numéro de séquence : %s\n",typeBuffer);
          memset(sendBuffer, 0, sizeof(sendBuffer));
          memset(recvBuffer, 0, sizeof(recvBuffer));
          // sprintf(sendBuffer,"%s%s",typeBuffer,bloc); //formation du message à envoyer
          //La ligne précédente utilise sprintf. Cela posera problème si l'on a un caractère \0 .
          memcpy(sendBuffer,typeBuffer,6);
          if (j == taillefichier) memcpy(sendBuffer+6,file+i,restefichier); //Si moins de BUFFER_TAILLE - 6 à envoyer
          else memcpy(sendBuffer+6,file+i,BUFFER_TAILLE-6);
          //Envoi du message
          int a;
          if (j == taillefichier) a = sendto(socketServUDP_data, sendBuffer, restefichier + 6, 0, (const struct sockaddr *) &adresse_data, taille_data);
          else a = sendto(socketServUDP_data, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_data, taille_data);
          printf("%i\n",a);
        }

        //Boucle de réception
        problem = 0;
        while((!problem) && (beginWindow!=endWindow)){
          FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
          timeout.tv_usec = timeToWait; //Initialisation du timer
          selret = select(5,&socket_set,NULL,NULL,&timeout);
          if (selret<0)
          {
            perror("[!] Select Error");
            return -1;
          }
          else if (selret == 0) {
            windowSize = 1;
            problem = 1;
            printf("[-] Timeout.\n");
          }
          else if (selret != 0) {
            //Vérification nature du message reçu
            recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
            if(!((strstr(recvBuffer, "ACK") != NULL) && (get_beginWindow(recvBuffer,typeBuffer) >= beginWindow))) {
              nWrongAcks++;
              if (nWrongAcks==3){
                problem = 1;
                nWrongAcks = 0;
                windowSize = 1;
                printf("[-] 3 wrong acks.\n");
              }
            }
            else{
              //message bien reçu et acknowlegded
              printf("[+] Ack number %i received.\n",beginWindow);
              nWrongAcks = 0;
              beginWindow = get_beginWindow(recvBuffer,typeBuffer) + 1;
              printf("beginWindow du client : %i\n",beginWindow);
              windowSize++;
            }
          }
        }
      }

        /*
        unreceived = 1;
        while(unreceived){
          //TODO : en fonction de la taille de la fenêtre et de la nécessité de renvoyer ou non,
          //trouver un moyen de "moduler" les envois
          sendto(socketServUDP_data, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_data, taille_data);
          //Attente Ack
          FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
          timeout.tv_usec = timeToWait; //Initialisation du timer
          selret = select(5,&socket_set,NULL,NULL,&timeout);
          if (selret<0)
          {
            perror("[!] Select Error");
            return -1;
          }
          else if (selret == 0) fError(1,window,nWrongAcks);
          else if (selret != 0) {
            //Vérification nature du message reçu
            recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
            if(!((strstr(recvBuffer, "ACK") != NULL) && (get_beginWindow(recvBuffer,typeBuffer) == beginWindow))) fError(2,window,nWrongAcks); //Fast retransmit : 3 ack dupliqués pour renvoyer !
            else{
              //message bien reçu et acknowlegded
              printf("[+] Ack number %i received.\n",beginWindow);
              nWrongAcks = 0;
              unreceived = 0;
              fSuccess();
            }
          }
        }
        */
      //Fin du fichier atteint : procédure end
      printf("end of file\n");
      memset(sendBuffer, 0, sizeof(sendBuffer));
      sprintf(sendBuffer,"FIN");
      sendto(socketServUDP_data, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_data, taille_data);
      free(file); // On libère l'espace attribué pour la lecture
      online = 0; //Arrêt du serveur
    }
  }
  return 0;
}
