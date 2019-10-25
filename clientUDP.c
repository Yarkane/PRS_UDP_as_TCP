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

#define domain AF_INET
#define type SOCK_DGRAM
#define protocol 0
#define BUFFER_TAILLE 63
#define TIMEOUT_SECONDS 1
#define TIMEOUT_MICRO 0

int get_numSequence(char* buffer){
  //Récupération num séquence
  char *ptr = strtok(buffer, " "); //pointeur vers "numSequence"
  int numSequence = atoi(ptr);
  return numSequence;
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
  adresse.sin_family = domain;
  adresse.sin_port = htons(atoi(argv[2])); //Port du serveur, converti en valeur réseau
  inet_aton(argv[1], &adresse.sin_addr); //Conversion de la chaine en valeur réseau, et attribution à la structure adresse
  socklen_t taille_adresse = sizeof(adresse); //Taille de l'adresse
  //Définition socket :
  int socketClient;
  if( (socketClient = socket(domain,type,protocol)) == -1)
  {
    perror("Creation de socket impossible\n");
    return -1;
  }
  printf("Descripteur : %i\n",socketClient);

  //Connexion
  bind(socketClient,(struct sockaddr*)&adresse,sizeof(adresse));

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
  char buffer[BUFFER_TAILLE];
  char ligne[BUFFER_TAILLE-6];

  const char* espace = " ";
  const char* underscore = "_";

  srand(time(NULL));
  int numSequence = rand()/10000;

  //write(socketClient,"SYN 4545",strlen("SYN 4545"));
  while(online){
    sprintf(buffer, "%i SYN",numSequence);
    sendto(socketClient, &buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
    //On attend un SYNACK
    selret = select(4,&socket_set,NULL,NULL,&timeout);
    if (selret<0)
    {
      perror("Erreur de select");
      return -1;
    }
    //si 0 : timeout, on renvoie le message de SYN
    if (selret != 0){
      recvfrom(socketClient, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
      if(strstr(buffer, "SYNACK") != NULL) {
        //Vérification numéro de séquence
        numSequence++;
        if(get_numSequence(buffer) == numSequence){
          numSequence++;
          sprintf(buffer, "%i ACK",numSequence);
          sendto(socketClient, &buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
          //On attend le port sur lequel se connecter
          selret = select(4,&socket_set,NULL,NULL,&timeout);
          if (selret<0)
          {
            perror("Erreur de select");
            return -1;
          }
          if (selret != 0){
            recvfrom(socketClient, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
            numSequence++;
            if(get_numSequence(buffer)==numSequence){
              char *ptr = strtok(NULL, " "); //pointeur vers num du port
              int port = atoi(ptr);
              printf("Port de data : %i\n",port);
              //CHANGEMENT DE port
              adresse.sin_port = htons(port); //Port du serveur data
              while(online){
                //TODO RECEPTION fichier
                //Ouverture fichier client
                FILE* fichier = fopen("fichier_client.txt", "w");
                if (fichier==NULL)
                {
                  printf("Erreur ouverture fichier !!");
                  return 0;
                }
                //Envoi premier MESSAGE
                numSequence++;
                sprintf(buffer, "%i BEGIN",numSequence);
                sendto(socketClient, &buffer, strlen(buffer), 0, (const struct sockaddr *) &adresse, taille_adresse);
                //Reception premier segment
                recvfrom(socketClient, buffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse, &taille_adresse);
                numSequence++;
                if(get_numSequence(buffer)==numSequence){
                  char *ptr = strtok(NULL," ");
                  sprintf(ligne,"%s",ptr);
                  //Traitement de la ligne : pour le bien de l'envoi, les espaces du message ont été convertis en underscore
                  replace_str(ligne,underscore,espace);
                  fputs(ligne, fichier);
                  //Loop :
                    //envoi acknowledgement
                    //incrementation num sequence
                    //reception segment
                }
                //+procédure END
                fclose(fichier); // On ferme le fichier qui a été ouvert
              }
            }
          }
        }
      }
    }
  }
  return 0;
}