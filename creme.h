#ifndef CREME_H
#define CREME_H

/* Gestion du serveur */
int beuip_start(const char *pseudo);
int beuip_stop(void);

/* Fonction unifiee pour les commandes reseau (liste, message) */
void commande(char octet1, char *message, char *pseudo);

/* Transfert de fichiers (TCP) */
void demandeListe(char *pseudo);
void demandeFichier(char *pseudo, char *nomfic);

#endif /* CREME_H */