#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include "libreria.h"

struct sockaddr_in iniServer(int port, char * addr) {

    struct sockaddr_in server;
    memset(&server,0,sizeof(server)); // Creo l'indirizzo del server
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET,addr,&server.sin_addr);
    return server;

}

char compareString(char* command, int gamemode, bool token) {

    if(gamemode == 1) {
        if(!strcmp(command,"S") || (!strcmp(command,"s")))
            return 's';
        else {
            return 'm';
        }
    }

    if(gamemode == 2) {
        if(!strcmp(command,"!show"))
            return 'p';
        else if(!strcmp(command,"!disconnect"))
            return 'd';
    }

    if(!strcmp("!help",command))
        return 'h';
    else if(!strcmp(command,"!who") && !gamemode)
        return 'w';
    else if(!strcmp(command,"!quit") && !gamemode)
        return 'q';
    else
        return 'n';

}

void welcome(bool gamemode) {

    // printf("|***************************************************************|\n");
    printf("Sono disponibili i seguenti comandi:\n");
    printf("!help --> mostra l'elenco dei comandi disponibili\n");
    if(!gamemode) {
        printf("!who --> mostra l'elenco dei client connessi al server\n");
        printf("!connect <username> --> avvia una partita con l'utente <username>\n");
        printf("!quit --> disconnetti il client dal server\n");
    } else {
        printf("!disconnect --> disconnette il client dalla partita in corso\n");
        printf("!shot <square> --> fai un tentativo sulla casella <square>\n");
        printf("!show --> visualizza griglia gioco\n");
        printf("0 = NON COLPITO - 1 = NAVE - 2 = ACQUA - 3 = AFFONDATO\n");
    }
   	printf("*****************************************************************\n");

}

void whoIsOnline(int tcp) {

    int i, ret, j;
    char buff[1024];
    
    toHostC('w',tcp);

    // Mi preparo a ricevere N utenti
    fromHostInt(&i,tcp);

    if(i > 1)
        printf("Ci sono %d utenti online:\n",i);
    else if(i == 1)
        printf("C'e' un utente online\n");
    else {
        printf("Non c'è nessun utente online\n");
        return;
    }

    for(j = 0; j < i; j++) {

        fromHostStr(buff,tcp);
        printf("   %s ",buff);

        ret = recv(tcp,(void *)buff,2,0);	// ricevo stato utente
        if(ret < 0)
    		perror("Error");
    	if(ret < 1)
        	printf("Error WhoIsOnline() ret=%d len=1",ret);
        if(buff[0] == '1')
            printf("(occupato)\n");
        else
            printf("(libero)\n");

    }

}

void quit(int tcp, int udp) {
    
    toHostC('q',tcp);
    close(tcp);
    close(udp);
    printf("Ti sei disconnesso correttamente\n");
    exit(0);

}

void resetStatus(bool* token, int* gamemode, int* myShipsSunk, int* enemyShipsSunk, struct timeval* timeout, bool* syncro) {

	*myShipsSunk = 0;
    *enemyShipsSunk = 0;
    *token = false;
    *syncro = false;
    *gamemode = 0;
    timeout->tv_sec = 9999;
    timeout->tv_usec = 0;

}

void leaveGame(int tcp) {

    toHostC('d',tcp);
    printf("\nSei uscito dalla partita in corso: HAI PERSO!\n");

}

bool sendHit(struct sockaddr_in server, int udp, int* g1, 
	     bool* token, char* command, int* myShipsSunk, int* enemyShipsSunk, int* gamemode, 
	     int tcp, struct timeval* timeout,bool* syncro) {

    int ret, l, n;
    socklen_t slen;
    char buff[3];

    // Controllo le coordinate
    // !shot f,4 <= pos = 6 e 8
    if((command[0] >= 'A' && command[0] <= 'F') || (command[0] >= 'a' && command[0] <= 'f')) {	// Con la getchar sono 6 per le lettere

        if(command[2] >= '1' && command[2] <= '6') {						// e 8 per i numeri

            // Verifico se ho già colpito in quelle coordinate
            l = command[0] >= 'a' ? command[0] - 'a' : command[0] -'A';	// colonna
            n = (command[2] - '0') - 1;	// riga

            // printf("mie coordinate: <%d,%d>\n",n,l);

            if(g1[n*6+l] == 0) {

                printf("Hai colpito in <%c,%d>: ",l+'A',n+1);

                buff[0] = 'c';
                buff[1] = '\0';
                
                ret = sendto(udp,buff,2,0,(struct sockaddr *) &server,sizeof(server));
                if(ret < 0)
    				perror("Error");
    		
    			if(ret < 1)
        			printf("Error sendHit() 1 ret=%d len=1",ret); 
                
                buff[0] = l + '0';
                buff[1] = n + '0';
                buff[2] = '\0';
                
                ret = sendto(udp,buff,3,0,(struct sockaddr *) &server,sizeof(server));
                if(ret < 0)
    				perror("Error");
    			if(ret < 2)
        			printf("Error sendHit() 2 ret=%d len=2",ret);  
                    
                slen = sizeof(server);
                ret = recvfrom(udp,buff,2,0,(struct sockaddr *) &server, &slen);
                if(ret < 0)
    				perror("Error");
    			if(ret < 1)
        			printf("Error sendHit() 3 ret=%d len=1",ret); 
                
                if(buff[0] == '0') {
                    g1[n*6+l] = 2;	// Acqua
                    printf("acqua! ");
                } else {
                    g1[n*6+l] = 3;	// Colpito e affondato!
                    *enemyShipsSunk = *enemyShipsSunk + 1;
                    printf("nave affondata! ");
                }

                *token = false;
                timeout->tv_sec = 60;
                timeout->tv_usec = 0;

                if(*enemyShipsSunk > 6)
                    buff[0] = 'v';
                else
                    buff[0] = 'y';	// Cambio turno
                buff[1] = '\0';
		
				ret = sendto(udp,buff,2,0,(struct sockaddr *) &server,sizeof(server));
				
				if(ret < 0)
    				perror("Error");
    			if(ret < 1)
        			printf("Error sendHit() 4 ret=%d len=1",ret); 

                if(*enemyShipsSunk > 6) {
                    resetStatus(token,gamemode,myShipsSunk,enemyShipsSunk,timeout,syncro);
                    toHostC('v',tcp);
                    printf("Partita terminata: HAI VINTO!\n");
                }

                return 1;

            }

            printf("Bersaglio gia' colpito. Riprova\n");
            return 0;

        }

    }

    printf("Coordinate errate. Riprova\n");
    return 0;

}

void recvHit(int* g, int udp, int* myShipsSunk, bool* token, int* gamemode, char* buff, struct timeval* timeout, struct sockaddr_in server) {

    int n, l, ret;
    socklen_t slen;
    
    slen = sizeof(server);
    ret = recvfrom(udp,buff,3,0,(struct sockaddr *) &server, &slen);
    if(ret < 0)
    	perror("Error");
    if(ret < 2)
    	printf("Error recvHit() 1 ret=%d len=2",ret); 

    l = buff[0] - '0';
    n = buff[1] - '0';

    printf("\n\nL'avversario ha colpito in <%c,%d>: ",l+'A',n+1);

    if(g[n*6+l] == 1) {
		
		*myShipsSunk = *myShipsSunk + 1;
        g[n*6+l] = 3;	// Colpito e affondato
        printf("nave affondata! ");

    } else {

        g[n*6+l] = 2;	// Acqua
        printf("acqua! ");

    }
    
    buff[0] = g[n*6+l] == 3 ? '1' : '0';	// 1 = colpito, 0 = acqua
    buff[1] = '\0';
    ret = sendto(udp,buff,2,0,(struct sockaddr *) &server, sizeof(server));	// Informo l'avversario!
    if(ret < 0)
    	perror("Error");
    if(ret < 1)
    	printf("Error recvHit() 2 ret=%d len=1",ret);
    	
    ret = recvfrom(udp,buff,2,0,(struct sockaddr *) &server, &slen);
    if(ret < 0)
    	perror("Error");
    if(ret < 1)
    	printf("Error recvHit() 3 ret=%d len=1",ret);
    	
    if(buff[0] == 'v')
    	return;
    	
    // else buff[0] == y
    // sleep(1);		// Per rendere desincronizzati i due host in caso di timeout
    
    *token = true;
    timeout->tv_sec = 60;
    timeout->tv_usec = 0;

}

void resetGriglia(int *g) {

    int i, j;
    for(i = 0; i < 6; i++) {
        for(j = 0; j < 6; j++) {
            g[i*6+j] = 0;
        }
    }

}

void stampaGriglia(int *g) {

    int i, j;
    char c = 'A';
    for(j = 0; j < 7; j++)
        if(j == 0)
            printf("  ");
        else
            printf("%c ",c++);
    printf("\n");

    //  A B C D E F
    //1 0 0 0 0 0 0
    //2 0 0 0 0 0 0

    for(i = 0; i < 6; i++) {
        for(j = 0; j < 6; j++) {
            if(j == 0)
                printf("%d ", i+1);
            printf("%d ", g[i*6+j]);
        }
        printf("\n");
    }

}

void posizionaNavi(int* g) {

    int i,l,n;
    char flotta[4];

    printf("Pronto a posizionare la tua flotta?!\nHai a disposizione 7 navi grandi quanto una casella ed una matrice 6x6 in questa forma:\n");
    stampaGriglia(g);
    printf("Per posizionare una nave (alla volta) digita <colonna,riga> (es. F,4) senza spazi e virgolette e premi invio.\n\n");
    printf("NB: hai al massimo 60 sec per posizionare le tue navi e dichiarare un attacco, altrimenti verrai disconnesso.\n");

    for(i = 0; i < 7; i++) {
        
        printf("@ Nave %d: ",i+1);
        scanf("%s",flotta);
		
        if(strlen(flotta) == 3)
            if((flotta[0] >= 'A' && flotta[0] <= 'F') || (flotta[0] >= 'a' && flotta[0] <= 'f'))
                if(flotta[2] >= '1' && flotta[2] <= '6') {
                    // Verifico che la nave non sia stata gia' posizionata
                    l = flotta[0] >= 'a' ? flotta[0] - 'a' : flotta[0] -'A';	// colonna
                    n = flotta[2] - '0';	// riga
                    
                    if(g[ (n - 1)*6 + l ] == 0){
                    
                    	g[ (n - 1)*6 + l ] = 1;	// Nave posizionata
                    	printf("Nave posizionata in <%c,%c>\n",l + 'A',flotta[2]);

                    	continue;
                    
                    }

                }
                
        printf("Coordinate errate. Riprova\n");
        i--;
        
    }

    printf("\nLa tua situazione:\n");
    stampaGriglia(g);

    printf("Sei pronto a giocare. Attendi che anche l'avvesario abbia terminato!\n\n");

}

void recvNotify(int tcp, int* gamemode, int* enemyId, struct timeval* timeout, 
				bool* token, int* myShipsSunk, int* enemyShipsSunk, bool* syncro,struct sockaddr_in server) {

    char buff[1024];

    fromHostC(&buff[0],tcp);		// Ricevo il comando dal server
    
    if(buff[0] == 'v'){
    	toHostC('d',tcp);
        resetStatus(token,gamemode,myShipsSunk,enemyShipsSunk,timeout,syncro);
        printf("\nPartita terminata: HAI PERSO!\n");
        return;
    }
    
    if(buff[0] == 't'){
     	return;
    } 
    
    if(buff[0] == 'd'){
    	toHostC('d',tcp);
    	if(*gamemode == 2)
        	printf("\nL'avversario si e' disconnesso: HAI VINTO!\n");
        else
        	printf("\nL'avversario si e' disconnesso. Partita annullata\n");
        resetStatus(token,gamemode,myShipsSunk,enemyShipsSunk,timeout,syncro);
     	return;
    }       

    if(buff[0] == 'g') {
  
    	fromHostInt(enemyId,tcp);
        fromHostStr(buff,tcp);
        printf("\n%s vuole giocare con te! Accettare? [S/n]\n",buff);
        *gamemode = 1;	// Necessario per risponde a sì o no quando giunge la richiesta di gioco da un altro client

    }

}

void abortRequest(int tcp,int* gamemode,int* enemyId) {

    char buff[1024];
    *gamemode = 0;
    toHostC('r',tcp);			// Response
    toHostInt(*enemyId,tcp);	// Send id user
    toHostC('0',tcp);			// Negative
    printf("Hai rifiutato la richiesta\n");
    fromHostC(buff,tcp);		// Questo dato non mi interessa nel caso del rifiuto, serve solo per sincronizzare client e server

}

void sendSyncro(int udp, struct sockaddr_in server) {

    int ret;
    char buff[2];

    // Invio segnale di syncronizzazione all'altro client
    buff[0] = 's';
    buff[1] = '\0';
    ret = sendto(udp,buff,2,0,(struct sockaddr *) &server,sizeof(server));
    if(ret < 1)
        printf("Error sendSyncro() \n");

}

struct sockaddr_in iniGame(int tcp, int udp, int* gamemode,int* g, int* g1, struct timeval* timeout) {
    char ip[40];
    int port;
    struct sockaddr_in server;

    *gamemode = 2;

    fromHostInt(&port,tcp);
    fromHostStr(ip,tcp);
    server = iniServer(port,&ip[0]);

    printf("Ripristino impostazioni...\n");
    resetGriglia(g);
    resetGriglia(g1);
    sleep(1);

    printf("Partita iniziata\n\n");
    welcome(*gamemode);
    posizionaNavi(g);
    sendSyncro(udp,server);

    timeout->tv_sec = 60;
    timeout->tv_usec = 0;

    return server;

}

struct sockaddr_in connectUsers(char* command,int tcp, int udp, char* username,int* gamemode, int* g, int* g1, bool* token, struct timeval* timeout) {

    char buff[1024];
    struct sockaddr_in server;
        
    toHostC('c',tcp);		// Comando connessione

    // Invio i miei dati al server
    toHostStr(username,tcp);	// Passo il mio nome utente
    // L'utente può non esiste.. o esistere, o essere impegnato
    toHostStr(command,tcp);	// Passo il suo nome utente (quello con cui voglio giocare)

    if(!strcmp(username,command)) {
        *gamemode = 0;
        printf("Non puoi inviare una richiesta a te stesso!\n");
        return server;
    }


    fromHostC(buff,tcp);	// Attendo risposta dal server

    if(!strcmp(buff,"0\0")) {

        printf("Richiesta inviata all'avversario. Rimani in attesa... ");
        fflush(stdout);

        fromHostC(buff,tcp);	// Attendo risposta dal server: libero, occupato o non esistente

        if(strcmp(buff,"0\0") == 0) {

            printf("L'avversario ha rifiutato\n");
            return server;

        } else if(strcmp(buff,"d\0") == 0) {
	    
	    toHostC('d',tcp);
            printf("L'avversario si è disconnesso\nInvito annullato\n");
            return server;

        } else {

            printf("L'avversario ha accettato!\nAttendi sincronizzazione...\n");
            server = iniGame(tcp,udp,gamemode,g,g1,timeout);
            *token = true;
            return server;

        }

    } else if(strcmp(buff,"1\0") == 0)
        printf("L'utente e' gia' impegnato in un'altra partita\n");
    else if(strcmp(buff,"2\0") == 0)
        printf("L'utente cercato non esiste\n");
    return server;

}

struct sockaddr_in startGame(int tcp, int udp, int* gamemode, int enemyId, int* g, int* g1, struct timeval* timeout) {

    char buff[1024];

    struct sockaddr_in server;
    
    toHostC('r',tcp);			// Comando richiesta di connessione

    toHostInt(enemyId,tcp);		// Invio l'id di chi mi ha fatto richiesta

    toHostC('1',tcp);			// Risposta affermativa
    
    fromHostC(&buff[0],tcp);		// Stato dell'avversario (online o offline)

    if(!strcmp(buff,"d\0")) {
    	toHostC('d',tcp);
        printf("L'avversario si è disconnesso. Partita annullata\n");
        *gamemode = 0;
        return server;
    }

    // Inizializzare server
    printf("Hai accettato\nAttendi sincronizzazione...\n");
    sleep(1);
    server = iniGame(tcp,udp,gamemode,g,g1,timeout);
    return server;

}

void selectCommand(int tcp, int udp, char* username, int port) {

    char command[50], buff[1024], c;
    socklen_t slen;
    bool token, syncro;
    int ret, gamemode, fdmax, fd, myShipsSunk, enemyShipsSunk, enemyId, g[6][6], g1[6][6];
    fd_set master, read;
    struct sockaddr_in server;
    struct timeval timeout = {9999,0};
    
	myShipsSunk = 0;
    enemyShipsSunk = 0;
    gamemode = 0;
    fdmax = tcp > udp ? tcp : udp;
    syncro = false;
    token = false;

    FD_ZERO(&master);
    FD_ZERO(&read);
    FD_SET(0,&master);		// stdinput stream sempre pronto
    FD_SET(tcp,&master);
    FD_SET(udp,&master);

    while(1) {

        read = master;

        if(gamemode == 2 && myShipsSunk < 7) {
            if(syncro && token) {
                printf("E' il tuo turno\n");
            } else if(syncro && !token) {
                printf("Attendi il turno avversario\n");
            }
        }

        if(gamemode == 0 || gamemode == 1)
            printf("> ");
        else if(syncro && gamemode == 2)
            printf("# ");
        fflush(stdout);

        if(select(fdmax+1,&read,NULL,NULL,&timeout)) {

            for(fd = 0; fd <= fdmax; fd++) {

                if(FD_ISSET(fd,&read)) {

                    if(fd == tcp)

                        recvNotify(tcp,&gamemode,&enemyId,&timeout,&token,&myShipsSunk,&enemyShipsSunk,&syncro,server);	// Controllo se ci sono mex dal server

                    if(fd == udp) {

                        slen = sizeof(server);
                        ret = recvfrom(udp,buff,1024,0,(struct sockaddr *) &server, &slen);

                        if(ret > 0) {

                            if(buff[0] == 's' && syncro == false) {
                                    syncro = true;
                            }

                            if(buff[0] == 'c') {

                                recvHit(&g[0][0],udp,&myShipsSunk,&token,&gamemode,&buff[0],&timeout,server);

                            }

                        }

                    }

                    if(fd == 0) {

                        memset(&command,'\0',sizeof(command));
                        scanf("%s",command);

                        if(strcmp(command,"!connect") == 0) { // Per la versione con scanf
                            scanf("%s",command);
                            if(strlen(command) == 0) {
                                printf("Non hai specificato un nome utente avversario!\nPer mostrare i giocatori online digita il comando: !who\n");
                            } else {
                                server = connectUsers(&command[0],tcp,udp,&username[0],&gamemode,&g[0][0],&g1[0][0],&token,&timeout);
                           	}
                            continue;
                        }
                        if(gamemode == 2 && !syncro)
                            continue;
                            
                        if(gamemode == 2 && !strcmp(command,"!shot") && token) {
                        
                            scanf("%s",command);
                            
                            if(strlen(command) == 0) {
                                printf("Coordinate non specificate. Riprova\n");
                            } else {
                                sendHit(server,udp,&g1[0][0],&token,command,&myShipsSunk,&enemyShipsSunk,&gamemode,tcp,&timeout,&syncro);
                            }
                                
                            continue;

                        }

                        c = compareString(command,gamemode,token);

                        switch(c) {
                        case 'h':
                            printf("\n");
                            welcome(gamemode);
                            break;
                        case 'w':
                            whoIsOnline(tcp);
                            break;
                        case 'q':
                            quit(tcp,udp);
                            break;
                        case 's':
                            server = startGame(tcp,udp,&gamemode,enemyId,&g[0][0],&g1[0][0],&timeout); // Chi accetta richiesta
                            break;
                        case 'p':
                            printf("\nLegenda:\n");
                            printf("0 = NON COLPITO - 1 = NAVE - 2 = ACQUA - 3 = AFFONDATO\n");
                            printf("\nLa mia situazione:\n");
                            stampaGriglia(&g[0][0]);
                            printf("\nLa situazione avversaria:\n");
                            stampaGriglia(&g1[0][0]);
                            printf("\n");
                            break;
                        case 'm':
                            abortRequest(tcp,&gamemode,&enemyId);
                            break;
                        case 'd':
                            resetStatus(&token,&gamemode,&myShipsSunk,&enemyShipsSunk,&timeout,&syncro);
                            leaveGame(tcp);
                            break;
                        case 'v':
                            printf("Attendi il tuo turno per colpire\n");
                        default:
                            //printf("Inserito comando non valido.\n");
                            continue;
                        }

                    } 							// Fine fd = 0

                }								// Fine if(FD_ISSET(fd,&read))

            }									// Fine for
            
        } else { 								// Se scade il timeout

            if(gamemode) {
                resetStatus(&token,&gamemode,&myShipsSunk,&enemyShipsSunk,&timeout,&syncro);
                toHostC('t',tcp);
                printf("\nTempo scaduto per l'azione. Partita terminata\n");
            }

            timeout.tv_sec = 9999;
            timeout.tv_usec = 0;

        }

    }											// Fine while

}

void login(char * username, int * port, int tcp, int udp) { // Sistemare binding sull'udp e poi controllo nome

    int ret;
    char buff[1024];
    struct sockaddr_in me;

    while(1) {

        printf("Inserisci nome: ");
        scanf("%s",username);
        
        if(strcmp(username,"Unown") == 0){
        	printf("Nome utente non permesso. ");
        	continue;
        }

		toHostC('u',tcp);				// Comando: passaggio username

        toHostStr(username,tcp);
        fromHostC(&buff[0],tcp);		// Ricevo lo stato della risposta: 1 = Già utilizzato, 0 = libero

        if(!strcmp(buff,"1\0")) {
            printf("Nome utente gia' in uso. ");
        } else
            break;

    }

    while(1) {
   
        printf("Inserisci porta UDP di ascolto: ");	// Recuperare il programma se va in crash per la porta
        ret = scanf("%d",port);
        if(ret == 0){
            while(getchar() != '\n' && !feof(stdin))	// Svuoto lo stream stdin in caso di errore
            	;
            continue;
        }
        
        if(*port <= 1024 || *port > 65535) {  // Controllo non necessario, tanto il sistema me ne da una disponibile a piacere suo in caso di errore
           printf("Numero di porta non valido! (Porta compresa tra 1024 e 65535)\n");
           continue;
        }

        memset(&me,0,sizeof(me)); // Creo l'indirizzo del server
        me.sin_family = AF_INET;
        me.sin_port = htons(*port);
        me.sin_addr.s_addr = INADDR_ANY;
        ret = bind(udp,(struct sockaddr *) &me,sizeof(me));	// Collego l'indirizzo al socket
        if(ret == -1) {
            printf("Porta gia' impegnata. ");
            continue;
        } else
            break;
            
    }

    // Passaggio porta
    toHostC('p',tcp);			// Comando: passaggio porta
    toHostInt(*port,tcp);
    
}

int main(int arg, char * args[]) {

    struct sockaddr_in server;
    int tcp, udp, ret, port;
    char username[50];

    printf("// CLIENT\n");
    if(arg < 3) {
        printf("[Err] [Abort] Informazioni non specificate\n");
        exit(1);
    }

    // Creo socket del server TCP: server per comunicare con il server
    tcp = socket(AF_INET,SOCK_STREAM,0);
    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET; // TCP
    server.sin_port = htons(atoi(args[2])); // Porta da usare
    inet_pton(AF_INET,args[1],&server.sin_addr);

    // Creazione socket UDP: serve per comunicare con l'avversario
    udp = socket(AF_INET,SOCK_DGRAM,0); // Creo il socket UDP

    ret = connect(tcp,(struct sockaddr*)&server,sizeof(server));
    if(ret < 0) {
        perror("Error");
        exit(1);
    }

    printf("Connessione al server %s (porta %s) effettuata con successo\n\n",args[1],args[2]);

    welcome(0);
    login(&username[0],&port,tcp,udp);

    selectCommand(tcp,udp,username,port);

    close(tcp);
    close(udp);

    return 0;

}
