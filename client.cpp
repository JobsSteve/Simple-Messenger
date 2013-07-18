#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>
#include <list>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <netdb.h>

#include "messenger.hh"

#define RSIZE 13
#define c1_HEIGHT 15
#define c1_WIDTH 45
#define c1_YPOS 3
#define c1_XPOS 30
#define c2_HEIGHT 5
#define c2_WIDTH 45
#define c2_YPOS 19
#define c2_XPOS 30
#define MAXF 50

struct RECVFILE {
        char user[20];
        char filename[20];
        FILE *fp;
};

WINDOW *win,*f,*login,*c1,*c2,*c2_in,*file;
int addf = -1;
char id[25];
int mode,sel=0,c2_pos[2],filepos,fnum=-1;
char FL[MAXF][20],user[30];
bool FLonline[MAXF],addonline;
int pipefd[MAXF][2],R13ptr[MAXF]={0};
int Opipefd[2],Fpipefd[2];
char Recent13[MAXF][RSIZE][1024];
int filecome[MAXF];
char Filecome[MAXF][20];
pthread_t thread[MAXF];

typedef std::list<RECVFILE> FILELIST;
FILELIST filelist;

void finish(int fd)
{
	close(fd);
	delwin(win);
	endwin();
	clear();
	puts("Client die peace~");
	exit(0);
}

void Login(int fd,int height,int width,int y,int x)
{
	char buf[1024];
	int i;

	login = subwin(win,height,width,y,x);
	wbkgd(login,COLOR_PAIR(2));
	box(login,0,0);
	mvwaddstr(login,2,2,"Username  : ");
	mvwaddstr(login,3,2,"Password  : ");
	mvwaddstr(login,5,5,"(Enter 'new' to register)");
	refresh();
	mvwgetstr(login,2,14,buf);  //User Login
	if(strcmp(buf,"new")==0){
		mvwaddstr(login,2,14,"    ");
		mvwaddstr(login,5,3,"                                   ");
		mvwaddstr(login,5,12,"~ Register ~");
		wrefresh(login);
		write(fd,buf,sizeof(buf));	//Write "new" to server
		mvwgetstr(login,2,14,buf);	//username
		write(fd,buf,sizeof(buf));
		mvwgetstr(login,3,14,buf);	//password
		write(fd,buf,sizeof(buf));
		read(fd,buf,sizeof(buf));
		wclear(login);
		box(login,0,0);
		if(!strcmp(buf,"ERROR")){
			mvwaddstr(login,2,7,"Username is invalid!");
		        mvwaddstr(login,3,9,"Please try again!");
		}else{
			mvwaddstr(login,2,8,"Registration Success!");
			mvwaddstr(login,3,6,"Please restart to login");
		}
		wrefresh(login);
		sleep(3);
	}else{	
		strcpy(user,buf);
		write(fd,buf,sizeof(buf));  	//Username
		mvwgetstr(login,3,14,buf);	//Password
		write(fd,buf,sizeof(buf));	
		read(fd,buf,sizeof(buf));
		wclear(login);
		box(login,0,0);
		if(!strcmp(buf,"ERROR")){
			mvwaddstr(login,3,5,"Sorry,I don't know you...");
		}else{
			read(fd,buf,sizeof(buf));
			fnum = atoi(buf);
			for(i=1;i<=fnum;i++){
				read(fd,buf,sizeof(buf));
				strcpy(FL[i],buf);
			}
			strcpy(FL[0],"ALL");
			mvwaddstr(login,3,10,"Login success !");
		}
		wrefresh(login);
		sleep(1);
	}
	return;
}
void Windows(int fd,int height,int width,int y,int x)
{
	char buf[1024];
	int i;
	
	wclear(login);
	wbkgd(login,COLOR_PAIR(7));
	wrefresh(login);
	c1 = subwin(win,c1_HEIGHT,c1_WIDTH,c1_YPOS,c1_XPOS);
	c2 = subwin(win,c2_HEIGHT+1,c2_WIDTH,c2_YPOS,c2_XPOS);
	c2_in = subwin(win,c2_HEIGHT,c2_WIDTH-7,c2_YPOS+1,c2_XPOS+6);
	f = subwin(win,height,width,y,x);
	file = subwin(win,1,c2_WIDTH,c2_YPOS-1,c2_XPOS);	

	wbkgd(c1,COLOR_PAIR(4));
	wbkgd(c2,COLOR_PAIR(3));
	wbkgd(c2_in,COLOR_PAIR(3));
	wbkgd(f,COLOR_PAIR(2));
	wbkgd(file,COLOR_PAIR(8));
	box(f,0,0);
	wattron(f,COLOR_PAIR(3));
	mvwaddstr(f,1,6,"Friend List:");
	wattroff(f,COLOR_PAIR(3));
	mvwaddstr(f,3,6,"ALL");
	for(i=1;i<=fnum;i++){
	     mvwaddstr(f,i+3,6,FL[i]);
	     wrefresh(f);
	}
	mvwaddstr(f,3,1,"->");
	sprintf(buf,"%s:",user);
	mvwaddstr(c2,0,1,buf);
	mvwaddstr(file,0,1,"Send file :"); 
	wrefresh(c2);
	wrefresh(c1);
	wrefresh(f);
	wrefresh(file);
}

void ShowFriendList()
{
	wclear(f);
	f = subwin(win,fnum+7,25,3,3);
	wbkgd(f,COLOR_PAIR(2));
	box(f,0,0);
	wattron(f,COLOR_PAIR(3));
	mvwaddstr(f,1,6,"Friend List:");
	wattroff(f,COLOR_PAIR(3));
	mvwaddstr(f,3,6,"ALL");
	for(int i=1;i<=fnum;i++){
	     if(FLonline[i]) wattron(f,COLOR_PAIR(6));
	     else wattron(f,COLOR_PAIR(2));
	     mvwaddstr(f,i+3,6,FL[i]);
	     if(FLonline[i]) wattroff(f,COLOR_PAIR(6));
	     else wattroff(f,COLOR_PAIR(2));
	     wrefresh(f);
	}
	mvwaddstr(f,3,1,"->");
	sel = 0;
	wrefresh(f);
	return;
}

void ReturnPos()
{
	    if(mode == 0){
		wmove(f,3+sel,3);
		wrefresh(f);
	    }else if(mode == 1){
		wmove(c2_in,c2_pos[0],c2_pos[1]);
		wrefresh(c2_in);
	    }else if(mode == 2){
		wmove(file,c2_YPOS-1,13+filepos);
		wrefresh(file);
	    }else if(mode == 3){
		    wmove(win,c1_YPOS-1,c1_XPOS+strlen(Filecome[sel]));
		    wrefresh(win);
	    }
	    return;
}

void *Chat(void *num)
{	
	int pipenum = (intptr_t)num,i,j;
	int len,tmp,flag=0;
	char content[1024],c,line[25];
	PACKAGE buf;
	RECVFILE temp;

    while(read(pipefd[pipenum][0],&buf,sizeof(buf))>0){
	if(buf.flag=='M'||buf.flag=='B'){
	     len = strlen(buf.content);  //contentent
	     sprintf(line,"%s:",buf.user);
	     strcpy(Recent13[pipenum][R13ptr[pipenum]],line);
   	     R13ptr[pipenum] = (R13ptr[pipenum]+1)%RSIZE;
	     if(len>(c1_WIDTH-7)){
		   for(i=0;i<len;i+=(c1_WIDTH-7-flag)){
		      for(j=i;j<i+(c1_WIDTH-7);){
			    if(buf.content[j]>0) j++;  //ASCII
			    else j+=2;              //Chinese
		      }
		      if(j != (i+c1_WIDTH-7)) flag = 1;
		      else flag = 0;
		      c = buf.content[i+(c1_WIDTH-7-flag)];
		      buf.content[i+(c1_WIDTH-7-flag)] = '\0';
		      sprintf(content,"     %s",&(buf.content[i]));
	     	      strcpy(Recent13[pipenum][R13ptr[pipenum]],content);
	    	      R13ptr[pipenum] = (R13ptr[pipenum]+1)%RSIZE;
		      buf.content[i+(c1_WIDTH-7-flag)] = c;
		   }
	     }else{
		   sprintf(content,"     %s",buf.content);
	           strcpy(Recent13[pipenum][R13ptr[pipenum]],content);
	           R13ptr[pipenum] = (R13ptr[pipenum]+1)%RSIZE;
	     }
	     if(mode != 0 &&sel==pipenum){
		wclear(c1);	     	
		for(i=0;i<RSIZE;i++){
		     mvwaddstr(c1,i+1,1,Recent13[pipenum][(R13ptr[pipenum]+i)%RSIZE]);
	    	}
	     	wrefresh(c1);
		ReturnPos();
	     }else{
		wattron(f,COLOR_PAIR(1));
		mvwaddstr(f,pipenum+3,7+strlen(FL[pipenum]),"(NEW)");
		wattroff(f,COLOR_PAIR(1));
		wrefresh(f);
	     	ReturnPos();
	     }
	}else if(buf.flag == 'Q'){
	     strcpy(temp.user, buf.user);
             strcpy(temp.filename, buf.content);
             filelist.push_back(temp);
	     sprintf(content,"Accept \"%s\" ? (Y/N) : ",buf.content);
	     strcpy(Filecome[pipenum],content);
	     filecome[pipenum] = 1;
	     if((sel==pipenum)){
		mvwaddstr(win,c1_YPOS-1,c1_XPOS,content);
		wrefresh(win);
	     	ReturnPos();
	     }else{
		wattron(f,COLOR_PAIR(1));
		mvwaddstr(f,pipenum+3,7+strlen(FL[pipenum]),"(NEW)");
		wattroff(f,COLOR_PAIR(1));
		wrefresh(f);
	        ReturnPos();
	     }
	}
     }
	close(pipefd[pipenum][0]);
}

void *OnlineNotice(void*)
{
	char c,msg[40];
	int i,prev;
	PACKAGE buf;

	while(read(Opipefd[0],&buf,sizeof(buf))>0){
		for(i=1;i<=fnum;i++){
		     if(strcmp(FL[i],buf.user)==0){
			 wattron(win,COLOR_PAIR(5));
		     	 if((buf.flag == 'O')||(buf.flag =='G')){
				 FLonline[i] = 1;
				 if(buf.flag=='O'){
				 	sprintf(msg,"%s is online!",FL[i]);
				        mvwaddstr(win,26,30,msg);
				 }
				 wattron(f,COLOR_PAIR(6));
				 mvwaddstr(f,3+i,6,FL[i]);
				 wattroff(f,COLOR_PAIR(6));
			 }else if(buf.flag == 'X'){
				 FLonline[i] = 0;
				 sprintf(msg,"%s is offline...",FL[i]);
				 wattron(f,COLOR_PAIR(2));
				 mvwaddstr(f,3+i,6,FL[i]);
				 wattroff(f,COLOR_PAIR(2));
				 mvwaddstr(win,26,30,msg);
			 }
			 wattroff(win,COLOR_PAIR(5));
			 wrefresh(win);
			 wrefresh(f);
		 	 ReturnPos();
			 sleep(5);
		 	 mvwaddstr(win,26,30,"                                   ");
			 wrefresh(win);
			 ReturnPos();
			 break;
		     }
		 }
	}
}

void *InputHandle(void *sockfd)
{
	int i,j,k,fd,len,prevsel;
	char c,buf[1024],buff[1024],line[25],chinese[3],haha[c2_HEIGHT-1][50];
	PACKAGE bbuf;
	FILELIST::iterator itr;
	fd = (intptr_t)sockfd;

	sprintf(line,"%s:",user);
	while(1){
	   if(mode == 0){	//in Friend List
		    wrefresh(f);
		    c = getch();
		    if(c==3){
			     	if(sel-1<0) sel = fnum;
		   	    	else sel--;
			    	mvwaddstr(f,3+((sel+1)>fnum?0:sel+1),1,"  ");
			   	mvwaddstr(f,3+sel,1,"->");
		    }else if(c==2){
			 	sel = (sel+1)%(fnum+1);
		   	    	mvwaddstr(f,3+((sel-1)<0?fnum:sel-1),1,"  ");
			    	mvwaddstr(f,3+sel,1,"->");
		    }else if(c==4){
		    }else if(c==5||c=='\n'){
		   		mode = 1;  //Press '->'or tab,Enter chat mode(1)
		    		mvwaddstr(f,sel+3,7+strlen(FL[sel]),"     ");
				    mvwaddstr(win,c1_YPOS-1,c1_XPOS,Filecome[sel]);
				    for(i=0;i<RSIZE;i++){
	         		         mvwaddstr(c1,i+1,1,Recent13[sel][(R13ptr[sel]+i)%RSIZE]);
	    			    }
	     			    wrefresh(c1);	
				    wrefresh(win);
				wrefresh(f);
		    }else if(c=='a'||c=='A'){
				mvwaddstr(f,5+fnum,1,"new:");
				wrefresh(f);
				echo();
				mvwgetstr(f,5+fnum,6,buf);
				noecho();
				if(strcmp(buf,"quit")!=0){
					bbuf.flag = 'A';
					strcpy(bbuf.user,buf);
					write(fd,&bbuf,sizeof(PACKAGE));
					while(addf==-1){}
					if(addf == 1){
						fnum++;
						if(pipe(pipefd[fnum])==-1){
   	    	     					perror("pipe");
	    	     					exit(1);
        					}
        					pthread_create(&thread[fnum],NULL,Chat,(void*)fnum);
						strcpy(FL[fnum],buf);
						FLonline[fnum] = addonline;
						ShowFriendList();
					}else{
						mvwaddstr(f,5+fnum,1,"                     ");
						ReturnPos();
					}
					addf = -1;
				}
		    }
	   }else if(mode == 1){  //in chat mode
		    wmove(c2_in,0,0);
		    wrefresh(c2_in);
		    i=0;j=0;k=0;
		    do {
			   if((j >= c2_WIDTH-7)&&(i < c2_HEIGHT-1)){
			   	  haha[i][j] = '\0';
				  i++;
				  j=0;
			   }
			   c2_pos[0] = i;
			   c2_pos[1] = j;
			   c = getch();
			   if(i < c2_HEIGHT-1){
			      if(isprint(c)){
				  buf[k++] = c;
				  haha[i][j] = c;
				  mvwaddch(c2_in,i,j++,c);
			      }else if(c<0){     //Chinese
				  buf[k++] = c;
				  chinese[0] = c;
				  c = getch();
				  buf[k++] = c;
				  chinese[1] = c;
				  chinese[2] = '\0';
				  if(j==c2_WIDTH-8) {i++;j=0;}
				  strcpy(&haha[i][j],chinese);
				  mvwaddstr(c2_in,i,j,chinese);
				  j+=2;
			      }else if(c == 9){ //tab
					mode = 2;
					break;
		    	      }else if(c == 27){
 			   		mode = 0;
			   		wmove(f,3+sel,3);
			   		wclear(c1);
					break;
		    	      }
			      wrefresh(c2_in);
			   }
		    }while(c!='\n');
		    buf[k]='\0';
		    haha[i][j] = '\0';
		    if(j==0 && i>0) i--;
		    wclear(c2_in);
		    if(mode==1){
			strcpy(Recent13[sel][R13ptr[sel]],line);
			R13ptr[sel] = (R13ptr[sel]+1)%RSIZE;
			for(j=0;j<=i;j++){   //i+1 stands for how many lines
			      sprintf(buff,"     %s",haha[j]);
	     		      strcpy(Recent13[sel][R13ptr[sel]],buff);
	    		      R13ptr[sel] = (R13ptr[sel]+1)%RSIZE;
		 	}
	     		wclear(c1);
	     		for(i=0;i<RSIZE;i++){
	         		 mvwaddstr(c1,i+1,1,Recent13[sel][(R13ptr[sel]+i)%RSIZE]);
	     		}
			if(sel==0){
				bbuf.flag = 'B';
				strcpy(bbuf.user,user);
				strcpy(bbuf.content,buf);
			}else{ 
				bbuf.flag = 'M';
				strcpy(bbuf.user,FL[sel]);
				strcpy(bbuf.content,buf);
			}
		    	write(fd,&bbuf,sizeof(bbuf));
		    }
		    wrefresh(c1);
		    wrefresh(c2_in);
 	   }else if(mode == 2){		//File Transfer
		    wmove(file,0,13);
		    wrefresh(file);
		    k = 0;filepos=0;
	   	    do{
		    	c = getch();
			if(isprint(c)){
			   buf[k] = c;
			   mvwaddch(file,0,k+13,c);
			   wrefresh(file);	
			   k++;
			   filepos++;
		    	}else if(c == 9){  //tab
 			   break;
		    	}
		    }while(c!='\n');
		    buf[k] = '\0';    //filename
		    if(c!=9){	      //Start sending file
			sprintf(buff,"%s:%s",FL[sel],buf);
			write(Fpipefd[1],buff,sizeof(buff));
		    }
		    if(filecome[sel] == 1 && c==9) mode = 3;
		    else mode = 1;
		    mvwaddstr(file,0,13,"                                                     ");
		    wrefresh(file);	    
	   }else if(mode ==3){
		    wmove(win,c1_YPOS-1,c1_XPOS+strlen(Filecome[sel]));
		    wrefresh(win);
		    c = getch();
		    if(c == 9){
			mode = 1;
		    }else{
			bbuf.flag = 'R';
                	for (itr = filelist.begin(); itr != filelist.end(); itr++){
                    		if (!strcmp((*itr).user, bbuf.user))
                        		break;
			}
			if(c == 'N'|| c == 'n'){
				filelist.erase(itr);
				strcpy(bbuf.content,"N");
			}else{
				strcpy(bbuf.content,"Y");
				(*itr).fp = fopen((*itr).filename, "w");
			}
			write(fd,&bbuf,sizeof(PACKAGE));
			mvwaddstr(win,c1_YPOS-1,c1_XPOS,"                                                ");
			wrefresh(win);
			filecome[sel] = 0;
			strcpy(Filecome[sel],"");
			mode = 1;
		    }
	   }
	}
}

void* Send(void* sockfd)
{
    int i,sendfd = (intptr_t)sockfd,tempsel,numbytes;
    PACKAGE pkg;
    char buffer[MAXBUF] = "",receiver[30],sendfilename[50];
    FILE *fp;
    while (read(Fpipefd[0],buffer,sizeof(buffer))>0) {
        sscanf(buffer,"%[^:]:%s",receiver,sendfilename);
	fp = fopen(sendfilename, "r");
        if (fp == NULL) {
     	    strcpy(Recent13[sel][R13ptr[sel]],"(Sending file fail! File Not exit!)");
	    R13ptr[sel] = (R13ptr[sel]+1)%RSIZE;
	    for(i=0;i<RSIZE;i++){
        	 mvwaddstr(c1,i+1,1,Recent13[sel][(R13ptr[sel]+i)%RSIZE]);
  	    }
	    wrefresh(c1);
	    ReturnPos();
	    continue;
        }
        pkg.flag = 'Q';
        strcpy(pkg.user, receiver);
        strcpy(pkg.content, sendfilename);

	sprintf(buffer,"(Sending \"%s\", waiting for approval...)",sendfilename);
    	strcpy(Recent13[sel][R13ptr[sel]],buffer);
	R13ptr[sel] = (R13ptr[sel]+1)%RSIZE;
	wclear(c1);
	for(i=0;i<RSIZE;i++){
        	mvwaddstr(c1,i+1,1,Recent13[sel][(R13ptr[sel]+i)%RSIZE]);
  	}
	wrefresh(c1);
	ReturnPos();
	tempsel = sel;
        write(sendfd, &pkg, sizeof(PACKAGE));
        if (read(sendfd, &pkg, sizeof(PACKAGE)) < 0 || !strcmp(pkg.content, "N")) {
	    sprintf(buffer,"(Sending \"%s\" fail! Not approved!)",sendfilename);
	    strcpy(Recent13[tempsel][R13ptr[tempsel]],buffer);
	    R13ptr[tempsel] = (R13ptr[tempsel]+1)%RSIZE;
	    if(tempsel == sel){
		wclear(c1);
		for(i=0;i<RSIZE;i++)
        		mvwaddstr(c1,i+1,1,Recent13[tempsel][(R13ptr[tempsel]+i)%RSIZE]);
	    }
	    wrefresh(c1);
	    ReturnPos();
            fclose(fp);
            continue;
        }
        while(!feof(fp)) {
            numbytes = fread(buffer, 1, sizeof(buffer), fp);
            pkg.file(receiver, buffer, numbytes);
            write(sendfd , &pkg, sizeof(PACKAGE));
        }
        pkg.file(receiver, "", 0);
        write(sendfd , &pkg, sizeof(PACKAGE));
        sprintf(buffer,"(Sending \"%s\" complete!)",sendfilename);
	strcpy(Recent13[tempsel][R13ptr[tempsel]],buffer);
	R13ptr[tempsel] = (R13ptr[tempsel]+1)%RSIZE;
	if(tempsel == sel){
	     wclear(c1);
	     for(i=0;i<RSIZE;i++)
        	mvwaddstr(c1,i+1,1,Recent13[tempsel][(R13ptr[tempsel]+i)%RSIZE]);
	     wrefresh(c1);
	}
	ReturnPos();
        fclose(fp);
    }
    pthread_exit(NULL);
}

void* Recv(void* sockfd)
{
    int recvfd = (intptr_t)sockfd;
    PACKAGE pkg;
    char buffer[MAXBUF] = "";
    FILELIST::iterator i;
    while (1) {
        read(recvfd, &pkg, sizeof(PACKAGE));
        for (i = filelist.begin(); i!=filelist.end(); i++)
            if (!strcmp(pkg.user, (*i).user))
                break;
        if (i == filelist.end())
            continue;
        if (!pkg.size) {
            fclose((*i).fp);
            filelist.erase(i);
	    sprintf(buffer,"Receiving \"%s\" complete!",(*i).filename);
	    mvwaddstr(win,c1_YPOS-1,c1_XPOS,buffer);
	    wrefresh(win);
	    ReturnPos();
	    sleep(3);
            mvwaddstr(win,c1_YPOS-1,c1_XPOS,"                                            ");
	    wrefresh(win);
	    ReturnPos();
	}
        else
            fwrite(pkg.content, 1, pkg.size, (*i).fp);
    }
    pthread_exit(NULL);
}

int main(int argc,char *argv[])
{
	if(argc < 2 || !strcmp(argv[1],"-h")){
		printf("\n\n===================Manual===================\n");
		printf("You should specify the port# (ex:./c 5555)\n");
		printf("Use 'new' to register");
		printf("When you login successfully,\nUse arrow key, tab, and Esc to change mode\n\n\n");
		return 0;
	}

	FILE *fp;
	int i,fd,sendfd,recvfd,rc;
	struct sockaddr_in srv,sendsrv,recvsrv;
	struct hostent *hp;
	char *name = "linux5.csie.org",buf[1024];
	PACKAGE pkg;
	pthread_t ONthread,INthread,Sendthread,Recvthread;

	if((fd = socket(AF_INET,SOCK_STREAM,0))<0||(sendfd = socket(AF_INET,SOCK_STREAM,0))<0||
						   (recvfd = socket(AF_INET,SOCK_STREAM,0))<0){
		perror("socket");
		exit(1);
	}
	srv.sin_family = AF_INET;
	srv.sin_port = htons(atoi(argv[1]));
	sendsrv.sin_family = AF_INET;
	sendsrv.sin_port = htons(atoi(argv[1])+1);
	recvsrv.sin_family = AF_INET;
	recvsrv.sin_port = htons(atoi(argv[1])+2);
	//srv.sin_addr.s_addr = inet_addr("140.112.172.4");
	if((hp = gethostbyname(name))!=NULL){
		srv.sin_addr.s_addr=((struct in_addr*)(hp->h_addr))->s_addr;
		sendsrv.sin_addr.s_addr=((struct in_addr*)(hp->h_addr))->s_addr;
		recvsrv.sin_addr.s_addr=((struct in_addr*)(hp->h_addr))->s_addr;
	}
	if(connect(fd,(struct sockaddr*)&srv,sizeof(srv))<0||
	   connect(sendfd,(struct sockaddr*)&sendsrv,sizeof(sendsrv))<0||
	   connect(recvfd,(struct sockaddr*)&recvsrv,sizeof(recvsrv))<0){
		perror("connect");
		exit(1);
	}
	if((win = initscr()) == NULL){
		perror("initial curse");
		exit(1);
	}
	start_color();
	init_pair(1,COLOR_RED,COLOR_CYAN);//for background
	init_pair(2,COLOR_BLACK,COLOR_CYAN);
	init_pair(3,COLOR_BLACK,COLOR_WHITE);
	init_pair(4,COLOR_BLACK,COLOR_YELLOW);
	init_pair(5,COLOR_WHITE,COLOR_MAGENTA);
	init_pair(6,COLOR_BLACK,COLOR_GREEN);
	init_pair(7,COLOR_BLACK,COLOR_BLACK);
	init_pair(8,COLOR_BLACK,COLOR_MAGENTA);
	keypad(win,TRUE);
	Login(fd,7,35,5,10);
	if(fnum < 0) finish(fd);
	noecho();
	//Start initializingi
	Windows(fd,fnum+7,25,3,3);
        for(i=0;i<=fnum;i++){      //Chat thread
		if(pipe(pipefd[i])==-1){
   	    	     perror("pipe");
	    	     exit(1);
        	}
        	pthread_create(&thread[i],NULL,Chat,(void*)i);
	}

	if(pipe(Opipefd)==-1||pipe(Fpipefd)==-1){
		perror("pipe");
		exit(1);
	}

	pthread_create(&ONthread,NULL,OnlineNotice,NULL);
	pthread_create(&INthread,NULL,InputHandle,(void *)fd);
	pthread_create(&Sendthread,NULL,Send,(void *)sendfd);
	pthread_create(&Recvthread,NULL,Recv,(void *)recvfd);
	//End initializing
	while(1){
		if(read(fd,&pkg,sizeof(PACKAGE))<=0) break;
		if((pkg.flag=='O')||(pkg.flag=='X')||(pkg.flag=='G')){		//Onlnie/Offline
			write(Opipefd[1],&pkg,sizeof(PACKAGE));
		}else if((pkg.flag=='M')||(pkg.flag=='Q')||(pkg.flag=='B')){		//Message
		    if(pkg.flag=='M'||pkg.flag=='Q'){ 
			for(i=1;i<=fnum;i++){
			    if(strcmp(pkg.user,FL[i])==0){  //判斷要丟給哪個pipe
				  write(pipefd[i][1],&pkg,sizeof(PACKAGE));
				  break;
			    }
			}
		    }else if(pkg.flag=='B'){
			    write(pipefd[0][1],&pkg,sizeof(PACKAGE));
		    }
		}else if(pkg.flag=='L'){		//Logout
			return 0;
		}else if(pkg.flag=='A'){
			if(!strcmp(pkg.content,"ERROR")) addf = 0;
			else{
			    addf = 1;
			    if(!strcmp(pkg.content,"online")) addonline = 1;
			    else if(!strcmp(pkg.content,"offline")) addonline = 0;
			}
		}
	}

	clear();
	refresh();
		
	finish(fd);
	return 0 ;
}
