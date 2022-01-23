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
const char erreur_cmd[MSG_LENGTH]="Erreur la commande doit être de la forme :./gettftp [IP_address] [name_file]\n";
const char RRQ_msg[MSG_LENGTH]="Un paquet RRQ a bien été transmis au serveur\n"; 
const char data_msg[MSG_LENGTH]="Le fichier souhaité a bien été reçu\n";


/* Prototypes fonctions */
int creat_connection_socket( struct addrinfo **serv_addr, char * IP, char * port);
int make_rrq(char * name_file, char * mode, char** rrq);
void send_rrq(struct addrinfo *serv_addr, int sockfd, char* name_file, char * mode);
void get_data(struct addrinfo *serv_addr, int sockfd, char* name_file);





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
    
    send_rrq(res, sockfd, name_file, mode); //création et envoi d'un paquet RRQ
    
    write( STDOUT_FILENO, RRQ_msg, MSG_LENGTH);
    
    get_data(res, sockfd, name_file); //récupération des données envoyées par le serveur et envoi de paquets d'acquitements
 

	
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



																   /*                   ----------------------------------------------------------------   */
int make_rrq(char * name_file, char * mode, char** rrq){           /* Packet RRQ   :::  || opcode(2bytes) || filename || 0(1byte) || mode || 0(1byte) ||   */
	char *rrq_packet;											   /*                   ----------------------------------------------------------------   */
	int len_packet;
	len_packet = (2+ strlen(name_file) + 1 + strlen(mode) + 1)*sizeof(char);
	
	rrq_packet = malloc(len_packet);
	memset(rrq_packet, 0, sizeof(*rrq_packet));
	
	/*Opcode :*/
	rrq_packet[0]=0; 
	rrq_packet[1]=1;
	
	/*filename*/
	strcpy(rrq_packet +2, name_file);
	
	//rrq_packet[2+strlen(name_file)]=0;
	
	/*mode*/
	strcpy(rrq_packet +3+strlen(name_file), mode);
	
	//rrq_packet[2+strlen(name_file)+1+strlen(mode)] = 0;
	
	*rrq = rrq_packet;
	
	return len_packet;
}

void send_rrq(struct addrinfo *serv_addr, int sockfd, char* name_file, char * mode){
	char* rrq;
	int len_rrq;
	/*création paquet RRQ*/
	len_rrq = make_rrq(name_file, mode, &rrq);
	/* envoi du paquet RRQ*/
	if(sendto(sockfd, rrq, len_rrq, 0, (struct sockaddr *)serv_addr->ai_addr, serv_addr->ai_addrlen) == -1){ perror("Error envoi packet RRQ\n"); exit(EXIT_FAILURE);}

	free(rrq);
	
}



/*                   -------------------------------------------------------------   */
/* Packet ACK   :::  || opcode(2bytes)=3 || block(2bytes) || data(max 512bytes) ||   */
/*                   -------------------------------------------------------------   */
																							/*                   ---------------------------------------   */
void get_data(struct addrinfo *serv_addr, int sockfd, char* name_file){						/* Packet ACK   :::  || opcode(2bytes)=4 || block(2bytes) ||   */
	int size_packet;																		/*                   ---------------------------------------   */
	int size_write;
	char *buff;
	buff=malloc(DATA_LENGTH*sizeof(char));
	FILE* file_data;
	char ACK[4]={0,4,0,0}; // création du paquet ACK
	
	if((file_data=fopen(name_file,"w"))==NULL){  perror("Ouverture du fichier impossible\n"); exit(EXIT_FAILURE);} //On ouvre/crée le fichier dans lequel on va écrit les datas envoyées par le serveur
	
	do{ //On récupère l'un après l'autree les paquets Data
			// Stockage des données dans le buffer : 
			if((size_packet = recvfrom(sockfd, buff, DATA_LENGTH,0, serv_addr->ai_addr, &serv_addr->ai_addrlen))==-1){ perror("impossible de recevoir des données\n"); exit(EXIT_FAILURE);}
			
			// Si l'Opcode des données envoyés vaut 5, il s'agit d'un paquet d'erreur :
			if(buff[0]=='0' && buff[1]=='5'){
				fwrite(buff+4, sizeof(char),size_packet-5, stdout);  // On affiche alors le message d'erreur. 
				exit(EXIT_FAILURE);
			}

			size_write = fwrite(buff+4, sizeof(char), size_packet-4, file_data); //On ecrit les données dans le fichier en prenant soin d'enlever l'opcode et le numéro du paquet
			
			if((size_write  != size_packet-4)){ perror("impossible d'ecrire les données dans le fichier\n"); exit(EXIT_FAILURE);}
			
			/* On modifie le block de notre ACK avec le block du dernier paquet reçu */
			ACK[2]=buff[2]; 
			ACK[3]=buff[3]; 
			
			
			/* Envoi du paquet d'acquitement */
			if(sendto(sockfd, ACK, 4, 0, serv_addr->ai_addr, serv_addr->ai_addrlen)==-1){perror("impossible d'envoyer ACK \n"); exit(EXIT_FAILURE);}
			
		
			
		
	}while(size_packet == DATA_LENGTH); // On sait que l'on a reçu le dernier paquet lorsque la partie données a une taille inférieur à 512 octets. 
	
	write( STDOUT_FILENO, data_msg, MSG_LENGTH);
	
	free(buff);
	fclose(file_data);
}






