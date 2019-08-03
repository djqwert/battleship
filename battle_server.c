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
#include "libreria.h"

#define BACKLOG 100
#define DIM 1024

struct user* users;
struct game* games;
int online;

void quit(int, struct game**);

struct user {
    char username[50];
    int port;		// Le porte disponibili vanno da 1024 a 65535, max su 4 B
    int id;
    int fd;
    bool busy;
    struct sockaddr_in client;
    struct user* next;
};

struct game {
    int gamers[2];	//gamers[0] = Chi invita, gamers[1] = chi accetta
    bool state;
    struct game* next;
};

int memGame(struct game** p0, int fd_ric, int fd_inv) {

    struct game* p;
    struct game* i = (struct game*) malloc(sizeof(struct game));

    if(!i) {
        printf("Impossibile allocare memoria per gli inviti\n");
        return -1;
    }

    i->gamers[0] = fd_ric;
    i->gamers[1] = fd_inv;
    i->state = false;		// Partita non iniziata
    i->next = NULL;

    if(!*p0)
        *p0 = i;
    else {

        p = *p0;
        while(p->next) {
            p = p->next;
        }

        p->next = i;

    }

    return 0;

}

void remGame(struct game** p0, int fd_ric, int fd_inv) {

    struct game* invito, *i = 0;

    for(invito = *p0; invito && (invito->gamers[0] != fd_ric && invito->gamers[1] != fd_inv); invito = invito->next)
        i = invito;

    if(!invito) {
        printf("Errore invito non trovato");
        return;
    }
    if(invito == *p0)
        *p0 = invito->next;
    else
        i->next = invito->next;
        
    if(invito->state)
    	printf("Partita terminata\n");
   	else
   		printf("Partita annullata\n");

    free(invito);

}

/* Caso mai un utente avesse fatto richiesta di gioco, e l'utente accettatario si fosse disconnesso,
 * posso avvisare chi ha mandato l'invito scorrendo la lista degli inviti e segnalargli la disconnessione
 */

void findGame(struct game** games, int fd,char* str) {

    struct game* i = *games;
    
    if(str[0] == 't')
    	str[0] = 'd';

    for(; i; i = i->next) {	// Cerco la partita
    
    	if(i->gamers[0] == fd || i->gamers[1] == fd){

			if(i->gamers[1] == fd) {			// Quando la trovo, cerco chi ha accettato
		
				toHostC(str[0],i->gamers[0]);		// Quindi lo informo che che l'altro ha rifiutato

			} else if(i->gamers[0] == fd) {			// Altrimenti chi mi ha invitato
		
		 	    toHostC(str[0],i->gamers[1]);		// Quindi lo informo che che l'altro ha rifiutato

			}
		
			remGame(games,i->gamers[0],i->gamers[1]);

			break;
		
		}
    
    }
    
    for(i = *games; i; i = i->next) {
     
    	printf("\nPartite in corso:\n");
    	printf("fd: %d, g0: %d, g1: %d\n",fd,i->gamers[0],i->gamers[1]);
    
    }

}

struct user* buildUser(struct user** p0) {

    // Potrei creare un secondo puntatore per accedere con complessità O(1) alla lista... ma non lo voglio fare
    struct user* p;
    struct user* user = (struct user*) malloc(sizeof(struct user));

    if(!user)
        return 0;

    user->next = NULL;

    if(!*p0)
        *p0 = user;
    else {

        p = *p0;
        while(p->next) {
            p = p->next;
        }

        p->next = user;

    }

    return user;

}

// Ottieni comando dal client
int getCommand(char* buff, int fd, struct game** games,fd_set* master) {

    int ret;
    struct user* user;

    //printf("Gestione comandi\n");

    ret = recv(fd,(void *)buff,2,0); 	// Comando
    if(ret == 0) {						// Il socket client e' stato chiuso
        quit(fd,games);
        FD_CLR(fd,master);
        close(fd);
        return -1;
    }
    if(ret < 0)
    	perror("Error");
    if(ret < 1)
        printf("Error getCommand() ret=%d len=1\n",ret);
        
    for(user = users; user; user = user->next) { // Cerco il mio id (chi fa richiesta di gioco)
        if(user->fd == fd) {
            printf("ID=%d. ",user->id);
            break;
        }
    }

    return 0;

}

// Non dovrebbe venire mai chiamata: gestisce chiamate fantasma da parte degli host
int ghostingCall(char* str) {
    return strlen(str);
}

// Comando !disconnect
int disconnectCommand(char* str) {
    return !strcmp(str,"d\0") || !strcmp(str,"v\0") || !strcmp(str,"t\0");
}

void disconnect(struct user* users,int fd, struct game** games,char* str) {

    struct user* user = users;

    for(; user; user = user->next)
        if(fd == user->fd)
            break;
            
    if(user->busy){
	    user->busy = false;
	    findGame(games,user->fd,str);	// Trovo l'altro giocatore nella partita in corso, a questo punto gli invio la richiesta di dc
	    printf("%s e' uscito dalla partita\n%s e' disponibile\n",user->username,user->username);
    }

}

// Comando !quit
int quitCommand(char* str) {
    return !strcmp(str,"q\0");
}

void quit(int fd, struct game** games) {

    struct user* user, *guest = 0;
    char str[2];

    for(user = users; user && user->fd != fd; user = user->next)
        guest = user;

    if(!user) return;

    str[0] = 'd';
    str[1] = '\0';
    findGame(games,fd,&str[0]);	// Se l'utente sta giocando cerco la partita in corso e informo l'altro giocatore

    if(user == users)
        users = user->next;
    else
        guest->next = user->next;
	
	if(strcmp(user->username,"Unown") == 0)
   		printf("ID=%d e' offline\n",user->id);
   	else
   		printf("%s e' offline\n",user->username);
    
    if(user->port)				// user->port = 0 => Utente non online
    	online = online - 1;
    	
    free(user);

}

// Comando !online
int onlineCommand(char* str) {
    return !strcmp(str,"w\0");
}

void onlineUsers(struct user* users,int fd) {

    int ret;
    char buff[DIM];
    struct user* user;

    toHostInt(online,fd);
	
    for(user = users; user; user = user->next) {
    
    	if(!user->port)								// Se la porta è uguale a 0, allora è come se l'utente fosse offline
    		continue;

        toHostStr(user->username,fd);	// Nome utente

        buff[0] = user->busy ? '1': '0';
        buff[1] = '\0';
        ret = send(fd,(void *)buff,2,0); // Stato
        if(ret < 0)
    		perror("Error");
   		if(ret < 1)
       		printf("Error onlineUsers() ret=%d len=1\n",ret);

    }
    printf("Creata lista utenti online\n");

}

// Comando !connect
int connectCommand(char* str) {
    return !strcmp(str,"c\0");
}

void connectUsers(struct user* users,int fd,struct game** games) {	// Funzione parallela alla 

    int myid;
    bool flag;
    char buff[DIM], bbuff[DIM];
    struct user* user, *guest;

    printf("Richiesta di connessione: ");

    fromHostStr(bbuff,fd); // Ricevo il mio nome utente (chi fa richiesta di gioco)

    fromHostStr(buff,fd);  // Ricevo nome utente avversario (chi accetta/rifiuta)

    if(strcmp(bbuff,buff) == 0) {
        printf("%s ha inviato la richiesta a se stesso\n",bbuff);
        return;
    }

    flag = true;

    for(user = users; user; user = user->next) { // Cerco il mio id (chi fa richiesta di gioco)
        if(strcmp(user->username,bbuff) == 0) {
            myid = user->id;
            guest = user;
            break;
        }
    }

    for(user = users; user; user = user->next) { // Cerco l'avversario e ...

        if(strcmp(user->username,buff) == 0) {	 // .. se lo trovo ..

            if(user->busy) {			 // .. è impegnato

				toHostC('1',fd);		 // Avversario impegnato
                flag = false;
                printf("%s e' gia' impegnato in un'altra partita\n",user->username);

            } else {				 	// Altrimenti è disponibile a giocare
            
            	if(strcmp(buff,"Unown") == 0){
            		flag = true;
            		break;	
            	}

                flag = false;
                toHostC('0',fd);			// Dico, a chi richiede, che l'avversario è disponibile a giocare (invio la risposta di disponibilità)
                guest->busy = true; 			// Il richiedente diventa "impegnato"
                user->busy = true;  			// .. e anche l'invitato, così nel frattempo non può ricevere nuove richieste
                
                toHostC('g',user->fd);			// Comando gioco per chi deve accettare
                toHostInt(myid,user->fd);		// Invio all'accettatario id, di chi invia la richiesta
                toHostStr(bbuff,user->fd);		// e lo username

                memGame(games,fd,user->fd);

                printf("%s ha inviato una richiesta a %s\n",guest->username,user->username);

            }
            
            break;

        }

    }	// Fine ciclo for

    if(flag) {			// Altrimenti non esiste
        toHostC('2',fd);// Comando gioco per chi deve accettare
        printf("Utente non trovato\n");
    }

}

// Comando response to connect
int responseCommand(char* str) {
    return !strcmp(str,"r\0");
}

void responseToConnect(struct user* users,int fd, struct game** games) {	// Funzione che si attiva, quando un giocatore ACCETTA

    int j;
    char buff[DIM], bbuff[DIM];
    struct user* user, *guest = NULL;
    struct game* invite = NULL;
 
    fromHostInt(&j,fd);			// Prelevo l'id dell'avversario che ho passato a chi accetta

    fromHostC(buff,fd);			// Prelevo risposta dell'avversario

    // Controllo che J non sia uscito dal gioco
    for(user = users; user; user = user->next) { // Cerco il mio id (chi fa richiesta di partita)
        if(user->id == j) {
            guest = user;
            break;
        }
    }

    // Cerco l'utente accettatario
    user = users;
    for( ; user; user = user->next) { 	// Cerco il mio id (chi ACCETTA la partita)
        if(user->fd == fd)
            break;
    }

    bbuff[0] = '0'; 					// Tutto okay!
    if(!guest) {
        printf("L'utente ID=%d si è disconnesso! Partita terminata\n",j);	// Chi ha fatto l'invito si è disconnesso
        bbuff[0] = '1';					// Significa errore
        user->busy = false;				// Ripristino lo stato di chi accettato
        remGame(games,guest->fd,user->fd);
    }
    bbuff[1] = '\0';
    toHostC(bbuff[0],fd);				// Dico a chi ACCETTA se chi ha richiesto il game è ancora online
   
    if(bbuff[0] == '1')
        return;							// Quindi blocco tutto

    if(strcmp(buff,"0\0") == 0) {		// Rifiuto

        printf("%s ha rifiutato\n",user->username);

        guest->busy = false;			// Il richiedente non e' più impegnato e può ricevere nuove richieste di gioco
        user->busy = false;				// Neppure l'accettatario
        toHostC('0',guest->fd);			// Partita rifiutata
        remGame(games,guest->fd,user->fd);
        
        return;

    } else { // Scambio le info tra i due client

        // Invio all'host che ha accettato la richiesta di gioco
        toHostInt(guest->port,fd);
        inet_ntop(AF_INET, &guest->client.sin_addr, bbuff, 40);
        toHostStr(bbuff,fd);

        toHostC('1',guest->fd);			// Confermo al richiedente, l'accettazione dell'avversario

        printf("%s ha accettato\n",user->username);

        // Invio all'host richiedente le info dell'accettatario
        user->busy = true; // Ora e' impegnato anche l'invitato
        toHostInt(user->port,guest->fd);
        inet_ntop(AF_INET, &guest->client.sin_addr, bbuff, 40);
        toHostStr(bbuff,guest->fd);
        
        for(invite = *games; invite; invite = invite->next){
        	if(invite->gamers[0] == guest->fd || invite->gamers[1] == user->fd){
        		invite->state = true;		// Partita iniziata
        	}
        }

        printf("Partita iniziata\n");

    }

}

void printUsers(struct user* users) {

    char buff[DIM];
    struct user* user;

    for(user = users; user; user = user->next) {
        inet_ntop(AF_INET, &user->client.sin_addr, buff, 40);
        printf("ID=%d. USER=%s PORT=%d FD=%d IP=%s ",user->id,user->username,user->port,user->fd,buff);
        if(user->busy == 0)
            printf("(libero)\n");
        else
            printf("(occupato)\n");
    }

}

void registerUser(struct user** guest, int* registered, int newfd) {

    // Costruisco utente registrato
    strcpy((*guest)->username,"Unown");
    (*guest)->id = *registered;
    (*guest)->busy = false;
    (*guest)->fd = newfd;
    (*guest)->port = 0;

    *registered = *registered + 1;

    printf("Utente registrato ID=%d\n",(*guest)->id);

}

// Comando ricezione username
int usernameCommand(char* str) {
    return !strcmp(str,"u\0");
}

void recvUsername(int fd) {

    char buff[DIM], username[50];
    struct user* user, *guest = 0;
    bool flag = false;

   	fromHostStr(buff,fd); // Passaggio username

    // Cerca se esiste già lo stesso username
    for(user = users; user; user = user->next) {

        if(strcmp(user->username,buff) == 0) {
            flag = true;
            break;
        }

        if(user->fd == fd)
            guest = user;

    }

    strcpy(username,buff);
    buff[1] = '\0';

    if(flag) {
        buff[0] = '1';
    } else {
        strcpy(guest->username,username);
        printf("%s e' online\n",guest->username);
        buff[0] = '0';
    }

    toHostC(buff[0],fd);

}

// Comando ricezione porta
int portCommand(char* str) {
    return !strcmp(str,"p\0");
}

void recvPort(int fd) {

    struct user* user;

    user = users;
    for(; user; user = user->next)
        if(user->fd == fd)
            break;

    fromHostInt(&user->port,fd); // Passaggio porta
    
    online = online + 1;

    printf("%s e' pronto a giocare\n",user->username);

    //printUsers(users);

}

int main(int arg, char * args[]) {

    socklen_t slen;
    struct sockaddr_in server;
    struct user *guest;
    int ret, registered, fd, newfd, fdmax, sfd;
    char buff[DIM];
    fd_set master, read;

    printf("// SERVER\n");
    if(arg < 2) {
        printf("[Err] [Abort] Porta non specificata\n");
        exit(1);
    }

    sfd = socket(AF_INET,SOCK_STREAM,0);
    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET; // TCP
    server.sin_port = htons(atoi(args[1])); // Porta da usare
    inet_pton(AF_INET,"127.0.0.1",&server.sin_addr);

    ret = bind(sfd,(struct sockaddr*)&server,sizeof(server));
    if(ret < 0) {
        printf("[Err] Impossibile indirizzare server\n");
        exit(1);
    }
    
    printf("Server inizializzato correttamente. Indirizzo: 127.0.0.1 (Porta %d)\n",ntohs(server.sin_port));

    ret = listen(sfd,BACKLOG);	// Non può fallire, posso solo sbagliare socket XD
    if(ret < 0) {
        printf("[Err] Impossibile mettersi in ascolto \n");
        exit(1);
    }

    FD_ZERO(&master);
    FD_ZERO(&read);

    FD_SET(sfd,&master);
    fdmax = sfd;
    registered = 0;
    online = 0;
    users = 0;
    games = 0;

    printf("Server in attesa di comandi...\n");

    while(1) {

        memset(&buff,'\0',sizeof(buff));
        read = master;
        select(fdmax+1,&read,NULL,NULL,NULL);

        for(fd = 0; fd <= fdmax; fd++) {

            if(FD_ISSET(fd,&read)) {

                if(fd == sfd) {
                    
                    guest = buildUser(&users);

                    slen = sizeof(guest->client);
                    newfd = accept(fd,(struct sockaddr *) &guest->client, &slen);
                    if(newfd == -1) {
                        printf("Impossibile creare nuova socket\n");
                        continue;
                    }

                    FD_SET(newfd,&master);
                    fdmax = newfd > fdmax ? newfd : fdmax;

                    registerUser(&guest,&registered,newfd);

                } else { // E' un newfd

                    if(getCommand(buff,fd,&games,&master))
                        continue;

                    if(usernameCommand(buff))		// Comando: passaggio username

                        recvUsername(fd);

                    if(portCommand(buff))			// Comando: passaggio porta

                        recvPort(fd);

                    if(onlineCommand(buff))       	// Comando: !who

                        onlineUsers(users,fd);

                    if(quitCommand(buff)) { 		// Comando: !quit

                        quit(fd,&games);
                        FD_CLR(fd,&master);
                        close(fd);

                    }

                    if(connectCommand(buff))		// Comando: !connect

                        connectUsers(users,fd,&games);

                    if(responseCommand(buff))		// Comando: accettazione/rifiuto partita

                        responseToConnect(users,fd,&games);

                    if(disconnectCommand(buff)) 	// Comando: !exit

                        disconnect(users,fd,&games,buff);

                }

            } // chiudo if socket

        }

    }

    close(sfd);

    return 0;
}
