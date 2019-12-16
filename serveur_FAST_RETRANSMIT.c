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
#define BUFFER_TAILLE 1500
#define ALPHA 1.2 //Facteur arbitraire : timeout = alpha * srtt
#define ALPHASRTT 0.9
#define THRESHOLD 50 //passage de slow start à congestion avoidance
#define INITIAL_WINDOW 2

void delay(int number_of_microseconds)
{
    // Storing start time
    clock_t start_time = clock();

    // looping till required time is not acheived
    while (clock() < start_time + number_of_microseconds)
        ;
}

int get_numSequence(char* buffer, char* buffer2){
  //Récupération num séquence
  memcpy(buffer2,buffer+3,6);
  int beginWindow = atoi(buffer2);
  return beginWindow;
}

int min(int a, int b){
  if (a<b) return a;
  else return b;
}

float minfloat(float a, float b){
  if (a<b) return a;
  else return b;
}


int max(int a, int b){
  if (a>b) return a;
  else return b;
}

int main(int argc, char *argv[])
{
  //Arguments :
  if (argc != 4)
  {
    printf("Bad usage : serveur <port_UDP_connexion> <premier_port_UDP_donnees> <nb_ports_données_disponibles>\n");
    printf("argv[0] : %s\n",argv[0]);
    return 0;
  }

  int nbPorts = atoi(argv[3]);

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
  adresseUDP_data.sin_addr.s_addr = htonl(INADDR_ANY);

  //Initialisation variable adresse client pour socket data
  struct sockaddr_in adresse_data; //Structure contenant addresse client
  memset((char*)&adresse_data,0,sizeof(adresse_data));
  socklen_t taille_data = sizeof(adresse_data); //Taille de l'adresse client
  char* ipClient_data;
  int portClient_data;

  //Ecoute (socket passif)
  listen(socketServUDP,1);


  //Initialisation variable adresse client pour socket connexion
  struct sockaddr_in adresse_arrivee; //Structure contenant addresse client
  memset((char*)&adresse_arrivee,0,sizeof(adresse_arrivee));
  socklen_t taille_arrivee = sizeof(adresse_arrivee); //Taille de l'adresse client
  char* ipClient;
  int portClient;


  FILE* fichier;
  int filedescriptor;
  char nomfichier[64];
  int taillefichier;
  int portIncrement = 0;

  int online = 1;
  int unreceived;
  int problem;
  char sendBuffer[BUFFER_TAILLE];
  char recvBuffer[BUFFER_TAILLE];
  char typeBuffer[7];
  char strSequence[7];
  int selret;
  int portData;

  float timer;
  long int timeToWait = 2000; //en MICROSECONDES
  double TempSRTT;
  double srtt = 0.002;
  int measurement = 0; //valeur du paquet dont on mesure le RTT, 0 sinon aucune mesure en cours

  //Définition de la window
  //Window = espace entre beginWindow et endWindow
  int beginWindow = 0; //Numéro de séquence partagé entre client et serveur
  int endWindow = 0; //Numéro de séquence des messages envoyés / à envoyer

  //Controle de congestion
  int windowSize = INITIAL_WINDOW; //S'incrémente avec slowstart et congestion avoidance
  int ssthresh = THRESHOLD; //Valeur de passage de Slown Start à Congestion Avoidance
  int nWrongAcks = 0; //Compteur de ACK "inappropriés"
  int receivedAck = 0; //Ack reçu

  //Descripteur
  fd_set socket_set;
  FD_ZERO(&socket_set);
  //timeoute
  struct timeval timeout;
  //La valeur du timeout sera renseignée après calcul du RTT

  int id = getpid();

  while(online) //Boucle de connexion
  {
    /*
    FD_SET(socketServUDP, &socket_set); //Activation du bit associé à au socket UDP de CONNEXION
    selret = select(5,&socket_set,NULL,NULL,NULL);
    if (selret<0)
    {
      perror("Erreur de select");
      return -1;
    }
    isInit = FD_ISSET(socketServUDP,&socket_set); //SI MESSAGE UDP
    */
    if(id != 0){
      //SERVEUR CONNEXION
      delay(100000); //Délai afin de laisser au fils le temps de recevoir un ACK selon le cas de figure
      recvfrom(socketServUDP, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_arrivee, &taille_arrivee);
      if(strcmp(recvBuffer,"") != 0) printf("Reçu UDP (port serveur) : %s\n",recvBuffer);
      //recuperation de l'adresse
      ipClient = inet_ntoa(adresse_arrivee.sin_addr);
      portClient = ntohs(adresse_arrivee.sin_port);
      printf("Adresse = %s\n",ipClient);
      printf("Port = %i\n",portClient);

      //initialisation de connexion (reception SYN)
      if(strcmp(recvBuffer, "SYN") == 0) {
        //FORK : La suite doit être gérée par un autre processus
        //Calcul valeur port
        portIncrement = (portIncrement + 1);
        if (portIncrement > nbPorts) portIncrement = 0;
        portData = atoi(argv[2]) + portIncrement;

        id = fork();
        if(id<0)
        {
          perror("[!] Fork failed");
          return -1;
        }
        printf("id : %i\n",id);
        if (id == 0) //PRocessus fils :
        {
          printf("PortData : %i\n",portData);
          //Création du socket data propre au processus fils

          //Définition socket UDP de data
          int socketServUDP_data;
          if( (socketServUDP_data = socket(DOMAINE,TYPE,PROTOCOL)) == -1)
          {
            perror("Creation de socket impossible\n");
            return -1;
          }
          printf("Descripteur UDP : %i\n",socketServUDP_data);

          adresseUDP_data.sin_port = htons(portData); //Port du serveur, converti en valeur réseau
          //Lien entre les deux :
          bind(socketServUDP_data,(struct sockaddr*)&adresseUDP_data,sizeof(adresseUDP_data));
          //Ecoute :
          listen(socketServUDP_data,1);

          //Envoi du SYNACK
          unreceived = 1;
          memset(sendBuffer, 0, sizeof(sendBuffer));
          sprintf(sendBuffer,"SYN-ACK%i",portData);
          while(unreceived){
            printf("Send Synack.\n");
            sendto(socketServUDP, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_arrivee, taille_arrivee);
            //Message envoyé ! On initialise un timer pour estimer le RTT
            //timer = clock();
            //Attente pour le ACK
            FD_SET(socketServUDP, &socket_set); //Activation du bit associé à au socket UDP de CONNEXION
            timeout.tv_usec = timeToWait;
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
          //RoundTripTime = (double)(clock() - timer)/CLOCKS_PER_SEC;
          //printf("RTT : %f\n",RoundTripTime);
          //Choix de la valeur du timeout
          //timeToWait = round(1000000 * RoundTripTime * ALPHA); //Facteur arbitraire, en microsecondes
          //printf("Timeout (microseconds) : %f\n",timeToWait);
          //timeout.tv_sec = 0;
          //timeout.tv_usec = timeToWait; //Initialisation du timer (à répéter avant chaque select)

          //PROCESSUS DE TRANSFERT DE FICHIER : fork
          FD_CLR(socketServUDP, &socket_set); //Desactivation du bit associé à au socket UDP de CONNEXION
          FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
          recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
          if(strcmp(recvBuffer,"") != 0) printf("Reçu UDP (port data) : %s\n",recvBuffer);
          //recuperation de l'adresse
          ipClient_data = inet_ntoa(adresse_data.sin_addr);
          portClient_data = ntohs(adresse_data.sin_port);
          printf("Adresse = %s\n",ipClient_data);
          printf("Port = %i\n",portClient_data);
          //beginWindow = rand()/10000;
          beginWindow = 1;
          endWindow = 1;
          //verification qu'il ne s'agisse pas d'un ACK perdu
          while (strstr(recvBuffer, "ACK") != NULL)
          {
            //SI ACK PERDU : RECEPTION A NOUVEAU
            printf("[-] Lost Ack\n");
            FD_CLR(socketServUDP, &socket_set); //Desactivation du bit associé à au socket UDP de CONNEXION
            FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
            recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
            if(strcmp(recvBuffer,"") != 0) printf("Reçu UDP (port data) : %s\n",recvBuffer);
            //recuperation de l'adresse
            ipClient_data = inet_ntoa(adresse_data.sin_addr);
            portClient_data = ntohs(adresse_data.sin_port);
            printf("Adresse = %s\n",ipClient_data);
            printf("Port = %i\n",portClient_data);
          }

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
          timeout.tv_sec = 0;
          while(beginWindow <= taillefichier){
            printf("\n");
            nWrongAcks = 0;
            //Congestion Avoidance : on doit incrémenter windowSize à chaque RTT.
            //On considère qu'une boucle ~ = un RTT.
            if (windowSize >= ssthresh) windowSize++;
            endWindow = beginWindow + windowSize;
            if (endWindow > taillefichier){
              endWindow = taillefichier;
              windowSize = endWindow - beginWindow + 1;
            }
            //Boucle de transmission
            for(int j = beginWindow; j<=endWindow; j++){
              i = (BUFFER_TAILLE - 6) * (j-1); //segment de sequence n = début à la position n-1
              //Construction du message
              sprintf(typeBuffer,"000000"); //Base du numéro de séquence
              sprintf(strSequence,"%i",j); //Numéro de séquence en str
              char *ptr = typeBuffer + (6-strlen(strSequence)); //Pointeur du début d'écriture pour le numéro de séquence
              memcpy(ptr,strSequence,strlen(strSequence)); //Ecriture au ponteur
              printf("[0] Sending number %s\n",typeBuffer);
              memset(sendBuffer, 0, sizeof(sendBuffer));
              memset(recvBuffer, 0, sizeof(recvBuffer));
              // sprintf(sendBuffer,"%s%s",typeBuffer,bloc); //formation du message à envoyer
              //La ligne précédente utilise sprintf. Cela posera problème si l'on a un caractère \0 .
              memcpy(sendBuffer,typeBuffer,6);
              if (j == taillefichier) memcpy(sendBuffer+6,file+i,restefichier); //Si moins de BUFFER_TAILLE - 6 à envoyer
              else memcpy(sendBuffer+6,file+i,BUFFER_TAILLE-6);
              //Envoi du message
              if (!measurement){
                timer = clock();
                measurement = j;
              }
              if (j == taillefichier) sendto(socketServUDP_data, sendBuffer, restefichier + 6, 0, (const struct sockaddr *) &adresse_data, taille_data);
              else sendto(socketServUDP_data, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_data, taille_data);
              //printf("[o] Sended : \n%s\n\n",sendBuffer);
              //printf("%i\n",a);
            }

            //Boucle de réception
            problem = 0;
            while((!problem) && ((beginWindow!=endWindow)||(beginWindow == taillefichier))){
              FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
              timeout.tv_usec = timeToWait; //Initialisation du timer
              //printf("timeout usec : %li\n",timeout.tv_usec);
              //printf("timeout sec : %li\n",timeout.tv_sec);
              selret = select(5,&socket_set,NULL,NULL,&timeout);
              if (selret<0)
              {
                perror("[!] Select Error.\n");
                return -1;
              }
              else if (selret == 0) {
                //mise à jour du ssthresh
                ssthresh = max(windowSize / 2,INITIAL_WINDOW);
                windowSize = INITIAL_WINDOW;
                problem = 1;
                nWrongAcks = 0;
                printf("[-] Timeout.\n");
                //Augmentation du srtt avec les timeout
                srtt = minfloat(1.3*srtt,0.005);
              }
              else{
                //Vérification nature du message reçu
                recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
                receivedAck = get_numSequence(recvBuffer,typeBuffer);
                if(receivedAck < beginWindow) {
                  //Erreur : mauvais num sequence
                  //Si = au wrongack précédent, on ignore simplement
                  nWrongAcks++;
                  if (nWrongAcks==3){
                    problem = 1;
                    //mise à jour du ssthresh
                    ssthresh = max(windowSize / 2,INITIAL_WINDOW);
                    windowSize = ssthresh; //FAST Recovery
                    printf("[-] 3 wrong acks.\n");
                    //printf("attendu (ou +) : %i\n",beginWindow);
                    //printf("reçu : %i\n",receivedAck);
                    //On vide tous les messages à recevoir
                    timeout.tv_usec = 100; //Initialisation du timer
                    FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
                    while(select(5,&socket_set,NULL,NULL,&timeout) != 0){
                      FD_SET(socketServUDP_data, &socket_set); //Activation du bit associé à au socket UDP de DATA
                      recvfrom(socketServUDP_data, recvBuffer, BUFFER_TAILLE, 0,(struct sockaddr*)&adresse_data, &taille_data);
                      timeout.tv_usec = 100; //Initialisation du timer
                    }
                  }
                }
                else{
                  //message bien reçu et acknowlegded
                  printf("[+] Ack number %i received.\n",receivedAck);
                  nWrongAcks = 0;
                  beginWindow = receivedAck + 1;
                  //printf("numSequence du client : %i\n",beginWindow);
                  if (windowSize<ssthresh) windowSize++;
                  if (windowSize == ssthresh) printf("[+] Congestion Avoidance.\n");
                  if (measurement < beginWindow) { //Paquet mesuré reçu !
                    TempSRTT = (double)(clock() - timer)/CLOCKS_PER_SEC;
                    srtt = ALPHASRTT*srtt + (1-ALPHASRTT)*TempSRTT;
                    timeToWait = 1000000*ALPHA*srtt;
                    //printf("RTT : %f\n",TempSRTT);
                    //printf("SRTT : %f\n",srtt);
                    //printf("timeout : %li\n",timeToWait);
                    measurement = 0;
                  }
                }
              }
            }
          }

          //Fin du fichier atteint : procédure end
          printf("end of file\n");
          printf("timeToWait : %li\n",timeToWait);
          memset(sendBuffer, 0, sizeof(sendBuffer));
          sprintf(sendBuffer,"FIN");
          sendto(socketServUDP_data, sendBuffer, BUFFER_TAILLE, 0, (const struct sockaddr *) &adresse_data, taille_data);
          free(file); // On libère l'espace attribué pour la lecture

          close(socketServUDP_data);
          online = 0; //Arrêt du serveur (fork)

        }
      }
    }
  }
  printf("[!]END OF FORK\n");
  return 0;
}
