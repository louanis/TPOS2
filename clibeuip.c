#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 9998
#define LBUF 512

int main(int argc, char *argv[]) {

    if(argc < 2) {
        printf("Usage:\n");
        printf("%s liste\n",argv[0]);
        printf("%s mess <pseudo|all> <message>\n",argv[0]);
        return 1;
    }

    int sid;

    struct sockaddr_in Sock;
    // socklen_t ls = sizeof(Sock);

    if((sid = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        perror("socket");
        return 2;
    }

    Sock.sin_family = AF_INET;
    Sock.sin_port = htons(PORT);
    Sock.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buf[LBUF];

    if(strcmp(argv[1],"liste")==0){

        buf[0]='3';
        strcpy(buf+1,"BEUIP");

        sendto(sid,buf,6,0,(struct sockaddr*)&Sock,sizeof(Sock));

        printf("Commande liste envoyee\n");
    }

    else if(strcmp(argv[1],"mess")==0 && argc>=4){

        char message[400];
        strcpy(message,argv[3]);

        for(int i=4;i<argc;i++){
            strcat(message," ");
            strcat(message,argv[i]);
        }

        if(strcmp(argv[2],"all")==0){

            buf[0]='5';
            strcpy(buf+1,"BEUIP");
            strcpy(buf+6,message);

            sendto(sid,buf,6+strlen(message)+1,0,(struct sockaddr*)&Sock,sizeof(Sock));

            printf("Message global envoye\n");
        }

        else{

            buf[0]='4';
            strcpy(buf+1,"BEUIP");

            strcpy(buf+6,argv[2]);

            int pos = 6 + strlen(argv[2]) + 1;

            strcpy(buf+pos,message);

            sendto(sid,buf,pos+strlen(message)+1,0,(struct sockaddr*)&Sock,sizeof(Sock));

            printf("Message envoye a %s\n",argv[2]);
        }
    }

    close(sid);

    return 0;
}