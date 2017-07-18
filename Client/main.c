#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr

int main(int argc , char **argv){
    int server_sock;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];

    // Controlla il numero dei parametri da linea di comando
	if(argc < 6){
		write(1, "Parametri non inseriti da linea di comando\n", strlen("Parametri non inseriti da linea di comando\n"));
		exit(EXIT_FAILURE);
	}

    // Creazione socket per connessione al Server
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("\nErrore socket");
        exit(EXIT_FAILURE);
    }

    // Inizializzazione della struttura sockaddr_in
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    // Connessione al Server
    if (connect(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("\nErrore connessione");
        exit(EXIT_FAILURE);
    }

    // Invia i parametri (IP e porta) al Server
    write(server_sock, parametri, sizeof(int)*3);

    /* Va modifica la write sopra, inoltre sotto va gestita la risposta del server(Vedi come ha fatto peolo in Client.c) */


    /*
    //keep communicating with server
    while(1)
    {
        printf("Enter message : ");
        scanf("%s" , message);

        //Send some data
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }

        //Receive a reply from the server
        if( recv(sock , server_reply , 2000 , 0) < 0)
        {
            puts("recv failed");
            break;
        }

        puts("Server reply :");
        puts(server_reply);
    }
    */

    close(server_sock);
    return 0;
}
