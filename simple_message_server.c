/**
 * @file simple_message_server.c
 * Verteilte Systeme - TCP/IP Programmierübung
 * tcpip server
 *
 * @author Claudia Madzak - ic16b028 - claudia.madzak@technikum-wien.at
 * @author Salko Nuhanovic - ic17b064 - salko.nuhanovic@technikum-wien.at
 *
 * @date 2017/12/10
 *
 * @version 0.14
 */

 /*
 * -------------------------------------------------------------- includes --
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <netinet/in.h>

/*
 * --------------------------------------------------------------- defines --
 */

 /**
 * How many pending connections queue will hold
 */

 #define BACKLOG 10

/*
 * -------------------------------------------------------------- typedefs --
 */

/*
 * --------------------------------------------------------------- globals --
 */

/**
 * path to the business logic executable
 */
static const char * const business_logic_path = "/usr/local/bin/simple_message_server_logic";
/**
 * name of the business logic application
 */
static const char * const business_logic = "simple_message_server_logic";

/*
 * ------------------------------------------------------------- functions --
 */

/**
 * @brief initializes the socket
 * @details handles the socket(), bind() & listen() SysCalls
 *
 * @param port Portnumber
 * @return static int socketfiledescriptor
 */

static int createSocket(char *port);

/**
 * @brief handles the accept() SysCall and the forking
 * @param int socketfd
 */

static void execServer(int socketfd);

/**
 * @brief handles the Child Processes
 * @details prevents the occurrence of Zombie-Processes
 * @param int s signal
 */

static void sigchld_handler(int s);

 /**
 * @brief prints a usage message to stdout or stderr
 * @details prints the usage message and then calls exit()
 *
 * @param Stream stderr or stdout
 * @param progName prints the Programname to stdout, stderr
 * @param exitCode the exitcode used for exit
 */

static void printUsage(FILE *stream, const char *progName, int exitCode);

/**
 * @brief main function
 *
 * @param argc number of arguments
 * @param argv arguments itselves (including the name of program in argv[0])
 *
 * @return 0 - if successful, otherwise != 0
 *
 */

int main(int argc,char *argv[]) {

	char *port = NULL;
	int socketfd = -1;
	int args;
	int portNumber =0;
	char * endptr = NULL;

	if(argc != 3){ //checks the argument-count
		printUsage(stderr, argv[0], EXIT_FAILURE);
	}

	while ((args = getopt(argc, argv, "p:h")) != -1) //getopt parses the commandline arguments
	{
		switch (args)
		{
			case 'p':
				port = optarg; //optarg holds the portnumber-string
				if(port == NULL)
				{
					printUsage(stderr, argv[0], EXIT_FAILURE);
				}
				errno = 0;
				portNumber = strtol(port, &endptr, 10);//converts the port-string into a int, according base 10 (decimal)
				if(errno != 0) {
					printUsage(stderr, argv[0], EXIT_FAILURE);
				}
				if(*endptr != '\0') {
					printUsage(stderr, argv[0], EXIT_FAILURE);
				}
				if (portNumber < 0 || portNumber > 65535) { // checks the portrange
					printUsage(stderr, argv[0], EXIT_FAILURE);
				}
				break;
			case 'h':
				printUsage(stdout, argv[0], EXIT_SUCCESS);
				break;
			case '?': //any input differing from 'p' and 'h'
			default:
				printUsage(stderr, argv[0], EXIT_FAILURE);
				break;
		}
	}

	socketfd = createSocket(port);

	execServer(socketfd);

	return 0;
}

static int createSocket(char *port) {
    struct addrinfo hints;
    struct addrinfo *addrInfo, *p;
    struct sigaction signalAction;
	int yes=1;
	int socketfd = -1;
	int rv;

    memset(&hints, 0, sizeof hints); //Initializing
	hints.ai_family = AF_INET; //Socket communicates with ipv4 Addresse
	hints.ai_socktype = SOCK_STREAM; //provides TCP
	hints.ai_flags = AI_PASSIVE;//Resolves the local Address and Port

	//returns one or more addrinfo structures, each of which contains an Internet address
	//that can be specified in a call to bind(2) or connect(2).
	if ((rv = getaddrinfo(NULL, port, &hints, &addrInfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
	}
	// loop through all the results and connect to the first we can
	for(p = addrInfo; p != NULL; p = p->ai_next) {
        //create socket
		if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { //domain, type, protocol
            perror("server: socket");
            continue;
        }
		//set socket options, SOL_SOCKET = set options at the socket level -> SO_REUSEADDR = allows other sockets to bind
		if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			close(socketfd);
			freeaddrinfo(addrInfo);
			perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        //bind to the created socket
		if (bind(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketfd);
			perror("server: bind");
            continue;
        }
        break;
    }

	if (p == NULL)  {
       fprintf(stderr, "server: failed to bind\n");
       freeaddrinfo(addrInfo);
	   exit(EXIT_FAILURE);
    }
	freeaddrinfo(addrInfo);

	/* listen */
	if (listen(socketfd, BACKLOG) == -1) {
		perror("Error listening.");
		close(socketfd);
		exit(EXIT_FAILURE);
	}

	/* set SignalAction for Child  - prevents Zombie-Processes*/
	signalAction.sa_handler = sigchld_handler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &signalAction, NULL) == -1) { //SIGCHILD provides information about the child
		perror("Error performing sigaction.");
		close(socketfd);
		exit(EXIT_FAILURE);
	}

    return socketfd;
}

static void execServer(int socketfd) {
	struct sockaddr_storage clientSockAddr;
	socklen_t clientSockSize;
	int newSocketfd = -1;

	while(1) { //runs the accept()and fork() SysCall in a loop
		clientSockSize = sizeof(clientSockAddr);

		newSocketfd = accept(socketfd, (struct sockaddr *) &clientSockAddr, &clientSockSize); //accept the connection from the client
		if (newSocketfd == -1) {
			perror("Error creating new socket.");
			close(socketfd);
			exit(EXIT_FAILURE);
		}

		/* FORKING */
		switch(fork())
		{
			case -1: //Failure
				close(newSocketfd);
				perror("fork failed");
				break;
			case 0: //Child-Process
				close(socketfd); //The combination of close and dup2 can redirect to STDIN or STDOUT
				if(dup2(newSocketfd, STDIN_FILENO) == -1 || dup2(newSocketfd, STDOUT_FILENO) == -1)
				{
					perror("failed to dup stdin");
					close(newSocketfd);
					exit(EXIT_FAILURE);
				}
				close(newSocketfd);
				if(execl(business_logic_path, business_logic, (char *) NULL) == -1) { //Business Logic will be executed
					perror("execl failed!");
					exit(EXIT_FAILURE);
				}
				exit(EXIT_FAILURE);
				break;
			default: //Parent-Process
				close(newSocketfd);
		}

	}
}

static void sigchld_handler(int s){
	int saved_errno = errno;
	s = s;
    while(waitpid(-1, NULL, WNOHANG) > 0); //wait for the termination of all child-processes
	errno = saved_errno;
}

static void printUsage(FILE *stream, const char *programName, int exitCode) {
	fprintf(stream, "usage: %s option\n", programName);
	fprintf(stream, "options:\n");
	fprintf(stream, "\t-p, --port <port>\n");
	fprintf(stream, "\t-h, --help\n");

	exit(exitCode);
}

/*
 * =================================================================== eof ==
 */
