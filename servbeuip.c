#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 9998
#define MAX_USERS 255
#define LBUF 512

typedef struct {
    char pseudo[64];
    struct in_addr ip;
} User;

User table[MAX_USERS];
int nb_users = 0;

int sockfd_global;
char mon_pseudo_global[64];

void handle_sigterm(int sig) {
    char buffer[512];
    buffer[0] = '0';
    strcpy(buffer + 1, "BEUIP");
    strcpy(buffer + 6, mon_pseudo_global);
    int len = 6 + strlen(mon_pseudo_global) + 1;
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    // Adresse broadcast du TP
    servaddr.sin_addr.s_addr = inet_addr("192.168.88.255"); 
    
    sendto(sockfd_global, buffer, len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("\n[Serveur] Arrêt demandé. Message de déconnexion envoyé.\n");
    close(sockfd_global);
    exit(EXIT_SUCCESS);
}

void ajouter_utilisateur(const char* pseudo, struct in_addr ip) {
    for (int i = 0; i < nb_users; i++) {
        if (strcmp(table[i].pseudo, pseudo) == 0) {
            table[i].ip = ip;
            return;
        }
        if (table[i].ip.s_addr == ip.s_addr) {
            strcpy(table[i].pseudo, pseudo);
            return;
    }
    if (nb_users < MAX_USERS) {
        strcpy(table[nb_users].pseudo, pseudo);
        table[nb_users].ip = ip;
        nb_users++;
        printf("[+] Nouvel utilisateur ajouté : %s (%s)\n", pseudo, inet_ntoa(ip));
    }
}
void supprimer_utilisateur(struct in_addr ip) {
    for (int i = 0; i < nb_users; i++) {
        if (table[i].ip.s_addr == ip.s_addr) {
            printf("[-] Utilisateur déconnecté : %s\n", table[i].pseudo);
            table[i] = table[nb_users - 1];
            nb_users--;
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pseudo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *mon_pseudo = argv[1];

    strcpy(mon_pseudo_global, mon_pseudo);
    signal(SIGTERM, handle_sigterm);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }
    sockfd_global = sockfd;

    int broadcastEnable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erreur bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Serveur BEUIP démarré sur le port %d pour %s.\n", PORT, mon_pseudo);

    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("192.168.88.255"); // IP broadcast

    char msg_bcast[LBUF];
    sprintf(msg_bcast, "1BEUIP%s", mon_pseudo);
    sendto(sockfd, msg_bcast, strlen(msg_bcast) + 1, 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    char buffer[LBUF];

    while (1) {
        int n = recvfrom(sockfd, buffer, LBUF, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 7) continue; 
        if (strncmp(buffer + 1, "BEUIP", 5) != 0) continue;

        char code = buffer[0];
        char *data = buffer + 6; 
        int is_local = (cliaddr.sin_addr.s_addr == inet_addr("127.0.0.1"));

        switch (code) {
            case '1': 
                ajouter_utilisateur(data, cliaddr.sin_addr);
                char msg_ar[LBUF];
                sprintf(msg_ar, "2BEUIP%s", mon_pseudo);
                sendto(sockfd, msg_ar, strlen(msg_ar) + 1, MSG_CONFIRM, (struct sockaddr *)&cliaddr, len);
                break;
            case '2':
                ajouter_utilisateur(data, cliaddr.sin_addr);
                break;
            case '3': 
                if (is_local) {
                    printf("\n--- Liste des %d utilisateurs présents ---\n", nb_users);
                    for (int i = 0; i < nb_users; i++) {
                        printf("- %s (%s)\n", table[i].pseudo, inet_ntoa(table[i].ip));
                    }
                    printf("------------------------------------------\n");
                }
                break;
            case '4': 
                if (is_local) {
                    char *cible_pseudo = data;
                    char *message = data + strlen(cible_pseudo) + 1; 
                    for (int i = 0; i < nb_users; i++) {
                        if (strcmp(table[i].pseudo, cible_pseudo) == 0) {
                            struct sockaddr_in dest;
                            dest.sin_family = AF_INET;
                            dest.sin_port = htons(PORT);
                            dest.sin_addr = table[i].ip;
                            char msg_env[LBUF];
                            sprintf(msg_env, "9BEUIP%s", message);
                            sendto(sockfd, msg_env, strlen(msg_env) + 1, 0, (struct sockaddr *)&dest, sizeof(dest));
                            break;
                        }
                    }
                }
                break;
            case '5': 
                if (is_local) {
                    char msg_env[LBUF];
                    sprintf(msg_env, "9BEUIP%s", data); 
                    for (int i = 0; i < nb_users; i++) {
                        struct sockaddr_in dest;
                        dest.sin_family = AF_INET;
                        dest.sin_port = htons(PORT);
                        dest.sin_addr = table[i].ip;
                        sendto(sockfd, msg_env, strlen(msg_env) + 1, 0, (struct sockaddr *)&dest, sizeof(dest));
                    }
                }
                break;
            case '9': 
                {
                    char exp_pseudo[64] = "Inconnu";
                    for (int i = 0; i < nb_users; i++) {
                        if (table[i].ip.s_addr == cliaddr.sin_addr.s_addr) {
                            strcpy(exp_pseudo, table[i].pseudo);
                            break;
                        }
                    }
                    if (strcmp(exp_pseudo, "Inconnu") == 0) {
                         printf("Erreur : Adresse IP %s non trouvée.\n", inet_ntoa(cliaddr.sin_addr));
                    } else {
                         printf("\n[Message de %s] : %s\n", exp_pseudo, data);
                    }
                }
                break;
            case '0': 
                supprimer_utilisateur(cliaddr.sin_addr);
                break;
        }
    }
    return 0;
}