// Inclusioni specifiche per Sockets (come da slide)
#if defined (_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "ws2_32.lib")

#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close   // Mappa closesocket su close per sistemi Unix [cite: 204, 205]
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// Costanti
#define PROTOPORT 48000                           // Porta di default per l'applicazione
#define ECHOMAX 255                               // Dimensione massima del buffer di echo
#define EXIT_STRING "TERMINE PROCESSO CLIENT"     // Stringa di terminazione
#define CONNECT_OK_STRING "connessione avvenuta"  // Stringa di conferma connessione

void ErrorHandler(char *errorMessage) 
{   // Funzione di gestione errori
    printf("%s", errorMessage); 
}

void ClearWinSock() 
{  // Funzione di pulizia Winsock
    #if defined (_WIN32) 
        WSACleanup();
    #endif
}

// Funzione di utilità per ricevere esattamente la dimensione richiesta
int RecvExact(int sock, char *buf, int len) 
{
    int total_bytes = 0;
    int bytes_rcvd;
    while (total_bytes < len) 
    {
        bytes_rcvd = recv(sock, buf + total_bytes, len - total_bytes, 0);           // Riceve dati dalla socket
        if (bytes_rcvd <= 0) return bytes_rcvd;                                     // Errore o connessione chiusa
        total_bytes += bytes_rcvd;                                                  // Aggiorna il conteggio dei byte totali ricevuti
    }
    return total_bytes;   // Restituisce il numero totale di byte ricevuti
}

int main(void) 
{
    // 1. Inizializzazione Winsock (solo per Windows)
    #if defined (_WIN32)
        WSADATA wsaData;
        if (WSAStartup (MAKEWORD(2,2), &wsaData) != 0) 
        {
            ErrorHandler ("Errore in WSAStartup()\n");
            return EXIT_FAILURE;
        }
    #endif

    int Csocket;                     // Definire una variabile (int) che conterrà il descrittore della socket:
    struct sockaddr_in sad;          //Creare un elemento di tipo sockaddr_in
    char serverName[ECHOMAX];        // Nome del server
    char operation_char;             // Lettera (A/S/M/D)
    char response_string[ECHOMAX];   // Stringa di risposta
    struct hostent *host;            // Per gethostbyname()

    // 2. CLIENT: richiede il nome del server
    printf("Inserisci il nome del server (es. localhost, 127.0.0.1):\n");
    scanf("%s", serverName);

    // 3. CLIENT: risoluzione del nome
    if ((host = gethostbyname(serverName)) == NULL) 
    {   // Necessaria per la risoluzione da nome simbolico a IP, per cui gethostbyname() prende come parametro la stringa del nome del server
        fprintf(stderr, "Risoluzione del nome fallita per %s.\n", serverName);    // Se la risoluzione fallisce, terminare il client
        // In caso di successo, la struttura hostent conterrà l'indirizzo IP del server (motivo per cui lavora con un puntatore a struct hostent)
        ClearWinSock();
        return EXIT_FAILURE;
    }
    // L'indirizzo IP è ora in host->h_addr_list[0] 
    //Nel caso in cui viene fornito l'indirizzo IP direttamente, gethostbyaddress() non è necessario

    // CREAZIONE DELLA SOCKET: vedi funzionamento nella parte server (rigo 86)
    if ((Csocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) 
    {
        ErrorHandler("Creazione della socket fallita.\n");                          
        ClearWinSock();                                                             
        return EXIT_FAILURE;                                                        
    }

    // COSTRUZIONE DELL'INDIRIZZO DEL SERVER
    memset(&sad, 0, sizeof(sad));                          // Inizializza a zero la struttura sockaddr_in
    sad.sin_family = AF_INET;                              // Famiglia di protocolli IPv4
    sad.sin_port = htons (PROTOPORT);                      // Porta in formato Big-Endian (Network Byte Order)
    sad.sin_addr = *(struct in_addr *)host->h_addr_list[0];  // Usa l'IP risolto 

    
    printf("Connessione al server IP: %s sulla porta %d...\n", inet_ntoa(sad.sin_addr), PROTOPORT);

    // 3. CLIENT: richiede la connessione al server

    /*
    Con la funzione connect () si stabilisce una connessione ad una socket specificata,
    passando il descrittore della propria socket, l'indirizzo target e la lunghezza dell'
    indirizzo. Restituisce 0 in caso di successo, altrimenti -1
    */

    if (connect (Csocket, (struct sockaddr *)&sad, sizeof(sad)) < 0) 
    {
        ErrorHandler( "Connessione fallita.\n");                                     
        closesocket(Csocket);    // Se la connessione fallisce, termina il client                                                    
        ClearWinSock(); 
        return EXIT_FAILURE;                                                        
    }

    // 5. CLIENT: riceve la stringa "connessione avvenuta" (usa recv che legge fino ai dati disponibili)
    memset(response_string, 0, ECHOMAX);
    if (recv(Csocket, response_string, ECHOMAX - 1, 0) <= 0) 
    {
        ErrorHandler("recv() fallita o connessione chiusa prematuramente (messaggio connessione).\n");
        closesocket(Csocket);                                                       // Chiude la socket
        ClearWinSock();
        return EXIT_FAILURE;                                                        // Restituisce EXIT_FAILURE in caso di errore
    }
    printf("Server dice: %s\n", response_string);

    // 6. CLIENT: legge una lettera e la invia al server
    printf("Inserisci l'operazione (A=Addizione, S=Sottrazione, M=Moltiplicazione, D=Divisione):\n");
    scanf(" %c", &operation_char);                                                  

    /*
    FUNZIONE SEND (): Invia dati ad una socket connessa: la funzione restituisce il numero di byte trasmessi in caso di successo, 
    altrimenti un valore <= 0. La funzione prenderà in input la propia socket, il buffer coi dati da inviare (tipicamente il buffer da cui leggere), 
    la dimensione del buffer che in questo caso sarà solo un carattere e il flag solitamente posto a 0. Se i byte inviati sono in numero diverso rispetto 
    alla dimensione del buffer, allora il programma dà errore.
    */

    if (send(Csocket, &operation_char, 1, 0) != 1) 
    {   // Invia la lettera al server
        ErrorHandler("send() fallita invio operazione.\n");  // Se l'invio fallisce, termina il client
        closesocket(Csocket); 
        ClearWinSock();
        return EXIT_FAILURE;  
    }

    // 7. CLIENT: riceve la stringa di operazione/terminazione (usa recv che legge fino ai dati disponibili)
    memset(response_string, 0, ECHOMAX); // Pulisce il buffer
    //FUNZIONE RECV (): Vedi funzionamento nella parte server (rigo 159)
    if (recv(Csocket, response_string, ECHOMAX - 1, 0) <= 0) 
    { // Riceve la stringa di risposta
        ErrorHandler("recv() fallita o connessione chiusa prematuramente (stringa operazione).\n");
        closesocket(Csocket);
        ClearWinSock();
        return EXIT_FAILURE;                                                        
    }
    printf("Risposta del server: %s\n", response_string);  // Stampa la risposta del server

    // 8. Logica di terminazione
    if (strcmp(response_string, EXIT_STRING) == 0) 
    {                                
        printf("Ricevuta indicazione di terminazione dal server. Chiusura del client.\n");
    } 

    else 
    {
        // L'operazione è valida, invia i due interi
        long op1, op2;
        printf("Inserisci due interi (separati da spazio): ");
        if (scanf("%ld %ld", &op1, &op2) != 2) 
        {
             ErrorHandler("Input non valido, terminazione.\n");
             closesocket(Csocket);
             ClearWinSock();
             return EXIT_FAILURE;
        }

        // Invia i due operandi: usare uint32_t e inviare 4 byte per valore (htonl/ntohl operano su 32-bit)
        uint32_t operands[2];
        operands[0] = htonl((uint32_t)op1);  // Conversione Host to Network (32-bit)
        operands[1] = htonl((uint32_t)op2);
        
        // 8. CLIENT: spedisce i due interi (2 * sizeof(uint32_t) bytes)
        if (send(Csocket, (char *)operands, sizeof(uint32_t) * 2, 0) != sizeof(uint32_t) * 2) 
        {
            ErrorHandler("send() fallita invio operandi.\n");
            closesocket(Csocket);
            ClearWinSock();
            return EXIT_FAILURE;
        }

        // 10. CLIENT: riceve il risultato (1 * sizeof(uint32_t) bytes)
        uint32_t net_result;
        if (RecvExact(Csocket, (char *)&net_result, sizeof(uint32_t)) <= 0) 
        {
            ErrorHandler("recv() fallita o connessione chiusa prematuramente (risultato).\n");
            closesocket(Csocket);
            ClearWinSock();
            return EXIT_FAILURE;
        }
        
        long result = (long)ntohl(net_result);
        printf("Risultato ricevuto dal server: %ld\n", result);
    }
    
    // 10. CLIENT: termina il processo (chiusura connessione)
    closesocket(Csocket);
    ClearWinSock();
    system ("pause"); // Solo per debug in ambiente Windows
    return EXIT_SUCCESS;
}