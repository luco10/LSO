/* Rendezvous */

/* Inclusione librerie */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

/* Struttura che contiene informazioni sul Server connesso */
typedef struct lista_server
{
     /* Tid processo */
     pthread_t thr;
     /* ID della torretta connessa */
     int id_torretta;
     /* Struttura che contiene le informazioni da inviare ai Server */
     struct sockaddr_in info;
}lista_server;

/* Dichiarazione variabili globali */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; /* Semaforo che gestisce la sincronizzazione tra thread */
pthread_mutex_t mutex_delete = PTHREAD_MUTEX_INITIALIZER; /* Semaforo che impedisce la cancellazione dei parametri dalla 'lista_server' durante l'utilizzo del master thread */
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; /* Cond è utilizzato per far inviare i parametri al nuovo Server connesso */
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER; /* Cond2 segnala al master thread che i Server hanno inviato le informazioni all'ultimo thread connesso */
lista_server ult_info; /* Variabile usata per memorizzare le informazioni sull'ultimo server connesso */
lista_server *list; /* Lista che contine le informazioni dei Server connessi */
int cont_thread_info = 0; /* Contatore dei thread che hanno inviato le informazioni all''ultimo Server connesso */
int cont_elem = 0; /* Contatore degli elementi connessi */

/* Prototipi */
void *gestione_server(void* fd);/* Funzione che gestisce la comunicazione di un Server */
void *invia_info(void* fd);/* Funzione che invia nuove informazioni al Server */

/* Parametri: (1) Porta Server di Rendezvous */
int main(int argc, char **argv)
{
    int i, sock, *sock_accept = NULL, lung_host = sizeof(struct sockaddr_in);
	pthread_t *th; /* Id thread che gestira' le comunicazioni tra un server e il server di rendzevous */
	struct sockaddr_in rendezvous; /* Contiene le informazioni sul Server di Rendezvous */
	struct sockaddr_in host; /* Contiene le informazioni sul Server connesso */
	struct sockaddr_in fine_lista; /* Messaggio di fine lista */
	int port[2]; /* Contiene ID e numero porta del nuovo Server connesso */
	char stampa_info[100]; /* Visualizza le informazioni dell'ultimo server connesso */
	int dim_list = 100; /* Dimensione della lista_server */

	sigset_t set;
	sigaddset(&set,SIGPIPE); /* Blocca il segnale SIGPIPE in caso di server disconnesso */
	sigprocmask(SIG_SETMASK,&set,NULL);

	/* Inizializza il messaggio di fine lista */
	fine_lista.sin_addr.s_addr = 0;
	fine_lista.sin_port = 0;

	/* Controlla il numero dei parametri da linea di comando */
	if( argc < 2)
	{
		write( 1, "\nParametri non inseriti da linea di comando\n", strlen("\nParametri non inseriti da linea di comando\n"));
		exit(EXIT_FAILURE);
	}

	list = malloc(sizeof(lista_server)*dim_list);

	/* Inizializza comunicazione con protocollo TCP/IP e numero di porta inserito da riga di comando */
	rendezvous.sin_family = AF_INET;
	rendezvous.sin_port = htons(atoi(argv[1]));
	rendezvous.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Crea socket per le comunicazioni */
	if(( sock=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		perror("\nErrore Socket");
		exit(EXIT_FAILURE);
	}

    /* Bind */
	if( (bind (sock, (struct sockaddr *)&rendezvous, sizeof(rendezvous))) == -1 )
	{
		perror("\nErrore Bind");
		exit(EXIT_FAILURE);
	}

	/* Listen */
	if( listen(sock,100) == -1 )
	{
		perror("\nErrore listen");
		exit(EXIT_FAILURE);
	}

    write( 1, "\nServer di Rendezvous connesso ed in ascolto:\n", strlen("\nServer di Rendezvous connesso ed in ascolto:\n") );

	while(1)
	{
		/* Alloca spazio per un nuovo file descriptor */
		sock_accept = malloc(sizeof(int));

        /* Accept */
		*sock_accept = accept( sock, (struct sockaddr *)&host,(socklen_t*)&lung_host);

		/* Legge ID e numero porta del nuovo Server*/
		read( *sock_accept , port,2*sizeof(int));

		/* Scrive le informazioni dell'ultimo Server in una variabile globale per poter essere lette da tutti gli altri Server della lista */
		ult_info.info.sin_addr.s_addr = host.sin_addr.s_addr;
		ult_info.info.sin_family = AF_INET;
		ult_info.info.sin_port = htons(port[0]);
        ult_info.id_torretta= port[1];

		/* Acquisizione del mutex */
		pthread_mutex_lock(&mutex_delete);

		/* Controlla se la lista può ospitare un altro elemento */
		if( dim_list == cont_elem )
		{
            /* Altrimenti si raddoppia la dimensione della lista */
			dim_list = dim_list*2;
			list = (lista_server*)realloc( list,  sizeof(lista_server)*dim_list );
		}

        /* Verifica che il server appena connesso abbia un ID diverso dai Server nella lista */
		for( i=0; (i<cont_elem) && (list[i].id_torretta!=ult_info.id_torretta); i++ );
        if(list[i].id_torretta == ult_info.id_torretta )
		{
			/* Se l'ID non è univoco, rifiuta la connessione */
			write( *sock_accept ,"NOT",strlen("NOT"));
			pthread_mutex_unlock(&mutex_delete);
			continue;
		}

		write( *sock_accept ,"YES",strlen("YES")); /* Se l'ID è univoco permette la connessione */
		sprintf( stampa_info ,"\nConnessione della Torretta %d - %s\n",port[1],inet_ntoa(ult_info.info.sin_addr));
		write( 1, stampa_info, strlen(stampa_info));

		/* Invia all'ultimo Server connesso le informazioni sui Server connessi */
		for( i=0; i < cont_elem; i++ )
			write( *sock_accept, &list[i].info, sizeof(struct sockaddr_in));

		write( *sock_accept, &fine_lista, sizeof(struct sockaddr_in) );/* Invia il messaggio di fine lista */
		cont_thread_info = 0; /* Reset del contatore */
		pthread_cond_broadcast(&cond); /* Risveglia i thread in attesa di leggere le informazioni sull'ultimo Server connesso */

		/* Attende che tutti i thread inviino le informazioni sull'ultimo Server connesso */
		while( cont_thread_info < cont_elem )
			pthread_cond_wait(&cond2,&mutex);
		th = malloc(sizeof(pthread_t));

		/* Crea un thread per gestire le comunicazioni con il nuovo Server */
		if((pthread_create( th, NULL, (void*) &gestione_server, sock_accept)) == -1)
		{
			perror("\nErrore pthread_create");
			continue;
		}

		/* Copia le informazioni del nuovo Server nella lista */
        memcpy(&(list[cont_elem].thr),th,sizeof(pthread_t));
		list[cont_elem].info.sin_addr.s_addr  = ult_info.info.sin_addr.s_addr ;
		list[cont_elem].info.sin_family = ult_info.info.sin_family;
		list[cont_elem].info.sin_port = ult_info.info.sin_port;
		list[cont_elem].id_torretta= ult_info.id_torretta;

        cont_elem++; /* Incrementa il numero di Server connessi */

		/* Rilascia i mutex */
		pthread_mutex_unlock(&mutex);
		pthread_mutex_unlock(&mutex_delete);
	}
	return 0;
}

void *gestione_server(void* fd)
{
	int sock = *(int*)fd, n, i;
	pthread_t tid;
	char check, da_canc[100];

	free(fd); /* Dealloca il file descriptor perchè non deve essere più utilizzato */
	pthread_create( &tid, NULL, (void*)invia_info, &sock ); /* Crea un thread per inviare informazioni al Server */
	n=read( sock, &check, 1 ) ; /* Rimane in ascolto */

	/* Gestione Socket broken */
	if( n == 0 )
	{
		/* Blocca il mutex_delete per evitare problemi di inconsistenza della lista quando viene usata dal master thread */
 		pthread_mutex_lock(&mutex_delete);

		/* Ricerca delle informazioni sul server gestito */
		for( i = 0;  pthread_equal(pthread_self(), list[i].thr) == 0 ; i++ );

		sprintf(da_canc,"\nServer %s disconnesso\n",inet_ntoa(list[i].info.sin_addr));
		write( 1, da_canc, strlen(da_canc) );

        /* Scambia l'ultimo elemento della lista con le informazioni */
        memcpy(&(list[i].thr),&(list[cont_elem-1].thr),sizeof(pthread_t));
		list[i].info.sin_addr.s_addr = list[cont_elem-1].info.sin_addr.s_addr;
		list[i].info.sin_port = list[cont_elem-1].info.sin_port;
        list[i].id_torretta = list[cont_elem-1].id_torretta;

        /* Azzera l'ultimo elemento */
		list[cont_elem-1].info.sin_addr.s_addr = 0;
		list[cont_elem-1].info.sin_port = 0 ;
		list[cont_elem-1].id_torretta = 0;

		cont_elem--; /* Aggiorna il numero di elementi nella lista_server */
		pthread_mutex_unlock(&mutex_delete); /* Rilascia il mutex_delete */
		close(sock); /* Chiusura del socket broken */
		pthread_cancel( tid ); /* Cancella il thread 'invia_info' */
		pthread_exit(NULL); /* Termina thread */
	}
     return NULL;
}

/* Invia al Server le informazioni del nuovo Server connesso */
void *invia_info(void* fd)
{
	while(1)
	{
		pthread_cond_wait( &cond, &mutex); /* Attende che il master-thread faccia leggere le informazioni in 'ult_info' */
		write( *(int*)fd, &ult_info.info, sizeof(struct sockaddr_in)); /* Invia le informazioni */
		cont_thread_info++; /* Incrementa il contatore dei thread che hanno inviato le informazioni */
		pthread_mutex_unlock(&mutex); /* Rilascia il mutex */
		pthread_cond_broadcast(&cond2); /* Risveglia il master thread */
	}
}
