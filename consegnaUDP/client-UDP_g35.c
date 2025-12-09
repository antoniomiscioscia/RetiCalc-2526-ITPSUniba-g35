/* Inclusioni specifiche per sockets:
   - Su Windows si usa winsock.h
   - Su sistemi Unix-like si includono sys/socket.h, arpa/inet.h, unistd.h
   - defines closesocket come close su Unix per compatibilita'
*/
#if defined (_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "ws2_32.lib")

#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h> /* per gethostbyname */
#define closesocket close
#endif

/*
  Client UDP per la calcolatrice: invia richiesta di operazione al server
  e riceve i messaggi di controllo e il risultato. Vedi la descizione nel server.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>


/* Costanti usate dall'applicazione */
#define PORT 48000          /* porta del server UDP */
#define ECHOMAX 255         /* dimensione massima dei messaggi di testo */
#define EXIT_STRING "TERMINE PROCESSO CLIENT" /* stringa di terminazione */

/* Stampa un messaggio di errore passato come stringa */
void ErrorHandler(char *errorMessage) 
{
    printf("%s", errorMessage);
}

/* Pulisce le risorse winsock su Windows (no-op su Unix) */
void ClearWinSock() 
{
#if defined (_WIN32)
    WSACleanup();
#endif
}

int main(void) 
{
    /* Inizializzazione Winsock (solo Windows): WSAStartup deve essere chiamato prima
       di usare le socket su Windows. Su Unix questa sezione viene ignorata. */
#if defined (_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) 
    {
        ErrorHandler("Errore in WSAStartup()\n");
        return EXIT_FAILURE;
    }
#endif

    /* Variabili principali del client */
    int sock;                           /* descrittore della socket UDP */
    struct sockaddr_in echoServAddr;    /* indirizzo del server (IP + porta) */
    struct sockaddr_in fromAddr;        /* indirizzo del mittente dei pacchetti ricevuti */
    unsigned int fromSize;              /* lunghezza della struttura fromAddr */
    char operation_char;                /* carattere che indica l'operazione richiesta */
    char response_string[ECHOMAX];      /* buffer per le stringhe ricevute dal server */
    int respStringLen;                  /* lunghezza ricevuta dal recvfrom */
    char serverName[ECHOMAX];           /* nome o IP del server inserito dall'utente */
    struct hostent *host;               /* risultato di gethostbyname() */

    /* Richiesta del nome o indirizzo del server all'utente */
    printf("Inserisci il nome del server (es. localhost, 127.0.0.1):\n");
    scanf("%s", serverName);

    /* Risoluzione DNS (o conversione nome -> indirizzo) */
    if ((host = gethostbyname(serverName)) == NULL) //vedi descrizione nella parte server
    {
        /* Se la risoluzione fallisce, si puliscono le risorse ed esce */
        fprintf(stderr, "Risoluzione del nome fallita per %s.\n", serverName);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Creazione della socket UDP: PF_INET + SOCK_DGRAM + IPPROTO_UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) 
    {
        ErrorHandler("socket() fallita\n");
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Costruzione della struttura indirizzo del server (IP + porta) */
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;                          /* IPv4 */
    echoServAddr.sin_port = htons(PORT);                        /* porta in network byte order */
    echoServAddr.sin_addr = *(struct in_addr *)host->h_addr_list[0]; /* primo indirizzo risolto */

    /* Messaggio informativo all'utente su dove verra' inviato il pacchetto */
    printf("Invio al server IP: %s sulla porta %d...\n", inet_ntoa(echoServAddr.sin_addr), PORT);

    /* Lettura dell'operazione da inviare: A,S,M,D */
    printf("Inserisci l'operazione (A=Addizione, S=Sottrazione, M=Moltiplicazione, D=Divisione):\n");
    scanf(" %c", &operation_char); /* spazio prima di %c per saltare whitespace */

    /* Invio del singolo carattere che indica l'operazione al server con sendto() */

    /* FUNZIONE SENDTO:
    La funzione sendto invia dati su una socket non connessa (UDP). La funzione serve a leggere mediante la propria socket 
    dal buffer (in questo caso occorre inviare un carattere) una quantità di dati da inviare sulla socket target all’indirizzo 
    di dimensione sizeof(echoServAddr). Di conseguenza gli argomenti che passa sono: la socket di riferimento, il buffer,
    la dimensione del messaggio (1 byte), le opzioni (0 in questo caso), l'indirizzo del destinatario e la dimensione di tale indirizzo.
    */
    if (sendto(sock, &operation_char, 1, 0,
               (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != 1) 
    {
        /* In caso di errore nell'invio, chiude e termina */
        ErrorHandler("sendto() fallita invio operazione\n");
        closesocket(sock);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Ricezione della stringa di risposta dal server (operazione riconosciuta o terminazione)
       recvfrom restituisce il numero di byte ricevuti o -1 in caso di errore. */
    fromSize = sizeof(fromAddr);

    //FUNZIONE RECVFROM: vedi parte server rigo 119
    respStringLen = recvfrom(sock, response_string, ECHOMAX - 1, 0,
                             (struct sockaddr *)&fromAddr, &fromSize);

    if (respStringLen < 0) 
    {
        /* Errore in ricezione */
        ErrorHandler("recvfrom() fallita ricezione stringa operazione\n");
        closesocket(sock);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Assicura il terminatore di stringa */
    response_string[respStringLen] = '\0';

    /* Validazione della provenienza: confronta IP del server atteso con il mittente reale
       Questo serve a evitare di accettare pacchetti UDP da sorgenti non previste. */
    if (echoServAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr) 
    {
        fprintf(stderr, "Errore: pacchetto ricevuto da sorgente non riconosciuta.\n");
        closesocket(sock);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Mostra la risposta del server */
    printf("Risposta del server: %s\n", response_string);

    /* Se il server ha risposto con la stringa di terminazione, si chiude il client */
    if (strcmp(response_string, EXIT_STRING) == 0) 
    {
        printf("Ricevuta indicazione di terminazione dal server. Chiusura del client.\n");
    } 
    
    else 
    {
        /* Altrimenti la procedura continua: il client chiede due interi e li invia al server */
        long op1, op2;
        printf("Inserisci due interi (separati da spazio): ");
        if (scanf("%ld %ld", &op1, &op2) != 2) 
        {
             ErrorHandler("Input non valido, terminazione.\n");
             closesocket(sock);
             ClearWinSock();
             return EXIT_FAILURE;
        }

        /* Converte gli operandi in formato network (htonl) e li invia come array di 2 int */
        int operands[2];
        operands[0] = htonl((int)op1); /* cast esplicito a int per la trasmissione */
        operands[1] = htonl((int)op2);

        /* Invio dei due interi con sendto; si inviano sizeof(int)*2 byte */
        //FUNZIONE SENDTO: come sopra
        if (sendto(sock, (char *)operands, sizeof(int) * 2, 0,
                   (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != sizeof(int) * 2) 
        {
            ErrorHandler("sendto() fallita invio operandi\n");
            closesocket(sock);
            ClearWinSock();
            return EXIT_FAILURE;
        }

        /* Ricezione del risultato: atteso un long in network byte order */
        long net_result;
        fromSize = sizeof(fromAddr);
        //FUNZIONE RECVFROM: come sopra
        respStringLen = recvfrom(sock, (char *)&net_result, sizeof(long), 0,
                                 (struct sockaddr *)&fromAddr, &fromSize);

        if (respStringLen != sizeof(long)) 
        {
            ErrorHandler("recvfrom() fallita o dimensione risultato errata\n");
            closesocket(sock);
            ClearWinSock();
            return EXIT_FAILURE;
        }

        /* Verifica che il pacchetto risultato arrivi dal server atteso */
        if (echoServAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr) 
        {
            fprintf(stderr, "Errore: pacchetto risultato ricevuto da sorgente non riconosciuta.\n");
            closesocket(sock);
            ClearWinSock();
            return EXIT_FAILURE;
        }

        /* Conversione del risultato da network a host order e stampa */
        long result = ntohl(net_result);
        printf("Risultato ricevuto dal server: %ld\n", result);
    }

    /* Pulizia finale: chiude la socket e pulisce le risorse di Winsock su Windows */
    closesocket(sock);
    ClearWinSock();
    system ("pause");
    return EXIT_SUCCESS;
}