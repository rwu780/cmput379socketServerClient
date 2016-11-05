/*
** selectserver.c -- a cheezy multiperson chat server
** Link: http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
** Modified by: mwaqar
*/

/*
** Log file from http://www.falloutsoftware.com/tutorials/c/c1.htm
*/
/*
http://stackoverflow.com/questions/4214314/get-a-substring-of-a-char
*/
/*
http://stackoverflow.com/questions/15673846/how-to-give-to-a-client-specific-ip-address-in-c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>


//Define constant
#define TIMER 30
#define N 20

struct userInfo
{
	int fd;
	int length;
	char userName[N];
	time_t lastActive;
};

//Global Variable
fd_set master;
fd_set read_fds;
struct userInfo users[N];
int listener;
char logMessage[100];
int fdmax;
int count = 0;
char LOGFILE[N];

//Declare function
void newUserJoined(int fdmax, int listener, fd_set master, int index);
void userLeave(int fdmax, int listener, fd_set master, int oldfd);
void sendUserList(int newfd);
int updateUserList(int newfd);
int findIndex(int fd);
void sendHandShake(int newfd, int count);
void Log(char *message);	//logs a message to LOGFILE
void ouch(int sig);


int main(int arg, char** argv)
{
	//Setup Logfile
	sprintf(LOGFILE, "server379pro%d.log",getpid());	
	
	//Get Port number
	int port = atoi(argv[1]);
	
	//Setup Signal handler;
	struct sigaction act;
	act.sa_handler = ouch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, 0);
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000; 

	int newfd;        // newly accept()ed socket descriptor
	struct sockaddr_in sa; 
	struct sockaddr_in remoteaddr; // client address
	socklen_t addrlen;

	char buf[256];    // buffer for client data
	int nbytes;

	int i, j;

	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	
	// get us a socket and bind it
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(port);

	if (bind(listener, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		Log("Bind error, program exit \n");
		exit(0);
	}

	// listen
	if (listen(listener, 10) == -1) {
		Log("Listen error, program exit \n");
		exit(0);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	Log("===Server 379 start=== \n");
	Log("Server address: 127.0.0.1 \n");
	sprintf(logMessage, "Port number bind: %d \n", port);
	Log(logMessage);

	// main loop
	while(1) {
		read_fds = master; // copy it
		if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
			Log("Select error: program exit \n");
			exit(0);
		}
		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				//Handle new connection
				if (i == listener) {
					addrlen = sizeof remoteaddr;
					newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

					Log("New connection, ");
					sprintf(logMessage, "File descriptor: %d \n", newfd);
					if (newfd == -1) {
						perror("accept");
					} else {
						Log("Send handshake, ");
						sendHandShake(newfd, count);
						Log("Send userlist, ");
						sendUserList(newfd);
						int index = updateUserList(newfd);

						if(index == -1){
							char s = 0x02;
							write(newfd, &s, sizeof(char));
							close(newfd);
							continue;
						}else{

							char s = 0x00;
							write(newfd, &s, sizeof(char));
						}
						newUserJoined(fdmax, listener, master, index);
						FD_SET(newfd, &master); // add to master set
						count++;
						sprintf(logMessage, "Current number of connections: %d \n", count);
						Log(logMessage);

						if (newfd > fdmax) {    // keep track of the max
							fdmax = newfd;
						}

						sprintf(logMessage,"selectserver: new connection from %s:%d on socket %d\n",
							inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port), newfd);
						Log(logMessage);
					}
				} 
				//Handle data from user
				else {
					short messageL;
					//char messageSize;
					nbytes = read(i, &messageL, sizeof(short));
					if(nbytes <=0){
						close(i);
						FD_CLR(i, &master);
						count --;
						userLeave(fdmax, listener, master, i);	
					}
					else{
						int index = findIndex(i);
						time(&(users[index].lastActive));
						int mSize = ntohs(messageL);
						
						if(mSize == 0){
							continue;
						}
						else{
							read(i, buf, mSize);

							for(j = 0; j<=fdmax;j++){
								if(FD_ISSET(j, &master)){
									if(j!= listener){
										//Send chat message
										char sendChat = 0x00;
										write(j, &sendChat, sizeof(char));
										int userindex = findIndex(i);
										char userNamesize = users[userindex].length - 0x00;
										//Write username
										write(j, &userNamesize, sizeof(char));
										write(j, users[userindex].userName, users[userindex].length);
										//Write message
										write(j, &messageL, sizeof(short));
										write(j, buf, mSize);
									}
								}
							}
						}
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors

		//Check connections
		for(i = 0; i<N; i++){
			if(users[i].fd != 0){
				double diff_t;
				time_t current;
				time_t now;
				time(&current);
				diff_t = difftime(current, users[i].lastActive);

				if(diff_t >=TIMER){
					Log("Connection time out;");
					FD_CLR(users[i].fd, &master);
					close(users[i].fd);
					count --;
					userLeave(fdmax, listener, master, users[i].fd);
					diff_t = 0;
				}
			}
		}//End loop through connections
	} // END while(1)
	return 0;
}

//Send handshake
void sendHandShake(int newfd, int count){
	unsigned char handshake[2];
	handshake[0] = 0xCF;
	handshake[1] = 0xA7;
	write(newfd, handshake, 2);

	int outlength = count + 0x00;
	//Send number of user
	write(newfd, &outlength, sizeof(char));
}
//Find user list index with fd.
int findIndex(int fd){
	int i, userindex;
	for(i = 0; i<N; i++){
		if(users[i].fd == fd){
			userindex = i;
			break;
		}
	}
	return userindex;
}

//Send user list to new join client
void sendUserList(int newfd){
	int i;
	char sLength;

	for(i = 0; i<N; i++){
		if(users[i].fd != 0){
			sLength = 0x00+users[i].length;
			write(newfd, &sLength, sizeof(char));
			write(newfd, users[i].userName, users[i].length);
		}
	};
}

//Add user to user list
int updateUserList(int newfd){
	char nameSize;
	read(newfd, &nameSize, sizeof(char));
	int size = nameSize - 0x00;
	char userName[size+1];
	read(newfd, userName, size);
	
	int i;
	int index = -1;
	for(i = 0; i<N; i++){
		if(users[i].fd == 0 && index == -1){
			index = i;	
		}

		if(users[i].fd != 0){
			if(!strcmp(users[i].userName, userName)){
				Log("New user had a username already existed in the chat. ");
				sprintf(logMessage, "Exit User: %.*s \n", size, userName);
				Log(logMessage);
				return -1;
			}
		}
	}
	
	users[index].fd = newfd;
	users[index].length = size;
	strcpy(users[index].userName, userName);

	time(&(users[index].lastActive));

	Log("New user joined to the chat. ");
	sprintf(logMessage, "Joined User: %.*s \n", users[index].length, users[index].userName);
	Log(logMessage);
	return index;

}

//Send to everyone when a uesr joined
void newUserJoined(int fdmax, int listener, fd_set master, int index){
	
	char s = 0x01;
	char nameSize = users[index].length + 0x00;

	int j;
	for(j=0; j<= fdmax; j++){
		if(FD_ISSET(j, &master)){
			if(j!= listener){
				write(j, &s, sizeof(char));
				write(j, &nameSize, sizeof(char));
				write(j, users[index].userName, users[index].length);
			}
		}
	}
	Log("Send user update to existing clients \n");
}

//Send to everyone when a user leave
void userLeave(int fdmax, int listener, fd_set master, int oldfd){
	int j;
	int index = findIndex(oldfd);

	char nameSize = users[index].length + 0x00;
	char s = 0x02;
	for(j = 0; j<=fdmax; j++){
		if(FD_ISSET(j, &master)){
			if(j != listener){
				write(j, &s, sizeof(char));
				write(j, &nameSize, sizeof(char));
				write(j, users[index].userName, users[index].length);
			}
		}
	}
	sprintf(logMessage, "User: %.*s exits \n",users[index].length, users[index].userName);
	Log(logMessage);
	sprintf(logMessage, "Currnet number of connection: %d \n", count);
	Log(logMessage);

	//Remove user from user list;
	users[index].fd = 0;
	users[index].length = 0;
	strcpy(users[index].userName, "");
}

//Log message
void Log(char *message){
	
	FILE *file;

	if((file = fopen(LOGFILE, "a+"))){
		//Do nothing
	}
	else{
		file = fopen(LOGFILE, "w");
	}
	fputs(message, file);
	fclose(file);
}

void ouch(int sig){

	close(listener);
	int i;
	for(i = 0; i<fdmax; i++){
		if(FD_ISSET(i, &master)){
			close(i);
		}
	}
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	Log("Terminating ...... \n");
	exit(0);
}


