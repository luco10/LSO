/* Bersaglio */

/* Inclusione librerie */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

/* Prototipo */
void stampa_esito(int id_server, int esito); /* Funzione per la stampa dell'esito */

/* Parametri in input  (1) ID (2) IP Server (3) Porta Server (4) X,Y */
int main(int argc, char **argv)
{
    int sock_server, parametri[3], esito[2];
    struct sockaddr_in server;
    char *server_ip = (char*)malloc(sizeof(100)), *server_port = (char*)malloc(sizeof(100)), *x, *y, id_serv[100];

    if( argc < 5 )
    {
        write( 1, "\nNon sono presenti tutti i parametri su riga di comando\n", strlen("\nNon sono presenti tutti i parametri su riga di comando\n") );
        exit(EXIT_FAILURE);
    }

    parametri[0] = atoi(argv[1]); /* Estrae ID missile da riga di comando */
    strcpy(server_ip, argv[2]); /* Estrae ip da riga di comando */
    strcpy(server_port, argv[3]); /* Estrae la porta da riga di comando */
    x = strtok(argv[4],",\n"); /* Estrae la coordinata x da riga di comando */
    y = strtok(NULL,",\n"); /* Estrae la coordinata y da riga di comando */

    parametri[1] = atoi(x);
    parametri[2] = atoi(y);

    /* Creazione socket per connessione al Client */
    if(( sock_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        perror("\nErrore socket");
        exit(EXIT_FAILURE);
    }

    /* Inizializzazione della struttura sockaddr_in */
    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(server_port));

    /* Connessione al Server */
    if (connect(sock_server , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("\nErrore connessione");
        exit(EXIT_FAILURE);
    }

    /* Invia i parametri (id, x, y) al Server */
    write( sock_server, parametri, sizeof(int)*3 );

    /* Riceve l'esito dal Server */
    read( sock_server, esito, sizeof(int)*2);
    sprintf(id_serv, "\nConnesso alla Torretta %d - %s\n", esito[0],server_ip);
    write( 1, id_serv, strlen(id_serv) );

    /* Visualizza l'esito del bersaglio */
    stampa_esito(esito[0], esito[1]);
    return 0;
}

void stampa_esito(int id_server, int esito)
{
    char stampa_bersaglio[100];

    if( esito == -1)
    {
        sprintf( stampa_bersaglio,"\nNon raggiungibile da nessuna Torretta\n");
        write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
    }
    else if( esito == 0 )
    {
        sprintf( stampa_bersaglio,"\nMissili della Torretta %d insufficienti\n",id_server);
        write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
    }
    else
    {
        sprintf( stampa_bersaglio,"\nColpito dalla Torretta %d\n",id_server);
        write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
    }
}
