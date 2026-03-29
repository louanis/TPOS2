/* biceps.c : Bel Interpreteur de Commandes
Version 0.2 */

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


#include "gescom.h"



char **Mots = NULL;
int NMots = 0;


/* ============================= */
/*      Gestion Ctrl-C           */
/* ============================= */
void ignore_sigint(int sig)
{
    (void)sig;
    printf("\n"); // juste retour à la ligne
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

/* ============================= */
/*   Copie dynamique de chaine   */
/* ============================= */
// SUPPRIMEE COMME DEMANDEE DANS LENONCE
// char *copyString(char *s)
// {
//     if (!s)
//         return NULL;

//     size_t len = strlen(s) + 1;
//     char *copy = malloc(len);
//     if (!copy)
//     {
//         perror("malloc");
//         exit(EXIT_FAILURE);
//     }

//     strcpy(copy, s);
//     return copy;
// }

/* ============================= */
/*   Fabrication du prompt       */
/* ============================= */
char *fabrique_prompt(void)
{
    char hostname[256];
    char *user = getenv("USER");

    if (!user)
        user = "unknown";

    if(gethostname(hostname, sizeof(hostname))!=0){
        perror("gethostname NULL");
    }

    int root = (geteuid() == 0);
    size_t len = strlen(user) + strlen(hostname) + 4;

    char *prompt = malloc(len);
    if (!prompt)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    snprintf(prompt, len, "%s@%s %c  ",
             user,
             hostname,
             root ? '#' : '$');

    return prompt;
}

/* ============================= */
/*     Liberation memoire        */
/* ============================= */
void freeMots(void)
{
    for (int i = 0; i < NMots; i++)
        free(Mots[i]);

    free(Mots);
    Mots = NULL;
    NMots = 0;
}

/* ============================= */
/*      Analyse de commande      */
/* ============================= */
int analyseCom(char *b)
{
    char *buffer = strdup(b); // copie modifiable
    char *token;
    char *ptr = buffer;

    NMots = 0;
    int tailleMots = 100;
    Mots = malloc(tailleMots * sizeof(char *));
    if (!Mots)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    while ((token = strsep(&ptr, " \t\n")) != NULL)
    {
        if (strlen(token) == 0)
            continue;

        if (NMots >= tailleMots)
        {
            tailleMots *= 2;
            char **tmp = realloc(Mots, tailleMots * sizeof(char *));
            if (!tmp)
            {
                perror("realloc");
                freeMots();
                exit(EXIT_FAILURE);
            }
            Mots = tmp;
        }

        Mots[NMots++] = strdup(token);
    }
    Mots[NMots] = NULL;

    free(buffer);
    return NMots;
}

/* ============================= */
/*             MAIN              */
/* ============================= */
int main(void)
{

    
    char *ligne;

    signal(SIGINT, ignore_sigint);
    majComInt();
    read_history(".biceps_history");

    while (1)
    {

        char *prompt = fabrique_prompt();
        ligne = readline(prompt);
        free(prompt);

        if (!ligne)
            break;

        if (*ligne)
            add_history(ligne);

        int lasti = 0;
        int i = 0;
        for (; ligne[i] != '\0'; i++)
        {
            if (ligne[i] == ';')
            {

                char *commande1 = malloc(sizeof(char) * (i - lasti + 1));
                memcpy(commande1, &ligne[lasti], i - lasti);
                commande1[i - lasti] = '\0';

                int n = 0;
                n = analyseCom(commande1);

                if (n > 0 && Mots[0] != NULL)
                {
                    int retval = execComInt(n, Mots);
                    if (retval == 0)
                        execComExt(Mots);
                }

                freeMots();
                free(commande1);
                lasti = i + 1;
            }
        }

        /* On envoie la derniere commande qui ne finit donc pas par un ;*/
        char *commande1 = malloc(sizeof(char) * (i - lasti + 1));
        memcpy(commande1, &ligne[lasti], i - lasti);
        commande1[i - lasti] = '\0';


        int n = 0;
        n = analyseCom(commande1);

        if (n > 0 && Mots[0] != NULL)
        {
            int retval = execComInt(n, Mots);
            if (retval == 0)
                execComExt(Mots);
        }

        freeMots();
        free(commande1);
        free(ligne);
    }
    

    printf("Bye !\n");
    write_history(".biceps_history");
    return 0;
}