#ifndef GESCOM_H
#define GESCOM_H

/* Met à jour les commandes internes (vide pour notre test) */
void majComInt(void);

/* Exécute une commande interne (renverra 0 pour dire qu'elle ne connaît rien) */
int execComInt(int n, char **mots);

/* Exécute une commande externe basique via execvp */
void execComExt(char **mots);

#endif /* GESCOM_H */