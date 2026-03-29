#include "gescom.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>


extern char **Mots;
extern int NMots;


typedef struct
{
    char *nom;
    int (*eqmain)(int N, char **P);

} ComInt;

#define NBMAXC 10 /* Nb maxi de commandes internes */
ComInt TabComInt[NBMAXC];
static int NbComInt = 0;
const float version = 1.0;

#include "creme.h"

BEUIP_Server srv;

/* ===================== */
/* commande beuip        */
/* ===================== */

int Cmd_beuip(int N, char **P)
{
    if (N < 2)
    {
        printf("Usage : beuip start <pseudo> | stop\n");
        return 1;
    }

    if (strcmp(P[1], "start") == 0)
    {
        if (N != 3)
        {
            printf("Usage : beuip start <pseudo>\n");
            return 1;
        }

        return beuip_start(&srv, P[2]);
    }

    if (strcmp(P[1], "stop") == 0)
    {
        return beuip_stop(&srv);
    }

    printf("Commande inconnue\n");

    return 1;
}

/* ===================== */
/* commande mess         */
/* ===================== */

int Cmd_mess(int N, char **P)
{
    if (N == 1)
        return beuip_liste();

    if (strcmp(P[1], "all") == 0 && N >= 3)
        return beuip_send_all(P[2]);

    if (N >= 3)
        return beuip_send_to(P[1], P[2]);

    printf("Usage:\n");
    printf("mess\n");
    printf("mess pseudo message\n");
    printf("mess all message\n");

    return 1;
}


/* ==================== */
/*      Ajoute COM      */
/* ==================== */

void ajouteCom(char *nom, int (*eqmain)(int N, char **P))
{
    if (NbComInt < NBMAXC)
    {
        TabComInt[NbComInt].nom = nom;
        TabComInt[NbComInt].eqmain = eqmain;
        NbComInt++;
    }
    else
    {
        fprintf(stderr, "Erreur : tableau des commandes internes plein (NBMAXC=%d)\n", NBMAXC);
        exit(EXIT_FAILURE);
    }
}

/* =================== */
/*      Liste COM      */
/* =================== */
int listeComInt(int N, char **P)
{
    for (int i = 0; i < NbComInt; i++)
    {
        printf("Commande n°%d : %s,\n", i, TabComInt[i].nom);
    }
    return 0;
}

/* ================ */
/*      Sortie      */
/* ================ */
int Sortie(int N, char *P[]) { 
    write_history(".biceps_history");
    exit(0);
}

/* ================ */
/*      Entree      */
/* ================ */
int Entree(int N, char *P[])
{

    if (N < 2) {
        fprintf(stderr,"cd: argument manquant\n");
        return 1;
    }

    if (chdir(P[1]) != 0) {
        perror("cd");
    }


    return 0;
}

/* ================== */
/*      Location      */
/* ================== */
int Location(int N, char *P[])
{
    char buffer[256];
    if (getcwd(buffer, sizeof(buffer)) == NULL){
        perror("getcwd");
    }
    printf("%s\n", buffer);
    return 0;
}

/* ================= */
/*      Version      */
/* ================= */
int Version(int N, char *P[])
{
    printf("Nous sommes en version %.2f\n", version);
    return 0;
}

/* ================= */
/*      History      */
/* ================= */


int History(int N, char *P[])
{
    HIST_ENTRY **hist = history_list();

    if (hist) {
        for (int i = 0; hist[i]; i++) {
            printf("%d %s\n", i, hist[i]->line);
        }
    }

    return 0;
}

/* ================================ */
/*      Exec Commandes interne      */
/* ================================ */
int execComInt(int N, char **P)
{
    if (P[0] == NULL)
        return 0;
    for (int i = 0; i < NbComInt; i++)
    {
        if (strcmp(TabComInt[i].nom, Mots[0]) == 0)
        {
            TabComInt[i].eqmain(N, P);
            return 1;
        }
    }
    return 0;
}

/* ================================ */
/*      Exec Commandes externe      */
/* ================================ */

int execComExt(char **P)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {                    // fils
        execvp(P[0], P); // P[0] = nom commande, P = argv
        // Si execvp échoue :
        perror("execvp");
        _exit(EXIT_FAILURE); // on utilise _exit dans le fils
    }
    else
    { // père
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            perror("waitpid");
            return -1;
        }
#ifdef TRACE
        if (WIFEXITED(status))
        {
            printf("Processus fils terminé avec code %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Processus fils tué par signal %d\n", WTERMSIG(status));
        }
#endif
    }

    return 0;
}

/* ================= */
/*      Maj COM      */
/* ================= */
void majComInt(void) /* mise a jour des commandes internes */
{
    ajouteCom("help", listeComInt);
    ajouteCom("exit", Sortie);
    ajouteCom("cd", Entree);
    ajouteCom("pwd", Location);
    ajouteCom("vers", Version);
    ajouteCom("history", History);
    ajouteCom("beuip", Cmd_beuip);
    ajouteCom("mess", Cmd_mess);
}
