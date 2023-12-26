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

### Protocole de transfert de fichier fiable en utilisant le protocole UDP.

En effet, deux commandes sont indispensables pour le bon fonctionnement de cette fonctionnalité, à savoir :

1. La commande `/GET [file name]`

   Ainsi, pour que le transfert de fichier puisse fonctionner correctement il faudrait qu'un client demande via `sendto()` au serveur le fichier qu'il souhaiterait obtenir en entrant la commande suivante `/GET [file name]`. Et par la suite, je vérifie que le fichier demandé existe bien dans le serveur, si oui alors j'envoie au client le nombre de frame qu'il doit recevoir et j'attends un ack de sa part. Après avoir envoyé le nombre de frames au client et attendu un accusé de réception (ack), le serveur commence à envoyer les frames du fichier demandé séquentiellement.

   - Le serveur lit le fichier par segments (frames) et les envoie au client.
   - Après l'envoi de chaque frame, il attend un ack du client pour confirmer la bonne réception de ce segment spécifique.
   - Si le serveur ne reçoit pas l'ack pour un segment particulier, il le renvoie jusqu'à ce qu'il reçoive l'ack correspondant ou jusqu'à un certain nombre maximal de tentatives (dans notre cas, on a 200 tentatives mentionnées).
   - En cas de réception d'un ack manquant ou incorrect, le serveur garde une trace du nombre de tentatives de renvoi (dropFrame).
   - Si le nombre maximal de tentatives est atteint pour un segment donné sans recevoir l'ack attendu, le serveur peut marquer un drapeau de dépassement de temps (tOutFlag) et arrêter le transfert.

   En plus, lors de la réception de tous les acks attendus pour chaque frame (segments de fichier), le serveur désactive l'option de timeout pour la communication avec ce client spécifique.

   Finalement, le serveur informe le client de la bonne réception de tous les segments ou, dans le cas d'un dépassement de temps ou d'un échec de transfert pour une raison quelconque, annonce que le fichier n'a pas été envoyé. Et il faudra également noter que c'est le client qui envoi la commande /GET sinon ça ne marchera pas.

   Par ailleurs, voici les bouts de code du dépot GITHUB que j'ai modifié et adapté à mon programme :

   - Pour la commande `/GET [file name]` :

     Coté server, nous avons :

     ```c
       printf("Server: Get called with file name --> %s\n", flname_recv);

       if (access(flname_recv, F_OK) == 0)
       { // Check if file exist

           int total_frame = 0, resend_frame = 0, drop_frame = 0, t_out_flag = 0;
           long int i = 0;

           stat(flname_recv, &st);
           f_size = st.st_size; // Size of the file

           t_out.tv_sec = 2;
           t_out.tv_usec = 0;
           setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); // Set timeout option for recvfrom

           fptr = fopen(flname_recv, "rb"); // open the file to be sent

           if ((f_size % BUF_SIZE) != 0)
               total_frame = (f_size / BUF_SIZE) + 1; // Total number of frames to be sent
           else
               total_frame = (f_size / BUF_SIZE);

           printf("Total number of packets ---> %d\n", total_frame);

           length = sizeof(cl_addr);

           sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *)&cl_addr, sizeof(cl_addr)); // Send number of packets (to be transmitted) to reciever
           recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *)&cl_addr, (socklen_t *)&length);

           while (ack_num != total_frame) // Check for the acknowledgement
           {
               /*keep Retrying until the ack matches*/
               sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *)&cl_addr, sizeof(cl_addr));
               recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *)&cl_addr, (socklen_t *)&length);

               resend_frame++;

               /*Enable timeout flag even if it fails after 20 tries*/
               if (resend_frame == 20)
               {
                   t_out_flag = 1;
                   break;
               }
           }

           /*transmit data frames sequentially followed by an acknowledgement matching*/
           for (i = 1; i <= total_frame; i++)
           {
               memset(&frame, 0, sizeof(frame));
               ack_num = 0;
               frame.ID = i;
               frame.length = fread(frame.data, 1, BUF_SIZE, fptr);

               sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *)&cl_addr, sizeof(cl_addr));            // send the frame
               recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *)&cl_addr, (socklen_t *)&length); // Recieve the acknowledgement

               while (ack_num != frame.ID) // Check for ack
               {
                   /*keep retrying until the ack matches*/
                   sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *)&cl_addr, sizeof(cl_addr));
                   recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *)&cl_addr, (socklen_t *)&length);
                   printf("frame ---> %ld	dropped, %d times\n", frame.ID, ++drop_frame);

                   resend_frame++;

                   printf("frame ---> %ld	dropped, %d times\n", frame.ID, drop_frame);

                   /*Enable the timeout flag even if it fails after 200 tries*/
                   if (resend_frame == 200)
                   {
                       t_out_flag = 1;
                       break;
                   }
               }

               resend_frame = 0;
               drop_frame = 0;

               /*File transfer fails if timeout occurs*/
               if (t_out_flag == 1)
               {
                   printf("File not sent\n");
                   break;
               }

               printf("frame ----> %ld	Ack ----> %ld \n", i, ack_num);

               if (total_frame == ack_num)
                   printf("File sent\n");
           }
           fclose(fptr);

           t_out.tv_sec = 0;
           t_out.tv_usec = 0;
           setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); // Disable the timeout option
       }
       else
       {
           printf("Invalid Filename\n");
       }

     ```

     Et coté client, nous avons :

     ```c
     // The client.
       long int total_frame = 0;
       long int bytes_rec = 0, i = 0;

       t_out.tv_sec = 2;
       setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); // Enable the timeout option if client does not respond

       recvfrom(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *)&from_addr, (socklen_t *)&length); // Get the total number of frame to recieve

       t_out.tv_sec = 0;
       setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); // Disable the timeout option

       if (total_frame > 0)
       {
           sendto(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
           printf("----> %ld\n", total_frame);

           fptr = fopen(flname, "wb"); // open the file in write mode

           /*Recieve all the frames and send the acknowledgement sequentially*/
           for (i = 1; i <= total_frame; i++)
           {
               memset(&frame, 0, sizeof(frame));

               recvfrom(cfd, &(frame), sizeof(frame), 0, (struct sockaddr *)&from_addr, (socklen_t *)&length);  // Recieve the frame
               sendto(cfd, &(frame.ID), sizeof(frame.ID), 0, (struct sockaddr *)&send_addr, sizeof(send_addr)); // Send the ack

               /*Drop the repeated frame*/
               if ((frame.ID < i) || (frame.ID > i))
                   i--;
               else
               {
                   fwrite(frame.data, 1, frame.length, fptr); /*Write the recieved data to the file*/
                   printf("frame.ID ---> %ld	frame.length ---> %ld\n", frame.ID, frame.length);
                   bytes_rec += frame.length;
               }

               if (i == total_frame)
               {
                   printf("File recieved\n");
               }
           }
           printf("Total bytes recieved ---> %ld\n", bytes_rec);
           if (fclose(fptr) == EOF)
           {
               perror("fclose(fptr)");
           }
       }
       else
       {
           printf("File is empty\n");
       }
     ```

2. La commande `/PUT [File name]` :

   Cette commande permet à un client d'envoyer un fichier au server, on commence par construire le chemin à partir du nom de fichier obtenu précédemment, ensuite on vérifie si le fichier existe dans le dossier `./clientFiles` si non on affiche un message indiquant qu'il existe pas sinon on procède au traitement du fichier pour l'envoi qui se resume à récupérer la taille du fichier pour déterminer le nombre de segments (frames) à envoyer via `sendto` et on attends un accusé de reception de ce nombre de frame de la part du server pour confirmation de la bonne reception. Ensuite, on commence à envoyer les "frames" au serveur. Après chaque envoi, on attend un ACK correspondant du serveur. Si le client ne reçoit pas l'ACK correspondant, il réessaie un nombre limité de fois et le serveur de son coté reçoit les frames du client et envoie des ACKs correspondant pour chaque frame reçue. Il vérifie également les doublons et les pertes potentielles de "frames"; et cela se fait dans cette boucle :

   ```c
       /*Recieve all the frames and send the acknowledgement sequentially*/
    for (i = 1; i <= totalFrame; i++)
    {
        memset(&sframe, 0, sizeof(sframe));

        // Recieve the sframe
        CHECK(recvfrom(sockfd, &(sframe), sizeof(sframe), 0,
                       (struct sockaddr *)&clientStorage,
                       (socklen_t *)&clientLen));
        // Send the ack
        CHECK(sendto(sockfd, &(sframe.ID), sizeof(sframe.ID),
                     0, (struct sockaddr *)&clientStorage,
                     clientLen));

        /*Drop the repeated sframe*/
        if ((sframe.ID < i) || (sframe.ID > i))
        {
            i--;
        }
        else
        {
            /*Write the recieved data to the file*/
            fwrite(sframe.data, 1, sframe.length, sfptr);
            printf("sframe.ID ----> %ld	sframe.length ---->"
                   "%ld\n",
                   sframe.ID, sframe.length);

            bytes_rec += sframe.length;
        }

        if (i == totalFrame)
            printf("File recieved\n");
    }
   ```

   En outre, une fois que le client envoie tous les frames sont envoyés avec succès et que les ACKs correspondants sont reçus, il termine le transfert de fichier. Quant au serveur, après avoir reçu tous les "frames" attendu, il assemble les données et écrit le fichier.
