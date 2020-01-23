#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
 
#define MAXRCVLEN 4096

typedef struct file_node {
	int sock;
	char filename[21];
	int file_size;
	int chunk_size;
	struct file_node *next;
} FileNode;

typedef struct {
	FileNode *head;
	FileNode *tail;
	int length;
} DownloadQueue;

int status = 1;
DownloadQueue *q;
pthread_cond_t r_cond;
pthread_mutex_t q_mutex;
pthread_mutex_t w_mutex;
pthread_mutex_t ui_mutex;

void * ui_thread(void *arg) {
	FileNode *node;

	printf("\nWelcome to the server UI!\nd = display active transfers\n");
	printf("s = soft shutdown (finish transfers, then exit)\nh = hard shutdown (abort   active    transfers)\n\n");

	while (status == 1) {
		pthread_mutex_lock(&ui_mutex);

		char c = fgetc(stdin);

		if (c == 's') {
			status = 0;
		}else if (c == 'h') {
			status = -1;
		}else if (c == 'd') {
			if (q->length != 0) {
				pthread_mutex_lock(&q_mutex);
				pthread_cond_wait(&r_cond, &q_mutex);
				node = q->head;
				while (node != NULL) {
					printf("File Name: %s\nFile Size: %d\nChunk Size: %d\n", node->filename, node->file_size, node->chunk_size);
					node = node->next;
				}
			}else{ printf("Nothing in the download queue!\n"); }
			pthread_cond_signal(&r_cond);
			pthread_mutex_unlock(&q_mutex);
		}else{
			if (!(c == '\n')) {
				printf("Invalid input!\n");
			}
		}
		
		pthread_mutex_unlock(&ui_mutex);
	}

	return NULL;
}

/**
 * https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c-cross-platform
 * Checking if file exists in C
 */
void * dl_work_thread(void *arg) {
	char buffer[MAXRCVLEN + 1];
	char _infile_temp[256];
	int i, current;
	FILE * _infile;
	FileNode *node;
	FileNode *delete;

	pthread_mutex_init(&w_mutex, NULL);
	pthread_mutex_lock(&w_mutex);

	while (1) {
		pthread_cond_wait(&r_cond, &w_mutex);

		if (q->length == 0) { break; }
		node = q->head;

		while (node && (q->length > 0)) {
			pthread_mutex_lock(&q_mutex);

			current = node->sock;

			if (access(node->filename, F_OK) != -1) {
				strcpy(_infile_temp, node->filename);
				while (access(_infile_temp, F_OK) != -1) {
					i++;
					sprintf(_infile_temp, "%s_(%d)", node->filename, i);
				}
				_infile = fopen(_infile_temp, "w+");
			}else {
				_infile = fopen(node->filename, "w+");
			}

			delete = node;
			node = node->next;

			if (q->length == 1) {
				q->head = NULL;
				q->tail = NULL;
			}else{ q->head = node; }

			free(delete);
			pthread_mutex_unlock(&q_mutex);

			while(recv(current, buffer, MAXRCVLEN + 1, 0) > 0) {
				buffer[strlen(buffer)] = '\0';
				fprintf(_infile, "%s", buffer);
				send(current, buffer, MAXRCVLEN + 1, 0);
			}

			q->length--;
			fclose(_infile);
			close(current);
		}
	}
	pthread_cond_signal(&r_cond);
	pthread_mutex_unlock(&w_mutex);

	return NULL;
}

void * dl_conn_thread(void *arg) {
	char buffer[MAXRCVLEN + 1]; // +1 so we can add null terminator
	char header[MAXRCVLEN + 1]; // +1 so we can add null terminator
	int len;
	FileNode *node = (FileNode*) arg;

	// receive data from the client
	if ( (len = recv(node->sock, buffer, MAXRCVLEN, 0)) < 0) {
		perror("\nrecv() failed!\n");
		return NULL;
	}
	buffer[len] = '\0';
	
	// check if header or data
	if (buffer[0] == '@') {
		strcpy(header, buffer);
		char *ptr = strtok(header, "\n");
		ptr = strtok(NULL, "\n");
		if((int) strlen(ptr) <= 0) {
			perror("Error in filename!");
			return NULL;
		}
		strcpy(node->filename, ptr);
		node->filename[strlen(node->filename)] = '\0';
		ptr = strtok(NULL, "\n");
		if (atoi(ptr) == 0) {
			printf("Error in file size!");
			return NULL;
		}
		node->file_size = atoi(ptr);
		ptr = strtok(NULL, "\n");
		node->chunk_size = atoi(ptr);
	}

	send(node->sock, buffer, strlen(buffer), 0);

	pthread_mutex_lock(&q_mutex);

	node->next = NULL;
	if (q->length == 0) {
		q->head = node;
		q->tail = node;
		q->length++;
	}else{
		q->tail->next = node;
		q->tail = node;
		q->length++;
	}
	pthread_cond_signal(&r_cond);
	pthread_mutex_unlock(&q_mutex);

	return NULL;
}

int main(int argc, char *argv[])
{
	int serv_PORT;
	if (argc == 2) {
		char * endptr;
		int value = (int) strtol(argv[1], &endptr, 10);
		if (argv[1] == endptr || *endptr != '\0' || value < 1) {
			printf("Invalid PORT\n");
			return EXIT_FAILURE;
		}
		serv_PORT = value;
	}else{
		printf("\nInvalid arguments ... Please run as:\n");
		printf("    $./server port-number (12016)\n\n");
		return EXIT_FAILURE;
	}

	struct timeval tv;

	struct sockaddr_in dest; // socket info about the machine connecting to us
	struct sockaddr_in serv; // socket info about our server
	int mysocket;            // socket used to listen for incoming connections
	socklen_t socksize = sizeof(struct sockaddr_in);

	memset(&serv, 0, sizeof(serv));           // zero the struct before filling the fields
	
	serv.sin_family = AF_INET;                // use the IPv4 address family
	serv.sin_addr.s_addr = htonl(INADDR_ANY); // set our address to any interface 
	serv.sin_port = htons(serv_PORT);         // set the server port number 

	/* Create a socket.
   	   The arguments indicate that this is an IPv4, TCP socket
	*/
	mysocket = socket(AF_INET, SOCK_STREAM, 0);

	if (mysocket == -1) {
		printf("Unable opening socket!\n");
		return EXIT_FAILURE;
	}
  
	// bind serv information to mysocket
	// unlike all other function calls in this example, this call to bind()
	// does some basic error handling
	if (bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) != 0){
		printf("Unable to open TCP socket on localhost:%d\n", serv_PORT);
		printf("%s\n", strerror(errno));
		close(mysocket);
		return EXIT_FAILURE;
	}

	FileNode *node;

	pthread_t total[32];
	pthread_t ui;
	pthread_t work;
	pthread_cond_init(&r_cond, NULL);
	pthread_mutex_init(&ui_mutex, NULL);
	pthread_mutex_init(&q_mutex, NULL);
	pthread_create(&ui, NULL, ui_thread, NULL);
	pthread_create(&work, NULL, dl_work_thread, NULL);
	
	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(mysocket, &fd);

	// start listening, allowing a queue of up to 5 pending connection
	listen(mysocket, 5);

	q = (DownloadQueue*) malloc(sizeof(DownloadQueue));
	q->head = q->tail = NULL;
	q->length = 0;

	int consocket;

	while (status == 1) {
		FD_ZERO(&fd);
		FD_SET(mysocket, &fd);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		consocket = select(mysocket + 1, &fd, NULL, NULL, &tv);
		if (consocket == -1) {
			fprintf(stderr, "Something went wrong! Terminating program...");
			status = -1;
		}else if (consocket != 0) {
			consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
			node = (FileNode*) malloc(sizeof(FileNode));
			node->sock = consocket;
			pthread_create(&total[consocket], NULL, dl_conn_thread, (void*) node);
		}
	}

	if (status == 0) {
		getchar();
		pthread_mutex_lock(&q_mutex);

		char c[64];

		printf("\nAbort all transfers? (y/n)\n");
		fgets(c, 64, stdin);

		if (q->length == 0) { 
			pthread_join(ui, NULL);
			close(mysocket);
			return EXIT_SUCCESS;
		}

		if (strcmp(c, "n\n") == 0) {
			pthread_mutex_unlock(&q_mutex);
			pthread_join(work, NULL);
		}else{ pthread_mutex_unlock(&q_mutex); }
	}
	pthread_join(ui, NULL);

	close(mysocket);
	return EXIT_SUCCESS;
}