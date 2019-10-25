#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define domain AF_INET
#define type_UDP SOCK_DGRAM
#define protocol 0
#define BUFFER_TAILLE 63
#define TIMEOUT_SECONDS 1
#define TIMEOUT_MICRO 0

/*
  UNE REGLE SUR LE FICHIER A envoyer
  UNE LIGNE NE DOIT PAS COMMENCER PAR END !!!
*/

int get_numSequence(char* buffer){
  //Récupération num séquence
  char *ptr = strtok(buffer, " "); //pointeur vers "numSequence"
  int numSequence = atoi(ptr);
  return numSequence;
}

void *replace_str(char *str, const char *orig, const char *rep)
{
  //FONCTION REMPLACANT CERTAINS CARACTERES DANS CHAINE DE CARACTERES
  //Utilisé pour formater ligne à envoyer
  char *p;
  while((p = strstr(str,orig)))
  {
    *p = *rep;
  }
  return 0;
}

//TODO : LECTURE FICHIER (integration dans arguments)

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
  adresseUDP.sin_family = domain;
  adresseUDP.sin_port = htons(atoi(argv[1])); //Port du serveur, converti en valeur réseau
  adresseUDP.sin_addr.s_addr = htonl(INADDR_ANY);

  //Définition socket UDP
  int socketServUDP;
  if( (socketServUDP = socket(domain,type_UDP,protocol)) == -1)
  {
    perror("Creation de socket impossible\n");
  	return -1;
  }
  printf("Descripteur UDP : %i\n",socketServUDP);
  //Lien entre les deux :
  bind(socketServUDP,(struct sockaddr*)&adresseUDP,sizeof(adresseUDP));

  //Définition adresse UDP de connexion :
  struct sockaddr_in adresseUDP_data; //Structure contenant addresse serveur
  memset((char*)&adresseUDP_data,0,sizeof(adresseUDP_data));
  adresseUDP_data.sin_family = domain;
  adresseUDP_data.sin_port = htons(atoi(argv[2])); //Port du serveur, converti en valeur réseau
  adresseUDP_data.sin_addr.s_addr = htonl(INADDR_ANY);

  //Définition socket UDP
  int socketServUDP_data;
  if( (socketServUDP_data = socket(domain,type_UDP,protocol)) == -1)
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
  int online = 1;
  int numSequence;
  char buffer[BUFFER_TAILLE];
  int selret, isInit;
  const char* espace = " ";
  const char* underscore = "_";

  //Descripteur
  fd_set socket_set;
  FD_ZERO(&socket_set);
  //timeoute
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_SECONDS;
  timeout.tv_usec = TIMEOUT_MICRO;


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
      recvfrom(socketServUDP, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_arrivee, &taille_arrivee);
      if(strcmp(buffer,"") != 0) printf("Reçu UDP : %s\n",buffer);
      //recuperation de l'adresse
      ipClient = inet_ntoa(adresse_arrivee.sin_addr);
      portClient = ntohs(adresse_arrivee.sin_port);
      printf("Adresse = %s\n",ipClient);
      printf("Port = %i\n",portClient);

      //initialisation de connexion (reception SYN)
      if(strstr(buffer, " SYN") != NULL) {
        //Récupération num séquence
        numSequence = get_numSequence(buffer);
        //Envoi du SYNACK
        numSequence++;
        sprintf(buffer,"%i SYNACK",numSequence);
        sendto(socketServUDP, buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse_arrivee, taille_arrivee);
        //Attente pour le ACK
        FD_CLR(socketServUDP_data,&socket_set);
        selret = select(5,&socket_set,NULL,NULL,&timeout);
        if (selret<0)
        {
          perror("Erreur de select");
          return -1;
        }
        if (selret !=0){
          numSequence++;
          recvfrom(socketServUDP, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_arrivee, &taille_arrivee);
          if(strstr(buffer," ACK")){
            if(get_numSequence(buffer)==numSequence) {
              numSequence++;
              sprintf(buffer,"%i %i",numSequence,atoi(argv[2])); //Envoi du port de données
              sendto(socketServUDP, buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse_arrivee, taille_arrivee);
            }
          }
        }
      }
    }
    else{
      //TODO : PORT data
      recvfrom(socketServUDP_data, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
      if(strcmp(buffer,"") != 0) printf("Reçu UDP : %s\n",buffer);
      //recuperation de l'adresse
      ipClient_data = inet_ntoa(adresse_data.sin_addr);
      portClient_data = ntohs(adresse_data.sin_port);
      printf("Adresse = %s\n",ipClient_data);
      printf("Port = %i\n",portClient_data);
      //Premier message ?????
      if(strstr(buffer, " BEGIN") != NULL) {
        //Récupération num séquence
        numSequence = get_numSequence(buffer);
        //Ouverture fichier
        fichier = fopen("fichier_serveur.txt", "r");
        if (fichier==NULL)
        {
          printf("Erreur ouverture fichier !!");
          //todo : envoyer "erreur" pour que le client ferme la connexion
          return 0;
        }
        while (fgets(buffer, BUFFER_TAILLE-6, fichier) != NULL){
          //Traitement de la ligne : pour le bien de l'envoi, les espaces du message doivent être convertis en underscore
          replace_str(buffer,espace,underscore);
          //envoi
          numSequence++;
          sprintf(buffer,"%i %s",numSequence,buffer);
          sendto(socketServUDP_data, buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse_data, taille_data);
          //attente de l'ACK
          recvfrom(socketServUDP_data, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
          numSequence++;
          if((strstr(buffer, "ACK") != NULL) && (numSequence == get_numSequence(buffer))) {
            printf("ligne envoyée et reçue\n");
          }
          //else : renvoyer le message
        }
        printf("fin du fichier\n");
        numSequence++;
        sprintf(buffer,"%i END",numSequence);
        sendto(socketServUDP_data, buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse_data, taille_data);
        //Attente ENDACK
        recvfrom(socketServUDP_data, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
        printf("Reçu UDP : %s\n",buffer);
        numSequence++;
        if((strstr(buffer, "ENDACK") != NULL) && (numSequence == get_numSequence(buffer))) {
          //envoi ACK et fin de transmission
          printf("ENDACK reçu\n");
          numSequence++;
          sprintf(buffer,"%i ACK",numSequence);
          sendto(socketServUDP_data, buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse_data, taille_data);
          fclose(fichier); // On ferme le fichier qui a été ouvert
        }
        else{
          printf("Erreur numsequence ou type de message\n");
          printf("Reçu UDP : %s\n",buffer);
        //else : renvoi END
        }
      }
    }
  }
  return 0;
}
