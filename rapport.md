---
title: Chat Application
author: DIALLO Abdoul Aziz
date: 06/10/2023
...

# Chat Application

## Fonctionnalités

### Protocole en mode binaire

Dans ce protocole binaire, j'ai définis 2 type de commande: la commande QUIT et la commande HELO et chacune d'elle est codé sur 1 octet.

Ainsi, pour s'identifier au serveur, si BIN est défini, je déclare un buffer de 1 octet de type `uint8_t` et je le remplis avec la commande correspondante. Et j'utilise un pointeur générique pour passer ce buffer à la fonction `sendto()`, j'ai fait cela afin de pouvoir factoriser le code c'est-à-dire éviter d'écrire `sendto()` deux fois.

```c
#ifdef BIN
            uint8_t binBuff[1] = {HELO};
            const void *buff = binBuff;
            size = 1; // 1 octet.
#else
            const char *messageBuff = "/HELO";
            const void *buff = messageBuff;
            size = 5; // 5 caractères
#endif
```

Vous remarquerez également plus tard que j'ai utilisé la même technique tout juste avant que le serveur reçoit le message d'identification du client. Je prépare un buffer et j'utilise un pointeur générique à ce buffer et selon que le protocole soit binaire ou non, je le remplis en utilisant qu'une seule fois la primitive `recvfrom()` ceci afin d'éviter aussi d'écrire deux fois la primitive `recvfrom()`.

Enfin, lorsque la salle de discussion est créée, il faut et il suffit qu'une instance entre au clavier /QUIT qui a une taille de 5 octets et après j'utilise seulement la première case du tableau pour y mettre la commande QUIT et je rajoute le caractère de fin de chaîne `'\0'` afin que la fonction `strlen()` retourne 1 qui est la nouvelle taille du buffer et je l'envoie ensuite au serveur.
