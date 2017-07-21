#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h> //for threading , link with lpthread

//the thread function
void *connection_handler(void *);

int main(int argc, char **argv){
    int desc_sock, client_sock, c, *new_sock;
    struct sockaddr_in server, client;

    // Controlla il numero dei parametri da linea di comando
	if(argc < 2){
		write(1, "Parametri non inseriti da linea di comando\n", strlen("\nParametri non inseriti da linea di comando\n"));
		exit(EXIT_FAILURE);
	}

    // Inizializza comunicazione con protocollo TCP/IP e numero di porta inserito da riga di comando
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[1]));
	server.sin_addr.s_addr = htonl(INADDR_ANY);

    // Crea socket per le comunicazioni
	if((desc_sock=socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
		perror("\nErrore Socket");
		exit(EXIT_FAILURE);
	}

    // Bind
	if(bind(desc_sock,(struct sockaddr *)&server, sizeof(server)) == -1){
		perror("\nErrore Bind");
		exit(EXIT_FAILURE);
	}

	// Listen
	if(listen(desc_sock,100000) == -1){
		perror("\nErrore listen");
		exit(EXIT_FAILURE);
	}

	write(1, "Server connesso ed in ascolto...\n", strlen("Server connesso ed in ascolto...\n"));

	c = sizeof(struct sockaddr_in);

	write(1, Request.ServerVariables("LOCAL_ADDR"), 10);

	while(1){

        // Accept
        client_sock = accept(desc_sock, (struct sockaddr *)&client, (socklen_t*)&c);

	}

    //Accept and incoming connection
    /*c = sizeof(struct sockaddr_in);
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;

        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0){
            perror("could not create thread");
            return 1;
        }

        //Now join the thread , so that we dont terminate before the thread
        pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }

    if (client_sock < 0){
        perror("accept failed");
        return 1;
    }
*/
    return 0;
}

/*
 * This will handle connection for each client
 *
void *connection_handler(void *socket_desc){
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , client_message[2000];

    //Send some messages to the client
    message = "Greetings! I am your connection handler\n";
    write(sock , message , strlen(message));

    message = "Now type something and i shall repeat what you type \n";
    write(sock , message , strlen(message));

    //Receive a message from client
    while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 ){
        //Send the message back to client
        write(sock , client_message , strlen(client_message));
    }

    if(read_size == 0){
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1){
        perror("recv failed");
    }

    //Free the socket pointer
    close(sock);
    free(socket_desc);

    return 0;
}*/
