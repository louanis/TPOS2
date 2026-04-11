#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "gescom.h"
#include "creme.h" 

char **Mots = NULL;
int NMots = 0;

void ignore_sigint(int sig) {
    (void)sig;
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

char *fabrique_prompt(void) {
    char hostname[256];
    char *user = getenv("USER");
    if (!user) user = "unknown";
    if(gethostname(hostname, sizeof(hostname))!=0) perror("gethostname NULL");
    int root = (geteuid() == 0);
    size_t len = strlen(user) + strlen(hostname) + 4;
    char *prompt = malloc(len);
    snprintf(prompt, len, "%s@%s %c  ", user, hostname, root ? '#' : '$');
    return prompt;
}

void freeMots(void) {
    for (int i = 0; i < NMots; i++) free(Mots[i]);
    free(Mots);
    Mots = NULL;
    NMots = 0;
}

int analyseCom(char *b) {
    char *buffer = strdup(b);
    char *token;
    char *ptr = buffer;
    NMots = 0;
    int tailleMots = 100;
    Mots = malloc(tailleMots * sizeof(char *));
    while ((token = strsep(&ptr, " \t\n")) != NULL) {
        if (strlen(token) == 0) continue;
        if (NMots >= tailleMots) {
            tailleMots *= 2;
            Mots = realloc(Mots, tailleMots * sizeof(char *));
        }
        Mots[NMots++] = strdup(token);
    }
    Mots[NMots] = NULL;
    free(buffer);
    return NMots;
}

int gerer_commandes_reseau(int n, char **mots) {
    if (mots == NULL || mots[0] == NULL) return 0;
    
    if (strcmp(mots[0], "beuip") == 0) {
        if (n == 3 && strcmp(mots[1], "start") == 0) {
            beuip_start(mots[2]);
        } 
        else if (n == 2 && strcmp(mots[1], "stop") == 0) {
            beuip_stop();
        } 
        else if (n == 2 && strcmp(mots[1], "list") == 0) {
            commande('3', NULL, NULL); // Appel de liste
        } 
        else if (n >= 4 && strcmp(mots[1], "message") == 0) {
            // Reconstitution du message
            char msg[512] = "";
            for(int i = 3; i < n; i++) {
                strcat(msg, mots[i]);
                if(i < n - 1) strcat(msg, " ");
            }
            
            if (strcmp(mots[2], "all") == 0) {
                commande('5', msg, NULL);
            } else {
                commande('4', msg, mots[2]);
            }
        }
        // Bonus TP3
        else if (n == 3 && strcmp(mots[1], "ls") == 0) {
            demandeListe(mots[2]);
        } 
        else if (n == 4 && strcmp(mots[1], "get") == 0) {
            demandeFichier(mots[2], mots[3]);
        } 
        else {
            printf("Usage: beuip start <user> | beuip stop | beuip list | beuip message <user/all> <msg>\n");
        }
        return 1;
    }
    return 0;
}

int main(void) {
    char *ligne;
    signal(SIGINT, ignore_sigint);
    majComInt(); 
    read_history(".biceps_history");

    while (1) {
        char *prompt = fabrique_prompt();
        ligne = readline(prompt);
        free(prompt);
        if (!ligne) break;
        if (*ligne) add_history(ligne);

        int lasti = 0;
        int i = 0;
        for (; ligne[i] != '\0'; i++) {
            if (ligne[i] == ';') {
                char *commande1 = malloc(sizeof(char) * (i - lasti + 1));
                memcpy(commande1, &ligne[lasti], i - lasti);
                commande1[i - lasti] = '\0';
                int n = analyseCom(commande1);
                
                if (n > 0 && Mots[0] != NULL) {
                    if (!gerer_commandes_reseau(n, Mots)) {
                        int retval = execComInt(n, Mots); 
                        if (retval == 0) execComExt(Mots); 
                    }
                }
                freeMots();
                free(commande1);
                lasti = i + 1;
            }
        }

        char *commande1 = malloc(sizeof(char) * (i - lasti + 1));
        memcpy(commande1, &ligne[lasti], i - lasti);
        commande1[i - lasti] = '\0';
        int n = analyseCom(commande1);

        if (n > 0 && Mots[0] != NULL) {
            if (!gerer_commandes_reseau(n, Mots)) {
                int retval = execComInt(n, Mots); 
                if (retval == 0) execComExt(Mots); 
            }
        }
        freeMots();
        free(commande1);
        free(ligne);
    }
    
    beuip_stop(); // fermer le serveur quand on quitte
    printf("Bye !\n");
    write_history(".biceps_history");
    clear_history();
    return 0;
}