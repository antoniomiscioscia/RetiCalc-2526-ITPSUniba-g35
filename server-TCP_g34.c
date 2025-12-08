#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Inclusioni specifiche per Sockets (come da slide)
#if defined WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close                                                                           // Mappa closesocket su close per sistemi Unix [cite: 204, 205]
#endif

// Costanti
#define PROTOPORT 48000                                                                             // Porta di default per l'applicazione
#define QLEN 5                                                                                      // Dimensione della coda di richieste (ad es. 5), massimo numero di connessioni accettate
#define BUFFER_SIZE 512                                                                             // Dimensione del buffer
#define EXIT_STRING "TERMINE PROCESSO CLIENT"                                                       // Stringa di terminazione
#define CONNECT_OK_STRING "connessione avvenuta"                                                    // Stringa di conferma connessione

void ErrorHandler(char *errorMessage) {                                                             // Funzione di gestione errori
    printf("%s", errorMessage);
}
void ClearWinSock() {                                                                               // Funzione di pulizia Winsock
#if defined WIN32
    WSACleanup();
#endif
}

// Funzione di utilità per ricevere esattamente la dimensione richiesta
// Necessaria per la comunicazione TCP (stream) per assicurare la ricezione completa dei dati
int RecvExact(int sock, char *buf, int len) {                                                       // Riceve esattamente 'len' byte dalla socket 'sock' e li memorizza in 'buf'
    int total_bytes = 0;                                                                            // Conteggio totale dei byte ricevuti
    int bytes_rcvd;                                                                                 // Numero di byte ricevuti in una singola chiamata a recv()
    while (total_bytes < len) {                                                                     // Continua fino a quando non sono stati ricevuti 'len' byte
        bytes_rcvd = recv(sock, buf + total_bytes, len - total_bytes, 0);                           // Riceve dati dalla socket
    if (bytes_rcvd <= 0) return bytes_rcvd;                                                         // Errore o connessione chiusa
        total_bytes += bytes_rcvd;                                                                  // Aggiorna il conteggio dei byte totali ricevuti
    }
    return total_bytes;
}

int main(int argc, char *argv[]) {
    // 1. Inizializzazione Winsock (solo per Windows)
    #if defined WIN32
    WSADATA wsaData;
    if (WSAStartup (MAKEWORD (2,2), &wsaData) != 0) {
        ErrorHandler("Errore in WSAStartup()\n");
        return EXIT_FAILURE;
    }
    #endif

    int MySocket, clientSocket;                                                                     // Descrittore della socket principale e della socket client
    struct sockaddr_in sad, cad;                                                                    // sad: Server Address, cad: Client Address
    unsigned int clientLen;                                                                         // Lunghezza dell'indirizzo client
    char operation_char;
    char response_string[BUFFER_SIZE];                                                              // Buffer di risposta
    int operands[2];                                                                                // [0] = op1, [1] = op2
    long result;

    // 2. CREAZIONE DELLA SOCKET (Listening Socket)
    if ((MySocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {                                   // SOCK_STREAM per TCP
        ErrorHandler("Creazione della socket fallita.\n");                                              // Se la creazione fallisce, terminare il server
        ClearWinSock();
        return EXIT_FAILURE;                                                                            // Restituisce EXIT_FAILURE in caso di errore
    }

    // 3. COSTRUZIONE E BIND DELL'INDIRIZZO (Solo lato SERVER)
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;                                                                           // Famiglia di protocolli IPv4
    sad.sin_addr.s_addr = inet_addr("127.0.0.1");                                                       // Ascolto su localhost (la stessa macchina)
    sad.sin_port = htons (PROTOPORT);                                                                   // Porta in formato Big-Endian (Network Byte Order)

    if (bind (MySocket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {                                        // Assegna porta e IP alla socket 
        ErrorHandler("bind() fallito.\n");                                                                   // Se il bind fallisce, termina il server
        closesocket (MySocket);
        ClearWinSock();
        return EXIT_FAILURE;
    }

    // 4. SETTAGGIO DELLA SOCKET ALL'ASCOLTO
    if (listen (MySocket, QLEN) < 0) {                                                                      // Mette la socket in attesa di richieste di connessione
        ErrorHandler("listen() fallito.\n");                                                                 // Se il listen fallisce, termina il server
        closesocket (MySocket);
        ClearWinSock();
        return EXIT_FAILURE;                                                                                // Restituisce EXIT_FAILURE in caso di errore
    }
    printf("Server in ascolto sulla porta %d...\n", PROTOPORT);                                                  // Notifica che il server è in ascolto
    
    
    // 5. CICLO DI ACCETTAZIONE (Il server rimane in ascolto iterativamente)
    while (1) {
        clientLen = sizeof(cad);
 
        // α. ACCETTARE UNA NUOVA CONNESSIONE (bloccante fino a quando non arriva una richiesta)
        if ((clientSocket = accept (MySocket, (struct sockaddr *)&cad, &clientLen)) < 0) {                  // Restituisce una nuova socket connessa
            ErrorHandler("accept() fallito.\n");
            // Non chiudo MySocket, continua il ciclo.
            continue;
        }
        printf("\nGestione client %s\n", inet_ntoa (cad.sin_addr));

        // 4. SERVER: invia la stringa "connessione avvenuta" [cite: 5]
        if (send(clientSocket, CONNECT_OK_STRING, strlen(CONNECT_OK_STRING) + 1, 0) != strlen(CONNECT_OK_STRING) + 1) {
            ErrorHandler("send() fallita invio messaggio connessione.\n"); // Se l'invio fallisce, termina il server
        }

        // 7. SERVER: riceve la lettera (1 byte)
        if (recv(clientSocket, &operation_char, 1, 0) <= 0) {
            ErrorHandler("recv() fallita o connessione chiusa prematuramente (carattere operazione).\n");
            closesocket(clientSocket);
            continue;
        }

        // Logica condizionale: imposta la stringa di risposta
        int valid_operation = 1;
        switch (operation_char) {
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
            default: // Carattere non riconosciuto
                strcpy(response_string, EXIT_STRING);
                valid_operation = 0;
                break;
        }
        printf("Ricevuta op: '%c', Invio indietro: '%s'\n", operation_char, response_string);

        // SERVER: invia la stringa di operazione/terminazione
        if (send(clientSocket, response_string, strlen(response_string) + 1, 0) != strlen(response_string) + 1) {
            ErrorHandler("send() fallita invio stringa operazione.\n");
        }
        
        // Se l'operazione è valida:
        if (valid_operation) { 
            // 9. SERVER: riceve i due interi (2 * sizeof(int) bytes)
            if (RecvExact(clientSocket, (char *)operands, sizeof(int) * 2) <= 0) { // 
                ErrorHandler("recv() fallita o connessione chiusa prematuramente (operandi).\n"); // Riceve esattamente 2 * sizeof(int) byte
                closesocket(clientSocket);
                continue;
            }

            
            // Esegue l'operazione
            long op1 = ntohl(operands[0]); // Conversione Network to Host Long [cite: 107]
            long op2 = ntohl(operands[1]);

            switch (operation_char) {
                case 'A': case 'a': result = op1 + op2; break;
                case 'S': case 's': result = op1 - op2; break;
                case 'M': case 'm': result = op1 * op2; break;
                case 'D': case 'd': 
                
                if (op2 != 0) 
                {
                    result = op1 / op2;
                }
                else 
                {
                    result = 0; // Gestione semplice della divisione per zero
                    printf("Errore: divisione per zero.\n");
                }
                break;
            }
            printf("Calcolo: %ld %c %ld = %ld\n", op1, operation_char, op2, result);
            
            // 9. SERVER: invia il risultato (1 * sizeof(long) bytes)
            long net_result = htonl(result); // Conversione Host to Network Long [cite: 106]
            if (send(clientSocket, (char *)&net_result, sizeof(long), 0) != sizeof(long)) {
                ErrorHandler("send() fallita invio risultato.\n");
            }
        }

        // 9. Chiude la connessione corrente (la socket temporanea clientSocket) [cite: 29]
        printf("Chiusura della connessione con il client.\n");
        closesocket(clientSocket); // Chiude la socket temporanea
    }

    // Qui non si arriva mai nel server che non termina
    closesocket(MySocket);
    ClearWinSock();
    return EXIT_SUCCESS;
}

/* La funzione listen ( ) setta la socket in uno stato in cui rimane in attesa di richiesta di connessioni. 
La funzione restituisce 0 in caso di successo, altrimenti -1. Allo stesso modo della funzione bind ( ), 
è necessario al fine di mettere la socket in ascolto soltanto verificare il valore di ritorno. */