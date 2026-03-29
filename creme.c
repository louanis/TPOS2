#include "creme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/wait.h>

static int sock_client = -1;

static int init_client(void)
{
    if (sock_client >= 0)
        return sock_client;

    sock_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_client < 0)
    {
        perror("socket");
        return -1;
    }

    return sock_client;
}

/* ===================== */
/*   lancement serveur   */
/* ===================== */

int beuip_start(BEUIP_Server *srv, const char *pseudo)
{
    if (!srv || !pseudo)
        return -1;

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        execl("./servbeuip", "./servbeuip", pseudo, NULL);
        perror("execl servbeuip");
        _exit(EXIT_FAILURE);
    }

    srv->pid_server = pid;
    strncpy(srv->pseudo, pseudo, 63);
    srv->pseudo[63] = '\0';

    sleep(1);

    return 0;
}

/* ===================== */
/*    arret serveur      */
/* ===================== */

int beuip_stop(BEUIP_Server *srv)
{
    if (!srv || srv->pid_server <= 0)
        return -1;

    kill(srv->pid_server, SIGTERM);
    waitpid(srv->pid_server, NULL, 0);

    srv->pid_server = -1;

    return 0;
}

/* ===================== */
/*      commande list    */
/* ===================== */

int beuip_liste(void)
{
    int s = init_client();
    if (s < 0)
        return -1;

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char msg[LBUF];

    msg[0] = '3';
    strcpy(msg + 1, "BEUIP");

    sendto(s, msg, 6, 0, (struct sockaddr *)&addr, sizeof(addr));

    return 0;
}

/* ===================== */
/*    message privé      */
/* ===================== */

int beuip_send_to(const char *pseudo, const char *message)
{
    int s = init_client();
    if (s < 0)
        return -1;

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buf[LBUF];

    buf[0] = '4';
    strcpy(buf + 1, "BEUIP");

    strcpy(buf + 6, pseudo);

    int pos = 6 + strlen(pseudo) + 1;

    strcpy(buf + pos, message);

    int len = pos + strlen(message) + 1;

    sendto(s, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));

    return 0;
}

/* ===================== */
/*    message global     */
/* ===================== */

int beuip_send_all(const char *message)
{
    int s = init_client();
    if (s < 0)
        return -1;

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buf[LBUF];

    buf[0] = '5';
    strcpy(buf + 1, "BEUIP");

    strcpy(buf + 6, message);

    int len = 6 + strlen(message) + 1;

    sendto(s, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));

    return 0;
}