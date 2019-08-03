#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define DIM 1024

void fromHostInt(int* n, int fd){

    int ret;

    ret = recv(fd,n,4,0);	// Attendo dato
    if(ret < 0)
    	perror("Error");
    if(ret < 4)
    	printf("Error fromHostInt() ret=%d len=4\n",ret);
     
    *n = ntohl(*n);

}

void fromHostStr(char* obuff,int fd) {

    int len, ret;
    char buff[DIM];

    memset(&buff,'\0',sizeof(buff));
    
    fromHostInt(&len,fd);
    
    ret = recv(fd,(void *)buff,len+1,0);	// Attendo dato
    if(ret < 0)
    	perror("Error");
    if(ret < len)
        printf("Error fromHostStr() ret=%d len=%d\n",ret,len);

    strcpy(obuff,buff);

}

void fromHostC(char* obuff,int fd) {

    int ret;
    char buff[2];

    memset(&buff,'\0',sizeof(buff));
    
    ret = recv(fd,(void *)buff,2,0);
    if(ret == 0) {
        printf("\nServer è chiuso. Terminazione forzata del client\n");
        exit(1);
    }
    if(ret < 0)
    	perror("Error");
    if(ret < 1)
        printf("Error fromHostC() ret=%d len=1\n",ret);
    strcpy(obuff,buff);

}

void toHostInt(int n, int fd) {

    int ret;

    n = htonl(n);
    ret = send(fd,&n,4,0);	 // Invio dato
    if(ret < 0)
    	perror("Error");
    if(ret < 4)
        printf("Error toHostInt() ret=%d len=4\n",ret);

}

void toHostStr(char* str, int fd) {

    int len, ret;
    char buff[DIM];

    memset(&buff,'\0',sizeof(buff));
    
    len = strlen(str);
    toHostInt(len,fd);

    strcpy(buff,str);
    ret = send(fd,(void *)buff,len+1,0);	 // Invio dato
    if(ret < 0)
    	perror("Error");
    if(ret < len)
        printf("Error toHostStr() ret=%d len=%d\n",ret,len);

}

void toHostC(char c, int fd) {

    int ret;
    char buff[2];

    memset(&buff,'\0',sizeof(buff));
    
    buff[0] = c;
    buff[1] = '\0';
    
    ret = send(fd,(void *) buff,2,0); // Invio comando
    if(ret < 0)
    	perror("Error");
    if(ret < 1)
        printf("Error toHostC() ret=%d len=1\n",ret);

}
