# 👨‍💻 TP2_client_TFTP

Vous trouverez ici le client TFTP réalisé lors du deuxième TP de majeur informatique.  
Les commandes disponibles sont: 
- L'acquisition d'un fichier via [gettftp](/gettftp.c) : ./gettftp {adresse TP} {nom du fichier}
- L'envoi d'un fichier via [puttftp](/puttftp.c) : ./puttftp {adresse IP} {nom du fichier}
    
Le bon fonctionnement des deux commandes a été testé et obsérvé avec Wireshark pour des fichies constitués d'un ou plusieurs paquets comme le montre l'[image](/test_TFTP_client.PNG). 
N'ayant pas réussi à utiliser le serveur local disponible sur moodle à cause du port qu'il utilisait, j'ai utilisé un serveur local sur le port 69 : [Xinetd](https://mohammadthalif.wordpress.com/2010/03/05/installing-and-testing-tftpd-in-ubuntudebian/).
Le code n'a cependant pas pu être testé avec des fichiers de type .PNG . 

Une amélioration aurait été d'implémenter l'utilisation de l'option blocksize.

