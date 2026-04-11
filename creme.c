#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "creme.h"
#define PORT 9998
#define BCAST_ADDR "192.168.88.255"
#define LBUF 512
#define LPSEUDO 23

char mon_pseudo_global[64];
char mon_ip_globale[16] = "127.0.0.1"; // Valeur par défaut

/* --- Structure de la liste chainee --- */
struct elt {
    char nom[LPSEUDO + 1];
    char adip[16];
    struct elt *next;
};

/* --- Variables globales partagees --- */
struct elt *liste = NULL;
pthread_mutex_t mutex_liste = PTHREAD_MUTEX_INITIALIZER;
char mon_pseudo_global[64];
int serveur_actif = 0;
int sockfd_udp = -1;
int sockfd_tcp = -1;
pthread_t tid_udp, tid_tcp;

/* ========================================================= */
/* GESTION DE LA LISTE                     */
/* ========================================================= */

void ajouteElt(char *pseudo, char *adip) {
    if (strcmp(pseudo, mon_pseudo_global) == 0) return; // Ne pas s'ajouter

    pthread_mutex_lock(&mutex_liste);
    
    struct elt *nouveau = malloc(sizeof(struct elt));
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0';
    strcpy(nouveau->adip, adip);
    nouveau->next = NULL;

    // ordred alphabnetique
    if (liste == NULL || strcmp(pseudo, liste->nom) < 0) {
        nouveau->next = liste;
        liste = nouveau;
    } else {
        struct elt *cur = liste;
        while (cur->next && strcmp(cur->next->nom, pseudo) < 0) {
            cur = cur->next;
        }
        if (strcmp(cur->nom, pseudo) == 0) {
            free(nouveau); // Existe deja
            pthread_mutex_unlock(&mutex_liste);
            return;
        }
        nouveau->next = cur->next;
        cur->next = nouveau;
    }
#ifdef TRACE
    printf("\n[TRACE] Utilisateur ajoute: %s (%s)\n", pseudo, adip);
#endif
    pthread_mutex_unlock(&mutex_liste);
}

void supprimeElt(char *adip) {
    pthread_mutex_lock(&mutex_liste);
    struct elt *cur = liste, *prev = NULL;
    while (cur) {
        if (strcmp(cur->adip, adip) == 0) {
            if (prev == NULL) liste = cur->next;
            else prev->next = cur->next;
#ifdef TRACE
            printf("\n[TRACE] Utilisateur supprime: %s\n", cur->nom);
#endif
            free(cur);
            pthread_mutex_unlock(&mutex_liste);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&mutex_liste);
}

void listeElts(void) {
    pthread_mutex_lock(&mutex_liste);
    
    // On s'affiche soi-même
    printf("%s : %s\n", mon_ip_globale, mon_pseudo_global);

    // On affiche tous les autres
    struct elt *cur = liste;
    while (cur) {
        printf("%s : %s\n", cur->adip, cur->nom);
        cur = cur->next;
    }
    
    pthread_mutex_unlock(&mutex_liste);
}
/* ========================================================= */
/* BROADCAST DYNAMIQUE                       */
/* ========================================================= */

void envoyer_broadcast(char octet1, const char *pseudo) {
    char msg[LBUF];
    sprintf(msg, "%cBEUIP%s", octet1, pseudo);
    int len = 6 + strlen(pseudo) + 1;

    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr(BCAST_ADDR); // Utilisation de votre #define
    
    sendto(sockfd_udp, msg, len, 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
}

/* ========================================================= */
/* THREAD SERVEUR UDP                       */
/* ========================================================= */

void *serveur_udp(void *p) {
    (void)p;
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t len = sizeof(cliaddr);
    char buffer[LBUF];

    while (1) {
        int n = recvfrom(sockfd_udp, buffer, LBUF, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 7 || strncmp(buffer + 1, "BEUIP", 5) != 0) continue;
        buffer[n] = '\0';
        char code = buffer[0];
        char *data = buffer + 6;
        char ip[16];
        inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));

        // if (strcmp(ip, "127.0.0.1") == 0) continue; // ignorer nos messages

        switch (code) {
            case '1':
                ajouteElt(data, ip);
                char msg_ar[LBUF];
                sprintf(msg_ar, "2BEUIP%s", mon_pseudo_global);
                sendto(sockfd_udp, msg_ar, strlen(msg_ar) + 1, 0, (struct sockaddr *)&cliaddr, len);
                break;
            case '2':
                ajouteElt(data, ip);
                break;
            case '9':
                {
                    char exp_pseudo[LPSEUDO+1] = "Inconnu";
                    pthread_mutex_lock(&mutex_liste);
                    struct elt *cur = liste;
                    while (cur) {
                        if (strcmp(cur->adip, ip) == 0) {
                            strcpy(exp_pseudo, cur->nom);
                            break;
                        }
                        cur = cur->next;
                    }
                    pthread_mutex_unlock(&mutex_liste);
                    printf("\n\a[Message de %s] : %s\n", exp_pseudo, data);
                }
                break;
            case '0':
                supprimeElt(ip);
                break;
        }
    }
    return NULL;
}

/* ========================================================= */
/* THREAD SERVEUR TCP                       */
/* ========================================================= */

void envoiContenu(int fd) {
    char req;
    if (read(fd, &req, 1) <= 0) return;

    if (req == 'L') {
        if (fork() == 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            execlp("ls", "ls", "-l", "reppub/", NULL);
            exit(1);
        }
        wait(NULL);
    } else if (req == 'F') {
        char nomfic[256];
        int i = 0;
        char c;
        while (read(fd, &c, 1) > 0 && c != '\n' && i < 255) {
            nomfic[i++] = c;
        }
        nomfic[i] = '\0';
        char path[512];
        snprintf(path, sizeof(path), "reppub/%s", nomfic);
        
        if (fork() == 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            execlp("cat", "cat", path, NULL);
            exit(1);
        }
        wait(NULL);
    }
}

void *serveur_tcp(void *rep) {
    (void)rep;
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t len = sizeof(cliaddr);

    while (1) {
        int client_fd = accept(sockfd_tcp, (struct sockaddr *)&cliaddr, &len);
        if (client_fd >= 0) {
            envoiContenu(client_fd);
            close(client_fd);
        }
    }
    return NULL;
}

/* ========================================================= */
/* COMMANDES INTERNES (API)                    */
/* ========================================================= */

int beuip_start(const char *pseudo) {
    if (serveur_actif) {
        printf("Erreur : Le serveur tourne deja.\n");
        return -1;
    }
    strcpy(mon_pseudo_global, pseudo);

    // creation repertoire public
    mkdir("reppub", 0755);

    // config UDP
    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(sockfd_udp, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd_udp, (struct sockaddr *)&addr, sizeof(addr));

    // config TCP
    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(sockfd_tcp, (struct sockaddr *)&addr, sizeof(addr));
    listen(sockfd_tcp, 5);

    // Récupération de adresse IP sur le réseau
    int sock_test = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in test_addr;
    memset(&test_addr, 0, sizeof(test_addr));
    test_addr.sin_family = AF_INET;
    test_addr.sin_port = htons(53); // Port DNS (au hasard)
    
    // On simule une route vers l'extérieur pour forcer le choix de la carte réseau
    test_addr.sin_addr.s_addr = inet_addr("8.8.8.8"); 
    
    connect(sock_test, (struct sockaddr *)&test_addr, sizeof(test_addr));
    
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    socklen_t my_len = sizeof(my_addr);
    getsockname(sock_test, (struct sockaddr *)&my_addr, &my_len);
    inet_ntop(AF_INET, &my_addr.sin_addr, mon_ip_globale, sizeof(mon_ip_globale));
    close(sock_test);

    // lancement threads
    pthread_create(&tid_udp, NULL, serveur_udp, NULL);
    pthread_create(&tid_tcp, NULL, serveur_tcp, "reppub");
    serveur_actif = 1;

    // annonce au reseau
    envoyer_broadcast('1', mon_pseudo_global);

#ifdef TRACE
    printf("[TRACE] Serveurs UDP et TCP demarres (Multithreading).\n");
#endif
    return 0;
}

int beuip_stop(void) {
    if (!serveur_actif) return -1;
    
    envoyer_broadcast('0', mon_pseudo_global);
    
    pthread_cancel(tid_udp);
    pthread_cancel(tid_tcp);

    pthread_join(tid_udp, NULL);
    pthread_join(tid_tcp, NULL);

    close(sockfd_udp);
    close(sockfd_tcp);
    
    // nettoyage liste
    pthread_mutex_lock(&mutex_liste);
    struct elt *cur = liste;
    while (cur) {
        struct elt *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    liste = NULL;
    pthread_mutex_unlock(&mutex_liste);

    serveur_actif = 0;
#ifdef TRACE
    printf("[TRACE] Serveurs arretes et memoire liberee.\n");
#endif
    return 0;
}

void commande(char octet1, char *message, char *pseudo) {
    if (!serveur_actif) {
        printf("Erreur : Lancez 'beuip start' d'abord.\n");
        return;
    }

    if (octet1 == '3') {
        listeElts();
    } 
    else if (octet1 == '4' && pseudo && message) {
        pthread_mutex_lock(&mutex_liste);
        struct elt *cur = liste;
        char ip_cible[16] = "";
        while (cur) {
            if (strcmp(cur->nom, pseudo) == 0) {
                strcpy(ip_cible, cur->adip);
                break;
            }
            cur = cur->next;
        }
        pthread_mutex_unlock(&mutex_liste);

        if (strlen(ip_cible) > 0) {
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            dest.sin_addr.s_addr = inet_addr(ip_cible);
            
            char msg_env[LBUF];
            sprintf(msg_env, "9BEUIP%s", message);
            sendto(sockfd_udp, msg_env, strlen(msg_env) + 1, 0, (struct sockaddr *)&dest, sizeof(dest));
#ifdef TRACE
            printf("[TRACE] Message envoye a %s (%s)\n", pseudo, ip_cible);
#endif
        } else {
            printf("Erreur : Utilisateur %s introuvable.\n", pseudo);
        }
    } 
    else if (octet1 == '5' && message) {
        char msg_env[LBUF];
        sprintf(msg_env, "9BEUIP%s", message);
        int len = strlen(msg_env) + 1;

        pthread_mutex_lock(&mutex_liste);
        struct elt *cur = liste;
        while (cur) {
            struct sockaddr_in dest;
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            dest.sin_addr.s_addr = inet_addr(cur->adip);
            sendto(sockfd_udp, msg_env, len, 0, (struct sockaddr *)&dest, sizeof(dest));
            cur = cur->next;
        }
        pthread_mutex_unlock(&mutex_liste);
#ifdef TRACE
        printf("[TRACE] Broadcast applicatif envoye.\n");
#endif
    }
}

void demandeListe(char *pseudo) {
    pthread_mutex_lock(&mutex_liste);
    struct elt *cur = liste;
    char ip_cible[16] = "";
    while (cur) {
        if (strcmp(cur->nom, pseudo) == 0) {
            strcpy(ip_cible, cur->adip);
            break;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&mutex_liste);

    if (strlen(ip_cible) == 0) {
        printf("Erreur : Utilisateur %s introuvable.\n", pseudo);
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(ip_cible);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur connexion TCP");
        close(sock);
        return;
    }

    write(sock, "L", 1);
    char buffer[512];
    int n;
    printf("--- Fichiers publics de %s ---\n", pseudo);
    while ((n = read(sock, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, n);
    }
    close(sock);
}

void demandeFichier(char *pseudo, char *nomfic) {
    pthread_mutex_lock(&mutex_liste);
    struct elt *cur = liste;
    char ip_cible[16] = "";
    while (cur) {
        if (strcmp(cur->nom, pseudo) == 0) {
            strcpy(ip_cible, cur->adip);
            break;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&mutex_liste);

    if (strlen(ip_cible) == 0) {
        printf("Erreur : Utilisateur %s introuvable.\n", pseudo);
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(ip_cible);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur connexion TCP");
        close(sock);
        return;
    }

    char req[512];
    snprintf(req, sizeof(req), "F%s\n", nomfic);
    write(sock, req, strlen(req));

    char path_local[512];
    snprintf(path_local, sizeof(path_local), "reppub/%s", nomfic);
    
    // verifier si le fichier existe deja localement
    if (access(path_local, F_OK) == 0) {
        printf("Attention : Le fichier %s existe deja dans reppub/. Ecrasement evite.\n", nomfic);
        close(sock);
        return;
    }

    int fd_out = open(path_local, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("Erreur creation fichier local");
        close(sock);
        return;
    }

    char buffer[512];
    int n;
    int total = 0;
    while ((n = read(sock, buffer, sizeof(buffer))) > 0) {
        write(fd_out, buffer, n);
        total += n;
    }
    close(fd_out);
    close(sock);
    printf("Fichier '%s' telecharge avec succes (%d octets) dans reppub/.\n", nomfic, total);
}