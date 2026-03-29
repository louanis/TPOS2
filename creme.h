#ifndef CREME_H
#define CREME_H

#include <sys/types.h>

#define PORT 9998
#define LBUF 512

typedef struct {
    pid_t pid_server;
    char pseudo[64];
} BEUIP_Server;

/* gestion du serveur */
int beuip_start(BEUIP_Server *srv, const char *pseudo);
int beuip_stop(BEUIP_Server *srv);

/* commandes mess */
int beuip_liste(void);
int beuip_send_to(const char *pseudo, const char *message);
int beuip_send_all(const char *message);

#endif