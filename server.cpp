#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "messenger.hh"

USERLIST ul;

void* FileTransfer(void* arg)
{
    USERLIST::iterator i, j;
    int maxfd = -1, rc;
    PACKAGE pkg;
    fd_set sendfds;
    struct timeval timeout;
    while (1) {
        FD_ZERO(&sendfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        for (i = ul.begin(); i!=ul.end(); i++)
            if ((*i).online) {
                maxfd = ((*i).sendfd > maxfd) ? (*i).sendfd : maxfd;
                FD_SET((*i).sendfd, &sendfds);
            }
        rc = select(maxfd+1, &sendfds, NULL, NULL, &timeout);
        for (i = ul.begin(); i!=ul.end(); i++)
            if ((*i).online && FD_ISSET((*i).sendfd, &sendfds)) {
                read((*i).sendfd, &pkg, sizeof(PACKAGE));
                for (j = ul.begin(); j!=ul.end(); j++)
                    if (!strcmp((*j).ID, pkg.user))
                        break;
                strcpy(pkg.user, (*i).ID);
                if (pkg.flag == 'Q') {
                    printf("%s wanna send \"%s\" to %s\n", (*i).ID, pkg.content, (*j).ID);
                    if (j == ul.end() || (*j).online == 0) {
                        pkg.flag = 'R';
                        strcpy(pkg.content, "N");
                        write((*i).sendfd, &pkg, sizeof(PACKAGE));
                    }
                    else
                        write((*j).clifd, &pkg, sizeof(PACKAGE));
                }
                else {
                    if (j == ul.end() || (*j).online == 0)
                        continue;
                    write((*j).recvfd, &pkg, sizeof(PACKAGE));
                }
            }
    }
    pthread_exit(NULL);
}


void *Client(void *fdset)
{
    char buffer[MAXBUF] = "",tmp[50];
    USER temp,*me;
    USERLIST::iterator i,j;
    PACKAGE pkg,buf;
    FDSET myset = *(FDSET*)fdset;
    int myfd = myset.clifd,fnum;
    bool addf=0;
    FILE *fp;
    if(read(myfd, buffer, sizeof(buffer))<=0){
	    close(myfd);
	    pthread_exit(NULL);
    }
    if (!strcmp(buffer, "new")) {
        temp.init();
        if (read(myfd, buffer, sizeof(buffer)) <= 0) {
            close(myfd);
            pthread_exit(NULL);
        }
        strcpy(temp.ID, buffer);
        if (read(myfd, buffer, sizeof(buffer)) <= 0) {
            close(myfd);
            pthread_exit(NULL);
        }
        strcpy(temp.PW, buffer);
        for (i = ul.begin(); i != ul.end(); i++){
            if (!strcmp(temp.ID, (*i).ID))
                break;
	}
        if (i != ul.end()) {
	    strcpy(buffer,"ERROR");
            write(myfd, buffer, sizeof(buffer));
            close(myfd);
            pthread_exit(NULL);
        }
	sprintf(buffer,"friend/%s",temp.ID);
	fp = fopen(buffer,"w");
	fputs("0\n",fp);
	fclose(fp);
	
        FILE *userList = fopen("userlist", "a");
        fwrite(&temp, sizeof(USER), 1, userList);
        fclose(userList);
        ul.push_back(temp);
	strcpy(buffer,"Success!!");
        write(myfd, buffer, sizeof(buffer));
        printf("%s join us!!\n", temp.ID);
        close(myfd);
        pthread_exit(NULL);
    }else {
        for (i = ul.begin(); i != ul.end(); i++)
            if (!strcmp(buffer, (*i).ID))
                break;
        // get PW
        if (read(myfd, buffer, sizeof(buffer)) <= 0) {
            close(myfd);
            pthread_exit(NULL);
        }
        if (i == ul.end() || (*i).online || strcmp(buffer, (*i).PW)) {
	    strcpy(buffer,"ERROR");
            write(myfd, buffer, sizeof(buffer));
	    printf("%s login fail\n",temp.ID);
            close(myfd);
            pthread_exit(NULL);
        }
	strcpy(buffer,"Success!!");
        write(myfd, buffer, sizeof(buffer));
        me = &(*i);
        me->clifd = myfd;
        me->sendfd = myset.sendfd;
        me->recvfd = myset.recvfd;      
	printf("%s login!!\n", (*me).ID);
    }

    //Send Friend List to user
    sprintf(buffer,"friend/%s",(*me).ID);
    fp = fopen(buffer,"r");
    while(!feof(fp)){    
           fscanf(fp,"%s\n",buffer);
           write(myfd,buffer,sizeof(buffer));
	   printf("read %s\n",buffer);
    }

    me->online = 1;
    for (i = ul.begin(); i != ul.end(); i++) {
        if (!(*i).online || &(*i) == me)
            continue;
        // send online list to my local host
        pkg.getUser((*i).ID);
        write(myfd, &pkg, sizeof(PACKAGE));
        // say hi to online guys
        pkg.online(me->ID);
        write((*i).clifd, &pkg, sizeof(PACKAGE));
    }

    // send offline messages
    for (i = ul.begin(); i != ul.end(); i++) {
        if (&(*i) == me)
            continue;
        char filename[80] = "";
        sprintf(filename, "history/%s_%s", me->ID, (*i).ID);
        FILE *h = fopen(filename, "r");
        if (!h)
            continue;
        int lastlogout = -1;
        while (!feof(h)) {
            fscanf(h, "%s", buffer);
            if (!strcmp(buffer, "======LOGOUT======"))
                lastlogout = ftell(h);
        }
        if (lastlogout < 0) {
            fclose(h);
            continue;
        }
        fseek(h, lastlogout, SEEK_SET);
        fgets(buffer, sizeof(buffer), h);
        while (fgets(buffer, sizeof(buffer), h) != NULL) {
            pkg.flag = 'M';
            sscanf(buffer, "%[^:]: %s", pkg.user, pkg.content);
            write(myfd, &pkg, sizeof(PACKAGE));
	    printf("write offline message %s\n",pkg.content);
        }
        fclose(h);
    }	

    //Core zone
    while (1) {
        if (read(myfd, &pkg, sizeof(PACKAGE)) <= 0)
            break;
        if(pkg.flag == 'B'){
	     for (i = ul.begin(); i != ul.end(); i++) {
		  if(strcmp((*i).ID,me->ID)!=0&&(*i).online==1){
		      write((*i).clifd, &pkg, sizeof(PACKAGE));
		  }
	     }
	     continue;
	}
	if (pkg.flag == 'X') { // logout
            break;
        }
        for (i = ul.begin(); i != ul.end(); i++) {
            if (!strcmp((*i).ID, pkg.user)&&strcmp((*i).ID,me->ID)!=0)
                break;
        }
        if (i == ul.end()){ // dst not found
            if(pkg.flag!='A'){
		    continue;
	    }else addf = 0;
	}else addf = 1;
        if(pkg.flag!='A') strcpy(pkg.user, me->ID);
        if (pkg.flag == 'M') { // write hitory msg
            char myfile[80] = "", hisfile[80] = "";
            sprintf(myfile, "history/%s_%s", me->ID, (*i).ID);
            sprintf(hisfile, "history/%s_%s", (*i).ID, me->ID);
            FILE *my = fopen(myfile, "a"), *his = fopen(hisfile, "a");
            fprintf(my, "%s: %s\n", me->ID, pkg.content);
            fprintf(his, "%s: %s\n", me->ID, pkg.content);
            fclose(my);
            fclose(his);
        }else if(pkg.flag == 'A'){		//Add friend
	    if (!addf){
		  buf.flag = 'A';
		  strcpy(buf.content,"ERROR");
		  write(myfd,&buf,sizeof(PACKAGE));
		  puts("add friend fail");
	    }else{
	    	sprintf(buffer,"friend/%s",(*me).ID);
	    	fp = fopen(buffer,"r+");
	    	fscanf(fp,"%s",tmp);
	    	fnum = atoi(tmp);
	    	fclose(fp);
	    	fp = fopen(buffer,"r+");
	    	sprintf(tmp,"%d",fnum+1);
	    	fputs(tmp,fp);
	    	fclose(fp);
	    	fp = fopen(buffer,"a");
		sprintf(tmp,"%s\n",pkg.user);
	    	fputs(tmp,fp);
	    	fclose(fp);
		buf.flag = 'A';
		if((*i).online==1) strcpy(buf.content,"online");
		else strcpy(buf.content,"offline");
		write(myfd,&buf,sizeof(PACKAGE));	//Notice user
		puts("add friend success");
		continue;
	    }
	}
        if ((*i).online == 0) // offline
            continue;
        if (pkg.flag == 'R') // reply
            write((*i).sendfd, &pkg, sizeof(PACKAGE));
	else		     
            write((*i).clifd, &pkg, sizeof(PACKAGE));
    }

    // Logout
    me->online = 0;
    pkg.flag = 'X';
    strcpy(pkg.user, me->ID);

    for (i = ul.begin(); i != ul.end(); i++) {
        if (&(*i) == me)
            continue;
        if ((*i).online) // tell others i'm offline
            write((*i).clifd, &pkg, sizeof(PACKAGE));
        char filename[80] = "";
        sprintf(filename, "history/%s_%s", me->ID, (*i).ID);
        FILE *h = fopen(filename, "a");
        fprintf(h, "======LOGOUT======\n");
        fclose(h);
    }

    printf("%s logout!!\n", (*me).ID);
    close(myfd);
    close(myset.sendfd);
    close(myset.recvfd);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    FILE *userList = fopen("userlist", "r");
    USER temp;
    if (userList) {
        while (!feof(userList)) {
            fread(&temp, sizeof(USER), 1, userList);
            ul.push_back(temp);
        }
        fclose(userList);
    }

    char buffer[MAXBUF] = "";
    int listenfd, sendfd, recvfd;
    socklen_t length;
    struct sockaddr_in serverAddress, sendServer, recvServer, clientAddress;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    sendfd = socket(AF_INET, SOCK_STREAM, 0);
    recvfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[1]));
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(listenfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    listen(listenfd, 128);
    bzero(&sendServer, sizeof(sendServer));
    sendServer.sin_family = AF_INET;
    sendServer.sin_port = htons(atoi(argv[1])+1);
    sendServer.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sendfd, (struct sockaddr *) &sendServer, sizeof(sendServer));
    listen(sendfd, 128);
    bzero(&recvServer, sizeof(recvServer));
    recvServer.sin_family = AF_INET;
    recvServer.sin_port = htons(atoi(argv[1])+2);
    recvServer.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(recvfd, (struct sockaddr *) &recvServer, sizeof(recvServer));
    listen(recvfd, 128);
    pthread_t thread, filetranfer;
    pthread_create(&filetranfer, NULL, FileTransfer, NULL);

    while (1) {
        length = sizeof(clientAddress);
        FDSET fdset;
        printf("wait for connection\n");
        fdset.clifd = accept(listenfd, (struct sockaddr *) &clientAddress, &length);
        printf("wait for connection\n");
        fdset.sendfd = accept(sendfd, (struct sockaddr *) &clientAddress, &length);
        printf("wait for connection\n");
        fdset.recvfd = accept(recvfd, (struct sockaddr *) &clientAddress, &length);
        printf("Server create clifd set: %d, %d, %d\n", fdset.clifd, fdset.sendfd, fdset.recvfd);

        thread = (pthread_t)malloc(sizeof(pthread_t));
        if (pthread_create(&thread, NULL, Client, (void *)&fdset)) {
            perror("clientthread");
            exit(1);
        }
    }
	puts("Server die");
    return 0;
}
