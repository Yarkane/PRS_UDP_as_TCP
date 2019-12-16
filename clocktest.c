/* According to POSIX.1-2001, POSIX.1-2008 */
#include <sys/select.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

int main(){
  //def timeout
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 52485;
  //Descripteur
  fd_set socket_set;
  FD_ZERO(&socket_set);
  //select
  printf("Démarrage select.\n");
  select(0,&socket_set,NULL,NULL,&timeout);
  printf("Fin select.\n");
  return 1;
}
