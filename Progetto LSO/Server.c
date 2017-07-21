/* Torretta */

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
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/* Definizione costante */
#define MAX_THREAD 500

/* Struttura che contiene le mie informazioni */
typedef struct postazione
{
	int id;
	int posizione[2];
	int numero_missili;
	int raggio;
}postazione;

/* Dichiarazione variabili globali */
postazione pos;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_esito = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_esito = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;

int ult_info[3]; /* Variabile che attende l'esito del bersaglio: [0] = id_bersaglio, [1] = coordinata X, [2] = coordinata Y */
int cont_server_connessi = 1; /* Conta il numero di Server connessi */
int cont_server_pos = 0; /* Conta il numero dei server a cui è stata inviata la posizione del bersaglio */
int **lista_candidati; /* Contiene le risposte dei Server: [0] = id_server, [1] = risultato algoritmo che decide il bersaglio da colpire */
int destino[2]; /* Contiene l'esito dell'ultimo bersaglio */
int dim = 100; /* Dimensione di 'lista_candidati' */

/* Prototipi */
void *comunicazione_server(void *fd);
void *accetta_server(void *fd);
void *accetta_bersaglio(void *fd);
void *lettura_rend(void *fd);
void *gestione_bersaglio(void *fd);
int esito(int x, int y, int r);
void *gestisci_server(void* fd);
void *bersaglio_avvistato(void* fd);
void stampa_esito(int id_bersaglio, int id_server, int esito);

/* Parametri: (1) ID (2) IP:Porta Rendezvous (3) X,Y (4) Numero missili (5) Raggio (6) Porta Server*/
int main(int argc, char **argv)
{
	int sock_rendezvous, sock_server, sock_client, *connect_server = NULL, port[2], i, lung_server = sizeof(struct sockaddr_in);
	struct sockaddr_in rendezvous; /* Contiene IP e numero di porta del Server di Rendezvous */
	struct sockaddr_in listen_server; /* Contiene i dati per la connessione dei Server */
	struct sockaddr_in listen_client; /* Contiene i dati per la connessione dei Client */
	struct sockaddr_in ricevuto; /* Contiene le informazioni sui Server inviati dal Rendezvous */
	char *ip_rend, *porta_rend, stampa_ultimo[100], esito_connessione[3]="\0";

	pthread_t th, th2, th3;
	sigset_t set;

	/* Blocca il segnale SIGPIPE in caso di disconnessione di un Server */
	sigaddset(&set,SIGPIPE);
	sigprocmask(SIG_SETMASK,&set,NULL);

    /* Controlla il numero dei parametri da riga di comando */
	if( argc < 7)
	{
		write( STDERR_FILENO, "\nNon sono stati inseriti tutti i parametri da riga di comando\n", strlen("\nNon sono stati inseriti tutti i parametri da riga di comando\n"));
		exit(EXIT_FAILURE);
	}

	/* Inzializza i dati della postazione */
	pos.id = atoi(argv[1]); /* Estrae ID da riga di comando */
	ip_rend = strtok(argv[2], ":\n"); /* Estrae IP da riga di comando */
	porta_rend = strtok(NULL, ":\n"); /* Estrae il numero di porta da riga di comando */
	pos.posizione[0] = atoi(strtok(argv[3],",\n")); /* Estrae la coordinata x da riga di comando */
	pos.posizione[1] = atoi(strtok(NULL,",\n")); /* Estrae la coordinata y da riga di comando */
	pos.numero_missili = atoi(argv[4]); /* Estrae il numero dei missili da riga di comando */
	pos.raggio = atoi(argv[5]); /* Estrae il range da riga di comando */
    port[1] = atoi(argv[1]);

    /* Controlla se l'ID è maggiore di 0 */
	if( pos.id < 1 )
	{
		write( 1, "\nL'ID deve essere maggiore di 0\n", strlen("\nL'ID deve essere maggiore di 0\n") );
		exit(EXIT_FAILURE);
	}

    /* Creazione socket per la connessione con il Server di Rendezvous */
	if(( sock_rendezvous=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		perror("\nErrore creazione socket Rendezvous");
		exit(EXIT_FAILURE);
	}

    /* Creazione socket per la connessione ai Server */
	if(( sock_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		perror("\nErrore creazione socket Server");
		exit(EXIT_FAILURE);
	}

    /* Creazione socket per la connessione al Client */
	if(( sock_client=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		perror("\nErrore creazione socket Client");
		exit(EXIT_FAILURE);
	}

	/* Inizializza la struttura sockaddr_in per la connessione al Server di Rendezvous*/
	rendezvous.sin_addr.s_addr = inet_addr(ip_rend);
    rendezvous.sin_family = AF_INET;
    rendezvous.sin_port = htons(atoi(porta_rend));

    /* Inizializza la struttura sockaddr_in per accettare le connessioni da altri Server */
    listen_server.sin_family = AF_INET;
	listen_server.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Inizializza la struttura sockaddr_in per accettare le connessioni dai Client */
    listen_client.sin_family = AF_INET;
	listen_client.sin_port = htons(atoi(argv[6]));
	listen_client.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Listen senza bind per avere una porta libera */
	if( listen(sock_server, MAX_THREAD) == -1 )
	{
		perror("\nErrore listen client");
		pthread_exit(NULL);
	}

	/* Si evita di indicare la porta in ascolto per un altro Server, inviandolo direttamente al Rendezvous*/
	if(getsockname(sock_server, (struct sockaddr *)&listen_server,(socklen_t *)&lung_server)<0)
	{
		write( 1, "\nErrore getsockname", strlen("\nErrore getsockname") );
		exit(EXIT_FAILURE);
	}

	port[0] = htons(listen_server.sin_port); /* Inserisce la porta in una stringa */

    /* Bind Client */
	if( (bind (sock_client, (struct sockaddr *)&listen_client, sizeof(listen_client))) == -1 )
	{
		perror("\nErrore bind client");
		exit(EXIT_FAILURE);
	}

	/* Connessione al Server di Rendezvous */
	if (connect(sock_rendezvous , (struct sockaddr *)&rendezvous , sizeof(rendezvous)) < 0)
	{
		perror("\nErrore connessione Server di Rendezvous");
		exit(EXIT_FAILURE);
	}

	write( 1, "\nConnesso al Server di Rendezvous\n", strlen("\nConnesso al Server di Rendezvous\n") );
	write( sock_rendezvous, port, 2*sizeof(int)); /* Invia ID e numero di porta al Server di rendezvous */
	read( sock_rendezvous, esito_connessione,3);
	esito_connessione[2]='\0';

	/* Se il Rendezvous rifiuta la connessione */
	if(strcmp(esito_connessione,"NOT") == 0)
	{
		write( 1, "\nID appartenente ad un altro Server\n", strlen("\nID appartenente ad un altro Server\n") );
		exit(EXIT_FAILURE);
	}

    /* Creazione thread che gestisce le connessioni */
	if((pthread_create(&th ,NULL,(void*)accetta_server,&sock_server)) == -1 )
	{
		perror("\nErrore accetta_server");
		exit(EXIT_FAILURE);
	}

    /* Attende finchè non riceve end_of_line */
	while(read(sock_rendezvous, &ricevuto, sizeof(struct sockaddr_in))>0 && ricevuto.sin_port!=0)
    {
		connect_server = malloc(sizeof(int));

		/* Creazione socket per la connessione al Server */
		if(( *connect_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 )
		{
			perror("\nErrore creazione socket Server");
			continue;
		}

		/* Connessione ad un Server della lista */
		if (connect(*connect_server , (struct sockaddr *)&ricevuto , sizeof(ricevuto)) < 0)
		{
			perror("\nErrore connessione al Server");
			continue;
		}

		/* Crea un thread con ogni Server in comunicazione */
        if((pthread_create( &th, NULL, (void*) &gestisci_server, connect_server)) == -1)
		{
			perror("\nErrore pthread_create");
			continue;
		}

		sprintf( stampa_ultimo ,"\nParametri ricevuti sul Server %s\n",inet_ntoa(ricevuto.sin_addr) );
		write( 1, stampa_ultimo, strlen(stampa_ultimo) ); /* Visualizza IP e numero di porta */
        cont_server_connessi++; /* Incrementa il contatore dei Server connessi */
	}

    dim = cont_server_connessi + 100; /* Aggiunge in 'dim' lo spazio sufficiente per ricevere le informazioni dai Server */
    lista_candidati =(int**) malloc ( dim * sizeof( int *));

    for( i=0; i<dim ; i++)
		lista_candidati[i] = malloc(sizeof(int)*2);

    /* Thread per gestire le connessioni ai bersagli */
	if((pthread_create(&th2 ,NULL,(void*)accetta_bersaglio, &sock_client)) == -1 )
	{
		perror("\nErrore accetta_bersaglio");
		exit(EXIT_FAILURE);
	}

	/* Thread creato per gestire i messaggi del Rendezvous sui nuovi Server connessi */
	if((pthread_create(&th3 ,NULL,(void*)lettura_rend, &sock_rendezvous)) == -1)
	{
		perror("\nErrore sock_rendezvous");
		exit(EXIT_FAILURE);
	}

	/* Attende che terminano gli altri thread */
	pthread_join(th,NULL);
	pthread_join(th2,NULL);
	pthread_join(th3,NULL);
}

/* Funzione che accetta connessioni da altri Server */
void *accetta_server(void *fd)
{
	int *accept_fd;
	pthread_t th;
	struct sockaddr_in server;
	int lung_server = sizeof(struct sockaddr_in);

	while(1)
	{
		accept_fd = malloc(sizeof(int));

		if((*accept_fd = accept(*(int*)fd, (struct sockaddr *)&server,(socklen_t*)&lung_server)) == -1 )
		{
			write( 1, "\nErrore accept server\n", strlen("\nErrore accept server\n") );
			continue;
        }

        if( (pthread_create( &th, NULL, (void*)bersaglio_avvistato,(void*) accept_fd)) == -1)
            write( 1, "\nErrore connessione Server\n", strlen("\nErrore connessione Server\n") );
    }
    return NULL;
}

/* Funzione che notifica l'attacco di una torretta con l'esito del bersaglio */
void *bersaglio_avvistato(void *fd)
{
	int sock = *(int*)fd, dati_ricevuti[3], n, controllo, risultato[2], destino[2];
	char stampa_bersaglio[100], status[100], id[100];
	struct timespec t;  /* Struttura usata da timedlock */

	free(fd);
	risultato[0] = pos.id;

	while(1)
	{
		n=read( sock, dati_ricevuti, sizeof(int)*3 );

		/* Verifica se il Server in comunicazione è attivo */
		if( n != 0 )
		{
			sprintf(stampa_bersaglio,"\nBersaglio %d in posizione (%d,%d)\n",dati_ricevuti[0],dati_ricevuti[1],dati_ricevuti[2]);
			write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
            clock_gettime(CLOCK_REALTIME, &t);
			t.tv_sec += 2;

			/* Cerca di acquisire il lock entro un certo intervallo di tempo */
			controllo = pthread_mutex_timedlock(&mutex2, &t);

			/* Se il mutex2 è stato acquisito */
			if( controllo == 0 )
			{
				risultato[1] = esito(dati_ricevuti[1],dati_ricevuti[2],pos.raggio); /* Calcola l'esito del bersaglio */
				write(sock, risultato, sizeof(int)*2); /* Invia l'esito al Server */
				read(sock, destino, sizeof(int)*2); /* Legge il destino del bersaglio */

				/* Controlla se i parametri sono sufficienti per abbattere il bersaglio */
				if( destino[0]==pos.id && pos.numero_missili > 0 )
					pos.numero_missili--; /* Decrementa il numero dei missili */

				pthread_mutex_unlock(&mutex2); /* Rilascia il mutex */
			}
			/* Altrimenti notifica il Server che non è disponibile ad attaccare il bersaglio */
			else
			{
				risultato[1] = -2;
				write(sock, risultato, sizeof(int)*2);
				read(sock, destino, sizeof(int)*2);
			}

			/* Stampa l'esito del bersaglio */
			stampa_esito(dati_ricevuti[0],destino[0],destino[1]);
			sprintf(status,"\nNumero missili Torretta %d: %d\n",destino[0],pos.numero_missili);
			write( 1, status, strlen(status) );
		}
		/* Altrimenti visualizza un messaggio se la connessione è stata persa con il Server */
		else
		{
			write( 1, "\nUna torretta si e' disconnessa\n", strlen("\nUna torretta si e' disconnessa\n") );
			close(sock);
			pthread_exit(NULL);
		}
	}
}

/* Funzione che gestisce la comunicazione con altri Server con l'esito dei Server candidati */
void *gestisci_server(void* fd)
{
	int sock = *(int*)fd, n, ricevuto[2];

	while(1)
	{
		pthread_cond_wait(&cond,&mutex); /* Thread sbloccato da 'gestione_client' */
		write(sock, ult_info,sizeof(int)*3); /* Manda i parametri sul bersaglio al server di cui gestisce il file descriptor */
		n=read(sock, ricevuto, sizeof(int)*2); /* Legge l'esito e lo inserisce nella lista dei candidati */

		if( n == 0 )
		{
			cont_server_connessi--;
			pthread_mutex_unlock(&mutex);
            pthread_cond_broadcast(&cond2);
			close(sock);
			write( 1, "\nConnessione persa col Server\n", strlen("\nConnessione persa col Server\n") );
			pthread_exit(NULL);
		}

		/* Copia dati ricevuti dalla lista */
		lista_candidati[cont_server_pos][0]=ricevuto[0];
		lista_candidati[cont_server_pos][1]=ricevuto[1];
		cont_server_pos++;

		pthread_mutex_unlock(&mutex); /* Rilascia il mutex per poter scrivere nella lista */
		pthread_cond_broadcast(&cond2); /* Notifica il thread 'gestione_bersaglio' che ha scritto nella lista */
		pthread_cond_wait(&cond_esito,&mutex_esito); /* Attende che 'gestione_server' calcoli l'esito */
		write(sock, destino, sizeof(int)*2); /* Invia l'esito */
		cont_server_pos++; /* Indica che ha inviato l'esito al Server in comunicazione */
        pthread_mutex_unlock(&mutex_esito); /* Rilascia il mutex per consentire ad altri di inviare l'esito */
        pthread_cond_broadcast(&cond2); /* Segnala a 'gestione_bersaglio' che ha inviato l'esito */
	}
}

/* Ascolta sulla porta dei missili e li gestisce */
void *accetta_bersaglio(void *fd)
{
	int *accept_fd;
	pthread_t th;
	struct sockaddr_in client;
	int lung_client = sizeof(struct sockaddr_in);

	if( listen(*(int*)fd, MAX_THREAD) == -1 )
	{
		perror("\nErrore listen client");
		pthread_exit(NULL);
	}

	while(1)
	{
		accept_fd = malloc(sizeof(int));

		if((*accept_fd = accept(*(int*)fd, (struct sockaddr *)&client,(socklen_t*)&lung_client)) < 0 )
		{
			write( 1, "\nErrore accept client\n", strlen("\nErrore accept client\n") );
			continue;
		}

		/* Crea thread per la gestione del bersaglio */
		if((pthread_create( &th, NULL, (void*)gestione_bersaglio, accept_fd)) == -1 )
			write( 1, "\nErrore gestione missile\n", strlen("\nErrore gestione missile\n") );
	}
	return NULL;
}

/* Gestione client */
void *gestione_bersaglio(void *fd)
{
	int sock = *(int*)fd, dati_bersaglio[3], i, max = -1, id_vincitore = -1, risultato;
	char stampa_bersaglio[100], status[100];

	/* Legge i dati inviati dal Client secondo un formato prestabilito [0] = id_bersaglio, [1] = coordinata X, [2] = coordinata Y */
	if( (read( sock, dati_bersaglio, sizeof(int)*3)) < 0)
	{
		perror("\nErrore lettura dati client");
		pthread_exit(NULL);
	}

	pthread_mutex_lock(&mutex2); /* Acquisisce il lock per gestire un missile alla volta */
	pthread_mutex_lock(&mutex); /* Acquisisce il lock per modificare  'ult_info' */
    sprintf(stampa_bersaglio,"\nBersaglio %d da colpire in posizione (%d,%d)\n",dati_bersaglio[0],dati_bersaglio[1],dati_bersaglio[2]);
	write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );

	/* Inserisce nella variabile globale le informazioni del missile */
	ult_info[0] = dati_bersaglio[0]; /* ID */
	ult_info[1] = dati_bersaglio[1]; /* Coordinata x */
	ult_info[2] = dati_bersaglio[2]; /* Coordinata y */
	cont_server_pos = 0;

	/* Esegue l'algoritmo e l'inserimento in cima alla 'lista_candidati' ed incremento di 'cont_server_pos' */
	risultato = esito( dati_bersaglio[1], dati_bersaglio[2], pos.raggio);
	lista_candidati[0][0] = pos.id;
	lista_candidati[0][1] = risultato;
	cont_server_pos++;
    pthread_mutex_unlock(&mutex);
	pthread_cond_broadcast(&cond); /* Risveglia tutti i thread in attesa */

	/* Attende che tutti abbiano inviato il messaggio con l'esito in 'lista_candidati' */
	while( cont_server_pos < cont_server_connessi)
        pthread_cond_wait(&cond2,&mutex);

    /* Scandisce la lista dei candidati per determinare l'esito del bersaglio */
	for ( i = 0; i < cont_server_connessi; i++ )
	{
		if( lista_candidati[i][1] > max )
		{
			id_vincitore = lista_candidati[i][0];
			max = lista_candidati[i][1];
		}
	}

	/* Scrive il destino del bersaglio in una variabile globale */
	destino[0] = id_vincitore;
	destino[1] = max;

	cont_server_pos = 1; /* Visualizza 1 perchè ha letto il destino del bersaglio */
    pthread_cond_broadcast(&cond_esito); /* Consente a tutti i thread di inviare il destino del bersaglio */

    /* Attende che tutti abbiano inviato il destino del bersaglio ai Server */
	while( cont_server_pos < cont_server_connessi)
        pthread_cond_wait(&cond2,&mutex_esito);

	if(id_vincitore == pos.id && pos.numero_missili>0)
		pos.numero_missili--;

	write( sock, destino, sizeof(int)*2 ); /* Invia al Client il suo destino */
	stampa_esito(dati_bersaglio[0], id_vincitore, max); /* Stampa l'esito del bersaglio */

	/* Stampa lo stato */
	sprintf(status,"\nNumero missili Torretta %d: %d\n",pos.id,pos.numero_missili);
	write( 1, status, strlen(status) );

	pthread_mutex_unlock(&mutex2); /* Rilascia il mutex */
	pthread_mutex_unlock(&mutex_esito); /* Rilascio il mutex che conferma l'esito da parte di tutti */
    close(sock); /* Chiude il socket di comunicazione con il Client */

	pthread_exit(NULL);
}

/* Funzione che riceve le informazioni sui Server connessi */
void *lettura_rend(void *fd)
{
	int sock_rend = *(int*)fd, n, i, *sock;
	struct sockaddr_in ricevuto;
	char stampa_mess[100];
	pthread_t th;

	while( (n=read(sock_rend , &ricevuto, sizeof(struct sockaddr_in))) > 0 )
	{
		sprintf( stampa_mess ,"\nServer %s connesso\n",inet_ntoa(ricevuto.sin_addr));
		sock = malloc(sizeof(int));
		*sock = socket(AF_INET, SOCK_STREAM, 0);

		/* Connessione ad un Server appena connesso */
		if (connect(*sock , (struct sockaddr *)&ricevuto , sizeof(ricevuto)) < 0)
		{
			perror("\nErrore connessione Server");
			continue;
		}

		pthread_mutex_lock(&mutex);

		/* Se è stato raggiunto il limite, si deve aumentare la dimensione della lista */
		if( cont_server_connessi == dim )
		{
            dim = dim*2; /* La dimensione viene raddoppiata */
            lista_candidati=(int**) realloc ( lista_candidati,dim * sizeof( int *) );

            for( i=(dim/2) ; i<dim; i++)
				lista_candidati[i] = malloc(sizeof(int)*2);
        }

		/* Thread creato per gestire la connessione con un Server appena connesso */
		if((pthread_create(&th ,NULL,(void*)gestisci_server,(void*)sock)) == -1 )
		{
			perror("\nErrore accetta_server");
			pthread_mutex_unlock(&mutex);
			continue;
		}
		else
		{
			if( n==sizeof(struct sockaddr_in))
				write( 1, stampa_mess, strlen(stampa_mess) );

			cont_server_connessi++;
			pthread_mutex_unlock(&mutex);
		}
	}
	write( 1, "\nConnessione persa con il Server di Rendezvous\n", strlen("\nConnessione persa con il Server di Rendezvous\n") );
	close(sock_rend);

	pthread_exit(NULL);
}

/* Funzione che calcola l'esito di un bersaglio */
int esito(int x, int y, int r)
{
	if((sqrt((pow(x-pos.posizione[0],2)) + (pow(y-pos.posizione[1],2)))) <= r )
		return pos.numero_missili;
    return -1;
}

/* Funzione che visualizza l'esito del bersaglio */
void stampa_esito(int id_bersaglio, int id_server, int esito)
{
	char stampa_bersaglio[100];

	if( esito == -1)
	{
		sprintf( stampa_bersaglio,"\nBersaglio %d non raggiungibile da nessuna Torretta\n", id_bersaglio);
		write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
	}
	else if( esito == 0 )
	{
		sprintf( stampa_bersaglio,"\nBersaglio %d non abbattibile da nessuna Torretta attiva\n", id_bersaglio);
		write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
	}
	else
	{
		sprintf( stampa_bersaglio,"\nBersaglio %d abbattuto dalla Torretta %d\n", id_bersaglio,id_server);
		write( 1, stampa_bersaglio, strlen(stampa_bersaglio) );
	}
}
