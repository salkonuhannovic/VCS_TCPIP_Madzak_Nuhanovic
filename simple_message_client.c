/**
 * @file simple_message_client.c
 * Verteilte Systeme - TCP/IP Programmierübung
 *
 * tcpip client
 *
 * @author Claudia Madzak - ic16b028 - claudia.madzak@technikum-wien.at
 * @author Salko Nuhanovic - ic17b064 - salko.nuhanovic@technikum-wien.at
 * @date 2017/12/09
 * @version 0.7
 */

 /*
 * -------------------------------------------------------------- includes --
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <simple_message_client_commandline_handling.h>

 /*
 * --------------------------------------------------------------- defines --
 */

 /**
 * Buffersize for reading and writing
 */
#define MAXDATASIZE	1024

 /*
 * --------------------------------------------------------------- globals --
 */
static int verbose = 0;

/*
* ------------------------------------------------------------- functions --
*/
static void printUsage(FILE* stream, const char *programName, int exitcode);
static void print_verbose(char * text);
static void *get_in_addr(struct sockaddr *sa);
static int get_connection(const char *server, const char *port);
static int send_request(int sockfd, const char *user, const char *img, const char *message);
static int get_response(int sockfd);
static int handle_response(FILE *rfp);


/**
 * @brief Prints the possible parameters of the application. and exits the process.
 
 * @param stream	file stream for output
 * @param programName		name of the programm
 * @param exitcode	exitcode will return to the operating system
 *
 */
static void printUsage(FILE* stream, const char *programName, int exitcode) 
{
	fprintf(stream, "usage: %s options\n", programName);
	fprintf(stream, "options:\n");
	fprintf(stream, "-s, --server <server>   Full qualified domain name or IP address of the server.\n");
	fprintf(stream, "-p, --port <port>       Well-known port of the server [0..65535].\n");
	fprintf(stream, "-u, --user <name>       Name of the posting user.\n");
	fprintf(stream, "-i, --image <URL>       URL pointing to an image of the posting user.\n");
	fprintf(stream, "-m, --message <message> Message to be added to the bulletin board.\n");
	fprintf(stream, "-v, --verbose           Writes execution trace information to stdout.\n");
	fprintf(stream, "-h, --help				 Writes usage information to stdout.\n");
	exit (exitcode);
}

/**
* @brief Prints passed verbose messages of the programm.
*
* @param text	text to be printed
*
*/
static void print_verbose(char * text) 
{
	if(verbose)
	{
		if(fprintf(stdout, "%s%c", text, '\n') == -1)
		{
			perror("error while printig verbose message\n");
		}
	}
}

/**
 * @brief get_in_addr get sockaddr, IPv4 or IPv6
 *
 * @param sa	struct sockaddr
 *
 */
static void *get_in_addr(struct sockaddr *sa) 
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * @brief get_connection 	creates a socket and connects to the specified server
 *
 * @param server	server name or ip
 * @param port		server port
 *
 * @return socket id - if successfull, otherwise -1
 *
 */
static int get_connection(const char *server, const char *port)
{
	int sockfd = 0;
	struct addrinfo hints, *servinfo, *p; 
	int rv = 0; 
	char s[INET6_ADDRSTRLEN] = "";
	char msg[MAXDATASIZE] = "";

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG; 

	if ((rv = getaddrinfo(server, port, &hints, &servinfo)) != 0)  //Get Adress info of Server whih can be used for connect
	{
		fprintf(stderr, "simple_message_client: getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	/* loop through all the results and connect to the first we can*/
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) // create socket
		{
			perror("can't create socket");
			continue;
		}
		print_verbose("Socket created");

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) //cokect to server
		{
			close(sockfd);
			perror("can't connect to socket");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		freeaddrinfo(servinfo);  // Clean used Resources
		perror("can't connect to socket");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s); // Covert IP Adress to text form 

	sprintf(msg, "Connected to %s", s);
	print_verbose(msg);

	freeaddrinfo(servinfo);

	return sockfd;
}

/**
 * @brief send_request 	prepares the request for the server and sends it
 *
 * @param sockfd	socket
 * @param user		user string
 * @param img		img_url string
 * @param message	message string
 *
 * @return 0 - if successfull, otherwise -1
 *
 */
static int send_request(int sockfd, const char *user, const char *img, const char *message)
{
	FILE *wfp = NULL;
	int newsock = 0;

	if ((user == NULL) || (message == NULL)) 
	{
		perror("invalid user or message");
		return -1;
	}
	
	if ((newsock = dup(sockfd)) == -1) //dup dupliziert den Filedeskriptor
	{
		perror("can't dup socket for request");
		return -1;
	}
	print_verbose("socket duplicated");

	wfp = fdopen(newsock, "w"); //wandelt filedeskriptor in einen File Pointer um
	if (wfp == NULL)
	{
		perror("can't open write-stream");
		return -1;
	}
	print_verbose("socket opened as file for writing");

	if (img == NULL) 
	{
		if (fprintf(wfp, "user=%s\n%s\n", user, message) == -1)
		{
			if(fclose(wfp) == -1){ 
				perror("can't close write-stream");
				return -1;
				}
			perror("can't send to server");
			return -1;
		}
		print_verbose("send message to server");
	}
	else //mit Bild
	{
		if (fprintf(wfp, "user=%s\nimg=%s\n%s\n", user, img, message) == -1)
		{
			if(fclose(wfp) == -1){ 
				perror("can't close write-stream");
				return -1;
				}
			perror("can't send to server");
			return -1;
		}
		print_verbose("send message with img-url to server");
	}

	if (fclose(wfp) == -1)
	{
		perror("can't close write-stream");
		return -1;
	}
	print_verbose("close reading file socket");

	if (shutdown(sockfd, SHUT_WR) == -1) // shut down socket send and disables further send operations
	{
		perror("can't shutdown");
		return -1;
	}
	print_verbose("shutdown the connection");

	return 0;
}



/**
 * @brief get_response 	get the respone of the message
 *
 * @param sockfd	socket
 *
 * @return 0 - if successfull, otherwise -1
 *
 */
static int get_response(int sockfd)
{
	FILE *rfp;
	int newsock = 0;
	int status = 0;
	char buffer[MAXDATASIZE] = "";
	int end = 0;
	char msg[MAXDATASIZE] = "";

	
	if ((newsock = dup(sockfd)) == -1) //dup dupliziert Filedeskriptor, und schließt nicht stdin
	{
		perror("can't dup socket for reply");
		return -1;
	}
	print_verbose("socket duplicated");

	rfp = fdopen(newsock, "r"); //wandelt Filedeskriptor in Filepointer um
	if (rfp == NULL) 
	{
		perror("can't open read-stream");
		return -1;
	}
	print_verbose("open socket to get answer from server");

	/* read status */
	fgets(buffer, MAXDATASIZE, rfp);
	if (rfp == NULL)
	{
		perror("can't read from socket");
		if(fclose(rfp) == -1) { 
			perror("can't close rfp");
			return -1;
		}
		return -1;
	}
	if (sscanf(buffer, "status=%d", &status) <= 0)
	{
		perror("can't read status");
		return -1;
	}
	sprintf(msg, "read status=%d", status);
	print_verbose(msg);

	/* read filename, filelen and data until EOF */
	do
	{
		end = handle_response(rfp);
		if (end < 0)
		{
			if(fclose(rfp) == -1) { 
				perror("can't close rfp");
				return -1;
			}
			return -1;
		}
	}
	while (end != 1); 

	if (fclose(rfp) == -1)
	{
		perror("can't close read-stream");
		return -1;
	}

	print_verbose("close socket file");

	return 0;
}

/**
* @brief handle_response 	Gets File poiner reads ouf of it and creates the ok/error and HTML file
*
* @param rfp	file pointer to get resposne data
*
* @return 0 - if successfull, otherwise -1
*
*/
static int handle_response(FILE *rfp)
{
	FILE *response = NULL;
	char msg[MAXDATASIZE] = "";
	int i = 0;
	char filename[MAXDATASIZE] = "";
	int filelen = 0;
	char buffer[MAXDATASIZE];

	if (fgets(msg, MAXDATASIZE - 1, rfp) == NULL) {
		if (feof(rfp))
		{
			/* no more files */
			print_verbose("EOF received from server");
			return 1;
		}
		if (ferror(rfp))
		{
			perror("can't read from network");
			return -1;
		}
	}
	/* read filename*/
	if (sscanf(msg, "file=%s", filename) <= 0)
	{
		perror("can't read filename");
		return -1;
	}
	sprintf(msg, "read filename: %s from server", filename);
	print_verbose(msg);

	/* read filelength */
	fgets(msg, MAXDATASIZE - 1, rfp);
	if (ferror(rfp) || feof(rfp))
	{
		perror("can't read from socket");
		return -1;
	}
	if (sscanf(msg, "len=%d", &filelen) <= 0)
	{
		perror("can't read len");
		return -1;
	}
	sprintf(msg, "read filelen: %d from server", filelen);
	print_verbose(msg);

	/* open local file */
	response = fopen(filename, "w");
	if (response == NULL)
	{
		sprintf(msg, "can't open file %s", filename);
		perror(msg);
		return -1;
	}
	sprintf(msg, "open local file: %s", filename);
	print_verbose(msg);

	/*	read file from network and saves it locally */
	for (i = 0; i < filelen; i++)
	{
		fread(buffer, sizeof(buffer[0]), 1, rfp);
		if (ferror(rfp) || feof(rfp))
		{
			if (fclose(response) == -1) {
				perror("can't close response");
				return -1;
			}
			perror("can't read from socket");
			return -1;
		}
		fwrite(buffer, sizeof(buffer[0]), 1, response);

		if (ferror(response))
		{
			if (fclose(response) == -1) {
				perror("can't close response");
				return -1;
			}
			perror("can't write file");
			return -1;
		}

	}
	sprintf(msg, "read %d bytes from server", filelen);
	print_verbose(msg);
	sprintf(msg, "write %d bytes to %s", filelen, filename);
	print_verbose(msg);

	if (fclose(response) == -1)
	{
		sprintf(msg, "can't close file %s", filename);
		perror(msg);
		return -1;
	}
	sprintf(msg, "close file %s", filename);
	print_verbose(msg);

	return 0;
}

/**
 * @brief main function
 *
 * @param argc number of arguments
 * @param argv arguments itselves (including the name of program in argv[0])
 *
 * @return 0 - if successfull, otherwise != 0
 *
 */
int main (int argc, const char * argv[])
{
	int sockfd = 0;
	const char *server = NULL;
	const char *port = NULL;
	const char *user = NULL;
	const char *message = NULL;
	const char *img = NULL;
	
	smc_parsecommandline(argc, argv, printUsage, &server, &port, &user, &message, &img, &verbose); 

	print_verbose("parsed parameter");

	sockfd = get_connection(server, port);
	if (sockfd < 0)
	{
		return EXIT_FAILURE;
	}

	if (send_request(sockfd, user, img, message) < 0)
	{
		close(sockfd);
		return EXIT_FAILURE;
	}

	if (get_response(sockfd) < 0)
	{
		close(sockfd);
		return EXIT_FAILURE;
	}

	if (close(sockfd) < 0)
	{
		perror("can't close socket.\n");
	}
	print_verbose("connection closed");

	return EXIT_SUCCESS;
}

/*
 * --------------------------------------------------------------- EOF ---
 */
