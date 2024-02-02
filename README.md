# Algorithmes des réseaux

## Chat à deux utilisateurs

Complétez le programme `client-chat.c` pour réaliser un client d'un chat en mode texte pour deux utilisateurs. Le programme devra être double pile, c'est-à-dire être compatible avec des clients `IPv4` et `IPv6`. Le protocole `UDP` sera utilisé au niveau transport. 

Le programme `client-chat` admet en argument le numéro de port sur lequel le programme devra écouter (si aucun autre client n'est déjà en écoute) ou le numéro de port sur lequel joindre un client déjà présent :

    ./client-chat port_number

Les seuls numéros de port valides sont ceux contenus dans l'intervalle `[10000; 65000]`.

Le programme utilise un protocole simpliste en mode texte pour l'ouverture et la fermeture du salon de discussion. Les commandes admises sont : `/HELO` et `/QUIT` comme illusté sur le diagramme état transition sur la figure :

![state chart](protocol.svg)

## Installation
```bash
$git clone
```
Pour compiler le programme, il suffit de taper sur le terminal `scons` qui va lancer le script python `SConstruct` qui généra quatre éxecutables grâce à une compilation conditionnelle :
- client-chat
- client-chat-bin
- client-chat-file
- client-chat-usr

## Utilisation
Pour executer la version basique où un client et le seveur peuvent en communiquer, ouvrez deux terminaux et entrez la commande suivante :
```bash
$ ./client-chat 15000
```
Avec l'executable `client-chat-bin` c'est quasi la même version que celle basique sauf qu'avec celle-là on utilise des commandes binaires pour initier le salon de discussion au lieu des commandes textes.

Par contre, avec `client-chat-file` c'est un executable qui permet le transfert de fichier securisé entre le client et le serveur en utilisant le protocole UDP. Les fichiers transferés seront logés respectivement dans les dossiers `/client-files` et `/server-files` selon celui qui l'envoie.

En fin, dans le dernier executable, j'ai utilisé un segment de mémoire partagé ce qui limite la communication des utilisateurs à une seule machine. Dans cette communication plusieurs utilisateurs peuvent s'envoyer simulanement des messages.

