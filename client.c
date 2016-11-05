/*
http://stackoverflow.com/questions/15673846/how-to-give-to-a-client-specific-ip-address-in-c
*/
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>

#define STDIN 0
#define N 20

int s;
void ouch(int sig);


int main(int arg, char** argv)
{	

	char hostname[N];
	strcpy(hostname, argv[1]);
	int portNumber = atoi(argv[2]);

	char userName[N];
	int nameSize = strlen(argv[3]);
	strcpy(userName, argv[3]);

	//Setup timer
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	//Setup Signal handler;
	struct sigaction act;
	act.sa_handler = ouch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	sigaction(SIGINT, &act, 0);

	char buf[N];
	//Setup comm
	int	s, number;

	struct	sockaddr_in	server;

	struct	hostent		*host;

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(hostname);
	
	if (host == NULL) {
		perror ("Client: cannot get host description");
		exit (1);
	}

	s = socket (AF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		perror ("Client: cannot open socket");
		exit (1);
	}
	server.sin_port = htons (portNumber);

	if (connect (s, (struct sockaddr*) & server, sizeof (server))) {
		perror ("Client: cannot connect to server");
		exit (1);
	}

	//Handshake
	unsigned char handshake[2];
	read(s, handshake, 2);
	
	if(handshake[0] == 0xCF && handshake[1] == 0xA7) {
		printf("Authentication Succeed!\n");

	} else {
		printf("Authentication Failed!\n");
		exit(1);
	}
	//Get number of users
	char ccount;
	read(s, &ccount, sizeof(char));
	int count = ccount - 0x00;
	printf("Current number of client: %d \n", count);

	int i;
	char readIn;
	printf("Existing Users: \n");
	
	//Get user lists
	for(i=0;i<count; i++){
		read(s, &readIn, sizeof(char));
		int size = readIn - 0x00;
		read(s, buf, size);
		printf("User: %d %.*s \n", size, size, buf);
	}

	//Need to implement username
	char nSize;

	nSize = nameSize + 0x00;
	write(s, &nSize, sizeof(char));
	write(s, userName, nameSize);

	//Set up fd
	fd_set read_fds;
	fd_set temp;
	fd_set read_stdin;
	int nbytes;

	//Read confirmation
	char rec;
	read(s, &rec, sizeof(char));
	if(rec == 0x02){
		printf("Conflict username, BYE BYE \n");
		exit(0);
	}

	//Join to chat
	count++;
	printf("Now, you are joined to the chart. Current number of users: %d \n", count);
	while(1){
		FD_ZERO(&read_fds);
		FD_SET(s, &read_fds);
		FD_SET(STDIN, &read_fds);

		temp = read_fds;
		if(select(s+1, &read_fds, NULL, NULL, &tv) == -1){
			perror("Select");
			exit(-1);
		}
		//Incoming message from Socket
		if(FD_ISSET(s, &read_fds)){
			char rec;
			int size;
			nbytes = read(s, &rec, sizeof(char));
			if(nbytes <= 0){
				printf("Connection broken, BYE BYE \n");
				close(s);
				exit(0);
			}
			if(rec == 0x00){
				char nameSize, messageSize;
				int msgSize;
				short messageL;

				//Get user name
				read(s, &nameSize, sizeof(char));
				size = nameSize - 0x00;
				char name[size];
				read(s, name, size);

				//Get message
				read(s, &messageL, sizeof(short));
				msgSize = ntohs(messageL);
				read(s, buf, msgSize);
				printf("%.*s: %.*s \n", size, name, msgSize, buf);
			}

			if(rec == 0x01){
				count ++;
				char nameSize;
				read(s, &nameSize, sizeof(char));
				size = nameSize - 0x00;
				read(s, buf, size);
				printf("User: %.*s had joined, current number of user: %d\n",size, buf, count);
				//continue;
			}
			if(rec == 0x02){
				count --;
				char nameSize;
				read(s, &nameSize, sizeof(char));
				size = nameSize - 0x00;
				read(s, buf, size);
				printf("User: %.*s exited, current number of users: %d \n", size, buf, count);
				//continue;
			}

		}

		//Incoming message from stdin
		if(FD_ISSET(STDIN, &read_fds)){
			//send
			short messageL;
			int size;
			
			if(!scanf("%d", &size)){
				printf("Invalid entry, BYE BYE");
				break;
			}
			if(size >=20){
				printf("Message too long BYE BYE\n");
				break;
			}
			if(size == 0){
				messageL = htons(size);
				char send = 0x00 + size;
				write(s, &messageL, sizeof(short));
				continue;
			}
			scanf("%d, %s", &size, buf);
			
			if(!scanf("%s", buf)){
				printf("Invalide entry, BYE BYE \n");
				break;
			};
			
			if(size < 1){
				printf("Invalid entry, BYE BYE \n");
				break;
			}
			if(size != 0){
				char send = 0x00 + size;
				messageL = htons(size);
				write(s, &messageL, sizeof(short));
				write(s, buf, size);
			}
			
		}

	}
	close(s);
}

void ouch(int sig){
	printf("Exit, BYE BYE \n");
	close(s);
	exit(0);
}
