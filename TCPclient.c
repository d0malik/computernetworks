#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "networkHelpers.h"

int main(int argc, char *argv[])
{
	char serv_IP[100];
	char _infileName[100];
	int serv_PORT, buffer_size;

	if (argc == 4) {
		char *ptr = strtok(argv[1], ":");
		lookup_host(ptr, serv_IP);
		ptr = strtok(NULL, ":");
		char *endptr;
		int value = (int) strtol(ptr, &endptr, 10);
		if (endptr == ptr || *endptr != '\0') {
			printf("Invalid PORT\n");
			return EXIT_FAILURE;
		}
		serv_PORT = value;
		if (strlen(argv[2]) > 21) {
			printf("FILE NAME too long\n");
			return EXIT_FAILURE;
		}
		strcpy(_infileName, argv[2]);
		buffer_size = atoi(argv[3]);
	}else if (argc == 3) {
		char *ptr = strtok(argv[1], ":");
		lookup_host(ptr, serv_IP);
		ptr = strtok(NULL, ":");
		char *endptr;
		int value = (int) strtol(ptr, &endptr, 10);
		if (endptr == ptr || *endptr != '\0') {
			printf("Invalid PORT\n");
			return EXIT_FAILURE;
		}
		serv_PORT = value;
		if (strlen(argv[2]) > 21) {
			printf("FILE NAME too long\n");
			return EXIT_FAILURE;
		}
		strcpy(_infileName, argv[2]);
		buffer_size = 4096;
	}else{
		printf("\nInvalid arguments ... Please run as:\n");
		printf("    $./client server-IP-address:12016 [file-path] [chunk-size (OPTIONAL)]\n\n");
		return EXIT_FAILURE;
	}

	// read in FILE
	FILE *_infile;
	_infile = fopen(_infileName, "r");
	if (_infile == NULL) {
		printf("Error opening input file.\n");
        return EXIT_FAILURE;
	}

	char buffer[buffer_size + 1]; // +1 so we can add null terminator
	char serv_msg[buffer_size + 1];

	int mysocket, len;
	struct sockaddr_in dest;    // socket info about the machine connecting to us
 
	/* Create a socket.
	   The arguments indicate that this is an IPv4, TCP socket
	*/
	mysocket = socket(AF_INET, SOCK_STREAM, 0);
  
	memset(&dest, 0, sizeof(dest));            // zero the struct
	
	// initialize the destination socket information
	dest.sin_family = AF_INET;				   // Use the IPv4 address family
	dest.sin_addr.s_addr = inet_addr(serv_IP); // Set destination IP number - localhost, 127.0.0.1
	dest.sin_port = htons(serv_PORT);          // Set destination port number
 	
	// connect to the server
	connect(mysocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
	
	/*
	 * File name (possibly length)
	 * Overall file size
	 * Size of chunks that client will use
	 */
	char header_info[256];
	int header_length = 0;
	fseek(_infile, 0, SEEK_END);
	int header_size = ftell(_infile);
	fseek(_infile, 0, SEEK_SET);
	while(fgets(buffer, 4096, _infile) != NULL) { header_length++; }
	rewind(_infile); // Rewind file stream after getting data for header
	buffer[0] = '\0';

	sprintf(header_info, "@\n%s\n%d\n%d\n", _infileName, header_size, buffer_size);

	send(mysocket, header_info, 256, 0);
	len = recv(mysocket, serv_msg, 256, 0);
	serv_msg[len] = '\0';

	printf("Successfully sent!\n");

	/*
	 * Start sending of data
	 */
	memset(buffer, 0, buffer_size + 1);
	while (fgets(buffer, buffer_size + 1, _infile)) {
		send(mysocket, buffer, buffer_size + 1, 0);
		usleep(500000);
		recv(mysocket, serv_msg, 256, 0);
	}

	fclose(_infile);
	close(mysocket);
	return EXIT_SUCCESS;
}
