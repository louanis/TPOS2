#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "gescom.h"

void majComInt(void) {
    // rien à initialiser
}

int execComInt(int n, char **mots) {
    (void)n;
    (void)mots;
    // on renvoie 0 pour indiquer que ce n'est pas une commande interne connue par gescom
    // biceps va donc essayer de l'exécuter comme une commande externe.
    return 0;
}

void execComExt(char **mots) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Erreur fork");
    } else if (pid == 0) {
        // processus fils pour les vrai commande linux
        execvp(mots[0], mots);
        // on afficher l'erreur si probleme
        perror("Commande introuvable");
        _exit(1);
    } else {
        // on attend la fin de la commande
        wait(NULL);
    }
}