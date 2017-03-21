#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8888
#define MAXLEN 1024
#define REQLEN 32
#define FILELEN 128
#define LISTENQ 1

struct clfs_req{
    enum{
        CLFS_PUT,
        CLFS_GET,
        CLFS_REM
    }type;
    int inode;
    int size;
};

enum clfs_status{
    CLFS_OK,
    CLFS_INVAL,
    CLFS_ACCESS,
    CLFS_ERROR
};

void handle_req(void * args){
    int connfd;
    int ret;
    int circle;
    int type, inode, size;
    char req[REQLEN];
    char *buf;
    char filename[FILELEN];
    char temp[FILELEN];
    FILE *f;
    
    pthread_detach(pthread_self());
    connfd = (int)(*((int *)args));
    ret = recv(connfd, req, REQLEN, 0);
    if(ret > 0){
        sscanf(req, "%d%d%d", &type, &inode, &size);
        printf("file size: %d\n", size);
        sprintf(filename, "./clfs_store/%d.dat", inode);
        buf = (char *)malloc(size);
        switch(type){
            case 1://put
                circle = 0;
                printf("receiving evicted data\n");
                f = fopen(filename, "wt+");
                while((ret = recv(connfd, buf, size, 0)) > 0){
                    printf("data #%d reviced\n", inode);
                    printf("ret = %d\n", ret);
                    buf[size] = '\0';
                    fputs(buf, f);
                }
                fclose(f);
                break;
            case 2://get
                if(!access(filename, 0)){
                    printf("file does not exist\n");
                    break;
                }
                f = fopen(filename, "wt+");
                fgets(buf, size, f);
                send(connfd, buf, size, 0);
                fclose(f);
                break;
            case 3://remove
                if(!access(filename, 0)){
                    printf("file does not exist\n");
                    break;
                }
                if(!remove(filename)){
                    printf("remove file failed\n");
                }
                break;
        }
    }
    close(connfd);
}

int open_listenfd(int port){
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                (const void *)&optval, sizeof(int)) < 0)
        return -1;

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if(bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    if(listen(listenfd, LISTENQ) < 0)
        return -1;

    return listenfd;
}

int main(){
    char buf[MAXLEN];
    int listenfd, connectfd;
    struct sockaddr_in client;
    socklen_t addrlen;
    pthread_t client_id;
    int create_ret;

    listenfd = open_listenfd(PORT);
    client_id = 0;
    while(1){
        printf("File server listening on port 8888...\n");
        ++client_id;
        addrlen = sizeof(client);
        connectfd = accept(listenfd, (struct sockaddr *)&client, &addrlen);
        create_ret = pthread_create(&client_id, NULL, (void *)handle_req, (void *)&connectfd);
        if(create_ret != 0)
            printf("Thread create failed!\n");
    }
    return 0;
}
