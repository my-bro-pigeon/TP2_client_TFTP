#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>


#define DATA_LENGTH 516
#define ACK_LENGTH 4
#define ERR_LENGTH 5
#define MSG_LENGTH 128
#define PORT "69"
const char erreur_cmd[MSG_LENGTH]="Erreur la commande doit être de la forme :./puttftp [IP_address] [name_file]\n";
const char WRQ_msg[MSG_LENGTH]="Un paquet WRQ a bien été transmis au serveur\n"; 
const char data_msg[MSG_LENGTH]="Le fichier souhaité a bien été envoyé\n";


/* Prototypes fonctions */
int creat_connection_socket( struct addrinfo **serv_addr, char * IP, char * port);
int make_wrq(char * name_file, char * mode, char** rrq);
void send_wrq(struct addrinfo *serv_addr, int sockfd, char* name_file, char * mode);
void send_data(struct addrinfo *serv_addr, int sockfd, char* name_file);


int main (int argc, char * argv[]){

	int sockfd;
	char *mode = "octet";
	struct addrinfo *res;

	
	/* Test de la bonne forme des arguments de la commande */
	if(argc !=3){
		write(STDOUT_FILENO, erreur_cmd,MSG_LENGTH);
		exit(EXIT_FAILURE);
	}
	
	
	char *IP_serveur = argv[1];
	char *name_file = argv[2];

	
	sockfd = creat_connection_socket(&res, IP_serveur, PORT); //réservation d'un socket de connexion vers le serveur 
    
    send_wrq(res, sockfd, name_file, mode); //création et envoi d'un paquet WRQ
    
    write( STDOUT_FILENO, WRQ_msg, MSG_LENGTH);
    
    send_data(res, sockfd, name_file); // Recupération des acquitements envoyés par le serveur et envoi des données
 

	
	free(res);
	close(sockfd);
		
		
	exit(EXIT_SUCCESS);

}

int creat_connection_socket( struct addrinfo **serv_addr, char * IP, char * port){
	int sock;
	int sockfd; //le descripteur du socket 
	struct addrinfo hints = {0};  //hints pointe sur une structure addrinfo qui spécifie les critères de sélection des structures d'adresses de sockets renvoyée dans la liste pointé par res.
	memset(&hints, 0 , sizeof(hints));
	hints.ai_family= AF_INET;   // Autorise IPv4 
	hints.ai_socktype = SOCK_DGRAM;  // Mode datagramme (UDP)
	hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_UDP; // protocol UDP
    if((sock = getaddrinfo(IP, port, &hints, serv_addr)) != 0){ perror("Error getaddrinfo impossible \n"); exit(EXIT_FAILURE);} //Récupération des informations sur le serveur
    if((sockfd = socket((*serv_addr)->ai_family, (*serv_addr)->ai_socktype, (*serv_addr)->ai_protocol))==-1){ perror("Error socket impossible créer\n"); exit(EXIT_FAILURE);} //Ouverture du socket
    
    return sockfd;    
	
}
																   /*                   ------------------------------------------------------------------   */
int make_wrq(char * name_file, char * mode, char** wrq){           /* Packet WRQ   :::  || opcode(2bytes)=2 || filename || 0(1byte) || mode || 0(1byte) ||   */
	char *wrq_packet;											   /*                   ------------------------------------------------------------------   */
	int len_packet;
	len_packet = (2+ strlen(name_file) + 1 + strlen(mode) + 1)*sizeof(char);
	
	wrq_packet = malloc(len_packet);
	memset(wrq_packet, 0, sizeof(*wrq_packet));
	
	/*Opcode :*/
	wrq_packet[0]=0; 
	wrq_packet[1]=2;
	
	/*filename*/
	strcpy(wrq_packet +2, name_file);
	
	//wrq_packet[2+strlen(name_file)]=0;
	
	/*mode*/
	strcpy(wrq_packet +3+strlen(name_file), mode);
	
	//wrq_packet[2+strlen(name_file)+1+strlen(mode)] = 0;
	
	*wrq = wrq_packet;
	
	return len_packet;
}

void send_wrq(struct addrinfo *serv_addr, int sockfd, char* name_file, char * mode){
	char* wrq;
	int len_wrq;
	/*création paquet WRQ*/
	len_wrq = make_wrq(name_file, mode, &wrq);
	/* envoi du paquet WRQ*/
	if(sendto(sockfd, wrq, len_wrq, 0, (struct sockaddr *)serv_addr->ai_addr, serv_addr->ai_addrlen) == -1){ perror("Error envoi packet WRQ\n"); exit(EXIT_FAILURE);}

	free(wrq);
	
}

void send_data(struct addrinfo *serv_addr, int sockfd, char* name_file){
	int size_ack;
	int number_ack;
	int size_write;
	char *buff;
	char buff2[DATA_LENGTH];
	FILE* file_data;
	buff=malloc(DATA_LENGTH*sizeof(char));
	char ACK[4]={0,4,0,0};
	
	if((file_data=fopen(name_file,"r"))==NULL){  perror("Ouverture du fichier impossible\n"); exit(EXIT_FAILURE);} //On ouvre le fichier à envoyer au serveur
	
	do{
			if((size_ack = recvfrom(sockfd, buff, DATA_LENGTH,0, serv_addr->ai_addr, &serv_addr->ai_addrlen))==-1){ perror("impossible de recevoir des données\n"); exit(EXIT_FAILURE);}

			// Si l'Opcode des données envoyé vaut 5, il s'agit d'un paquet d'erreur :
			if(buff[0]=='0' && buff[1]=='5'){
				fwrite(buff+4, sizeof(char),size_ack-5, stdout);  // On affiche alors le message d'erreur. 
				exit(EXIT_FAILURE);
			}
			/*On test que le paquet reçu est bien un acquitement*/
			if(buff[1]!=ACK[1]){
				write(STDOUT_FILENO, "erreur d'acquittement", MSG_LENGTH);
				exit(EXIT_FAILURE);		
			}
			/*On récupère le numéro de l'acquitement pour pouvoir en déduire le numéro du paquet suivant à envoyer*/
			number_ack = buff[3];
			
			/*On récupère au maximum 512 éléments du fichier à transmettre*/
			size_write = fread(buff2,sizeof(char), DATA_LENGTH,file_data);
			if((size_write  ==-1)){ perror("impossible de lire les données du fichier\n"); exit(EXIT_FAILURE);}
			
			buff2[size_write]='\0';
			
			/* On forme le paquet DATA à envoyer en rajoutant le Opcode et le block*/ 
			char *packet;
			packet = malloc(size_write+4);
			packet[0]=0;
			packet[1]=3;
			packet[2]=0;
			packet[3]=number_ack+1;
			strcpy(packet+4,buff2);
			

			/*On envoie le paquet DATA*/
			if(sendto(sockfd, packet, size_write+4, 0, serv_addr->ai_addr, serv_addr->ai_addrlen)==-1){perror("impossible d'envoyer le paquet \n"); exit(EXIT_FAILURE);}
			
			free(packet);
			
	}while(size_write == DATA_LENGTH); //On sait qu'il s'agit du dernier paquet à envoyer lorsque la valeur renvoyée par fread() ne vaut plus 512.
	
	write(STDOUT_FILENO, data_msg, MSG_LENGTH);
	
	free(buff);
	fclose(file_data);
}




