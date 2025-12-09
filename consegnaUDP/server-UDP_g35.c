#if defined (_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "ws2_32.lib")

#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
#endif

/*
  Server UDP per la calcolatrice: riceve l'operazione richiesta dal client,
  invia una conferma (o la stringa di terminazione), riceve gli operandi,
  calcola il risultato e lo invia indietro.
*/

/*
L'uso delle funzioni caratterizzanti in questo caso è volto all'implementazione
di un servizio, quale UDP, che non richiede connessione stabile tra client e server.
Per quanto riguarda dunque le funzioni riguardanti la creazione della socket, il
funzionamento rimane simile a quello di una socket TCP. La differenza sostanziale
sta nell'uso delle funzioni di invio e ricezione dei dati.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>


/* Inclusioni specifiche per sockets:
   - Su Windows si usa winsock.h
   - Su Unix-like si includono sys/socket.h, arpa/inet.h, unistd.h
   - Definiamo closesocket -> close su Unix per compatibilita' con il codice Windows
*/


/* Costanti di configurazione */
#define PORT 48000                 /* porta su cui il server UDP ascolta */
#define ECHOMAX 255                /* dimensione massima dei messaggi di testo */
#define EXIT_STRING "TERMINE PROCESSO CLIENT" /* stringa che indica terminazione dal client */

/* Stampa messaggi di errore ricevuti come stringa */
void ErrorHandler(char *errorMessage) 
{
    printf("%s", errorMessage);
}

/* Pulisce le risorse di Winsock su Windows; no-op su Unix */
void ClearWinSock() 
{
#if defined (_WIN32)
    WSACleanup();
#endif
}

int main(int argc, char *argv[]) 
{
    /* Inizializzazione Winsock (solo Windows): chiamare WSAStartup prima di usare le socket */
#if defined (_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) 
    {
        ErrorHandler("Errore in WSAStartup()\n");
        return EXIT_FAILURE;
    }
#endif

    /* Variabili principali */
    int sock;                            /* descrittore della socket UDP */
    struct sockaddr_in echoServAddr;     /* indirizzo del server (local bind) */
    struct sockaddr_in echoClntAddr;     /* indirizzo del client che invia pacchetti */
    unsigned int cliAddrLen;             /* dimensione della struttura client */
    char operation_char;                 /* operazione richiesta (carattere) */
    char response_string[ECHOMAX];       /* buffer per le stringhe di risposta */
    int operands[2];                     /* buffer per gli operandi inviati dal client */
    int recvMsgSize;                     /* numero di byte ricevuti da recvfrom */
    long result;                         /* risultato dell'operazione */

    /* Creazione della socket UDP: PF_INET, SOCK_DGRAM, IPPROTO_UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) 
    {
        ErrorHandler("socket() fallita\n");
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Costruzione della struttura indirizzo su cui fare bind (localhost:PORT) */
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;                /* IPv4 */
    echoServAddr.sin_port = htons(PORT);              /* porta in network byte order */
    echoServAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* ascolta solo su localhost */

    /* Bind della socket all'indirizzo locale */
    if (bind(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) 
    {
        ErrorHandler("bind() fallito\n");
        closesocket(sock);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    /* Notifica che il server e' pronto */
    printf("Server UDP in ascolto sulla porta %d...\n", PORT);

    /* Ciclo infinito di ricezione datagram: il server rimane attivo */
    while (1) 
    {
        cliAddrLen = sizeof(echoClntAddr);

        /* Prima ricezione: attendiamo il singolo byte che indica l'operazione richiesta
           Questa chiamata e' bloccante fino a che non arriva un datagram. */
        recvMsgSize = recvfrom(sock, &operation_char, 1, 0,
                               (struct sockaddr *)&echoClntAddr, &cliAddrLen);

        /* FUNZIONE RECVFROM:
        La funzione serve a scrivere mediante la propria socket in quanto interfaccia software su un buffer un messaggio ricevuto 
        (in questo caso un carattere) da un indirizzo mittente di dimensione sizeof(echoClntAddr). Di conseguenza gli argomenti che passa sono:
        la socket di riferimento, il buffer dove scrivere il messaggio ricevuto, la dimensione del messaggio (1 byte),
        le opzioni (0 in questo caso), l'indirizzo del mittente e la dimensione di tale indirizzo.
        */

        if (recvMsgSize < 0) 
        {
            /* Se c'e' un errore di ricezione, segnala e continua il ciclo */
            ErrorHandler("recvfrom() fallita\n");
            continue;
        }

        /* Informazione su quale client abbiamo appena ricevuto */
        printf("\nGestione client %s\n", inet_ntoa(echoClntAddr.sin_addr));

        /* Determina quale operazione e prepara la stringa di risposta */
        bool valid_operation = true;
        switch (operation_char) 
        {
            case 'A': case 'a':
                strcpy(response_string, "ADDIZIONE");
                break;
            case 'S': case 's':
                strcpy(response_string, "SOTTRAZIONE");
                break;
            case 'M': case 'm':
                strcpy(response_string, "MOLTIPLICAZIONE");
                break;
            case 'D': case 'd':
                strcpy(response_string, "DIVISIONE");
                break;
            default:
                /* Carattere non riconosciuto: chiediamo al client di terminare */
                strcpy(response_string, EXIT_STRING);
                valid_operation = false;
                break;
        }

        /* Stampa diagnostica e invio della stringa di conferma/terminazione al client */
        printf("Ricevuta op: '%c', Invio indietro: '%s'\n", operation_char, response_string);

        //FUNZIONE SENDTO: vedi parte client rigo 111
        if (sendto(sock, response_string, strlen(response_string) + 1, 0,
                   (struct sockaddr *)&echoClntAddr, cliAddrLen) != (int)(strlen(response_string) + 1)) 
        {
            ErrorHandler("sendto() fallita invio stringa operazione\n");
            /* Non usciamo; possiamo continuare a servire altri client */
        }

        /* Se l'operazione e' valida, attendiamo il pacchetto successivo contenente gli operandi */
        if (valid_operation) 
        {
            recvMsgSize = recvfrom(sock, (char *)operands, sizeof(int) * 2, 0,
                                   (struct sockaddr *)&echoClntAddr, &cliAddrLen);
            //FUNZIONE RECVFROM: come sopra

            /* Controllo che la dimensione ricevuta sia corretta (due int) */
            if (recvMsgSize != (int)(sizeof(int) * 2)) 
            {
                ErrorHandler("recvfrom() fallita o dimensione operandi errata\n");
                continue; /* torna a ricevere una nuova richiesta */
            }

            /* Convertiamo gli operandi da network byte order a host order prima dell'operazione */
            long op1 = ntohl(operands[0]);
            long op2 = ntohl(operands[1]);

            /* Calcolo dell'operazione richiesta */
            switch (operation_char) 
            {
                case 'A': case 'a': result = op1 + op2; break;
                case 'S': case 's': result = op1 - op2; break;
                case 'M': case 'm': result = op1 * op2; break;
                case 'D': case 'd':
                    if (op2 != 0)
                        result = op1 / op2;
                    else {
                        result = 0; /* gestione semplice divisione per zero */
                        printf("Errore: divisione per zero.\n");
                    }
                    break;
            }

            /* Stampa diagnostica del calcolo effettuato */
            printf("Calcolo: %ld %c %ld = %ld\n", op1, operation_char, op2, result);

            /* Invia il risultato al client in network byte order */
            long net_result = htonl(result);
            if (sendto(sock, (char *)&net_result, sizeof(long), 0,
                       (struct sockaddr *)&echoClntAddr, cliAddrLen) != (int)sizeof(long)) 
            {
                ErrorHandler("sendto() fallita invio risultato\n");
            }
        }
    }

    /* Non si arriva mai qui in un server che gira indefinitamente, ma chiudiamo per correttezza */
    closesocket(sock);
    ClearWinSock();
    system ("pause");
    return EXIT_SUCCESS;
}