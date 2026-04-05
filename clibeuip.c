#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9998
#define LBUF 512

void afficher_usage(char *nom_prog) {
    printf("Usage:\n");
    printf("  %s liste\n", nom_prog);
    printf("  %s msg <pseudo_cible> <\"message\">\n", nom_prog);
    printf("  %s bcast <\"message\">\n", nom_prog);
    printf("  %s quit <mon_pseudo>\n", nom_prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        afficher_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // creatrion du socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    // config du serv local
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    char buffer[LBUF];
    int len = 0;

    // --- Commande "liste" ---
    if (strcmp(argv[1], "liste") == 0) {
        sprintf(buffer, "3BEUIP");
        len = 6;
    } 
    // --- Commande "message à un pseudo" ---
    else if (strcmp(argv[1], "msg") == 0 && argc == 4) {
        buffer[0] = '4';
        strcpy(buffer + 1, "BEUIP");
        
        char *pseudo_cible = argv[2];
        char *message = argv[3];
        
        strcpy(buffer + 6, pseudo_cible);
        int offset = 6 + strlen(pseudo_cible) + 1; 
        
        strcpy(buffer + offset, message);
        len = offset + strlen(message) + 1;
    } 
    // --- Commande "mess all" ---
    else if (strcmp(argv[1], "bcast") == 0 && argc == 3) {
        buffer[0] = '5';
        strcpy(buffer + 1, "BEUIP");
        strcpy(buffer + 6, argv[2]);
        len = 6 + strlen(argv[2]) + 1;
    } 
    // --- Commande "quitter" ---
    else if (strcmp(argv[1], "quit") == 0 && argc == 3) {
        buffer[0] = '0';
        strcpy(buffer + 1, "BEUIP");
        strcpy(buffer + 6, argv[2]);
        len = 6 + strlen(argv[2]) + 1;
        
        int broadcastEnable = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        servaddr.sin_addr.s_addr = inet_addr("192.168.88.255"); 
    } 
    else {
        afficher_usage(argv[0]);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Envoi du message
    sendto(sockfd, buffer, len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("Commande envoyée au serveur.\n");

    close(sockfd);
    return 0;
}