/*****
 * Serveur BEUIP complet
 *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 9998
#define LBUF 512
#define MAX_USERS 255

struct user {
    char pseudo[64];
    struct sockaddr_in sock;
    unsigned long ip;
};

struct user table[MAX_USERS];
int nb_users = 0;

int sid_global;
char pseudo_global[64];

void add_user(const char *pseudo, struct sockaddr_in *Sock) {
    unsigned long ip = Sock->sin_addr.s_addr;
    for(int i=0;i<nb_users;i++) {
        if(table[i].ip == ip) return;
    }

    if(nb_users < MAX_USERS) {
        strncpy(table[nb_users].pseudo, pseudo, 63);
        table[nb_users].pseudo[63]='\0';
        table[nb_users].sock = *Sock;
        table[nb_users].ip = ip;
        nb_users++;
    }
}

void remove_user(unsigned long ip) {
    for(int i=0;i<nb_users;i++){
        if(table[i].ip == ip){
            for(int j=i;j<nb_users-1;j++)
                table[j]=table[j+1];
            nb_users--;
            return;
        }
    }
}

void print_users() {
    printf("---- LISTE UTILISATEURS ----\n");
    for(int i=0;i<nb_users;i++)
        printf("%s\n", table[i].pseudo);
    printf("----------------------------\n");
}

void send_broadcast(int sid, const char *pseudo) {
    int yes=1;
    setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    struct sockaddr_in bcast;
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(PORT);
    bcast.sin_addr.s_addr = inet_addr("192.168.88.255");

    char msg[LBUF];
    msg[0]='1';
    strcpy(msg+1,"BEUIP");
    strcpy(msg+6,pseudo);

    sendto(sid,msg,6+strlen(pseudo)+1,0,(struct sockaddr*)&bcast,sizeof(bcast));
}

void send_depart() {
    int yes=1;
    setsockopt(sid_global,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));

    struct sockaddr_in bcast;
    bcast.sin_family=AF_INET;
    bcast.sin_port=htons(PORT);
    bcast.sin_addr.s_addr=inet_addr("192.168.88.255");

    char msg[LBUF];
    msg[0]='0';
    strcpy(msg+1,"BEUIP");
    strcpy(msg+6,pseudo_global);

    sendto(sid_global,msg,6+strlen(pseudo_global)+1,0,(struct sockaddr*)&bcast,sizeof(bcast));
}

void sigterm_handler(int sig) {
    send_depart();
    close(sid_global);
    exit(0);
}

int main(int argc, char* argv[]) {

    if(argc != 2) {
        fprintf(stderr,"Usage : %s <pseudo>\n", argv[0]);
        return 1;
    }

    strcpy(pseudo_global, argv[1]);

    struct sockaddr_in Sock;
    socklen_t ls = sizeof(Sock);

    if((sid_global = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return 2;
    }

    signal(SIGTERM, sigterm_handler);

    Sock.sin_family = AF_INET;
    Sock.sin_port = htons(PORT);
    Sock.sin_addr.s_addr = INADDR_ANY;

    if(bind(sid_global,(struct sockaddr*)&Sock,sizeof(Sock))==-1) {
        perror("bind");
        return 3;
    }

    add_user(pseudo_global,&Sock);

    printf("Serveur BEUIP actif sur port %d\n",PORT);

    send_broadcast(sid_global,pseudo_global);

    char buf[LBUF];

    for(;;){

        int n = recvfrom(sid_global,buf,LBUF,0,(struct sockaddr*)&Sock,&ls);
        if(n<0) continue;

        buf[n]='\0';

        if(n<6) continue;

        char code = buf[0];

        if(strncmp(buf+1,"BEUIP",5)!=0) continue;

        switch(code){

            case '1':
                add_user(buf+6,&Sock);

                {
                    char msg[LBUF];
                    msg[0]='2';
                    strcpy(msg+1,"BEUIP");
                    strcpy(msg+6,pseudo_global);

                    sendto(sid_global,msg,6+strlen(pseudo_global)+1,0,(struct sockaddr*)&Sock,ls);
                }
                break;

            case '2':
                printf("AR recu de %s\n",buf+6);
                break;

            case '3':
                if(Sock.sin_addr.s_addr == inet_addr("127.0.0.1"))
                    print_users();
                break;

            case '4':
                if(Sock.sin_addr.s_addr != inet_addr("127.0.0.1")) break;

                {
                    char *ptr = buf+6;

                    char pseudo_dest[64];
                    char message[448];

                    strncpy(pseudo_dest,ptr,63);
                    pseudo_dest[63]='\0';

                    ptr += strlen(pseudo_dest)+1;

                    strncpy(message,ptr,447);
                    message[447]='\0';

                    int found=0;

                    for(int i=0;i<nb_users;i++){

                        if(strcmp(table[i].pseudo,pseudo_dest)==0){

                            char m[LBUF];
                            m[0]='9';
                            strcpy(m+1,message);

                            sendto(sid_global,m,1+strlen(message)+1,0,
                            (struct sockaddr*)&table[i].sock,sizeof(table[i].sock));

                            found=1;
                            break;
                        }
                    }

                    if(!found)
                        printf("Pseudo %s introuvable !\n",pseudo_dest);
                }
                break;

            case '5':

                if(Sock.sin_addr.s_addr != inet_addr("127.0.0.1")) break;

                {
                    char m[LBUF];

                    m[0]='9';
                    strcpy(m+1,buf+6);

                    for(int i=0;i<nb_users;i++){

                        if(table[i].ip != Sock.sin_addr.s_addr)

                            sendto(sid_global,m,1+strlen(buf+6)+1,0,
                            (struct sockaddr*)&table[i].sock,sizeof(table[i].sock));
                    }
                }
                break;

            case '9':

                {
                    char *message = buf+1;
                    char *sender = NULL;

                    for(int i=0;i<nb_users;i++){

                        if(table[i].ip == Sock.sin_addr.s_addr){

                            sender = table[i].pseudo;
                            break;
                        }
                    }

                    if(sender)
                        printf("Message de %s : %s\n",sender,message);
                    else
                        printf("Message inconnu : %s\n",message);
                }
                break;

            case '0':
                remove_user(Sock.sin_addr.s_addr);
                printf("Utilisateur parti\n");
                break;
        }
    }

    close(sid_global);

    return 0;
}