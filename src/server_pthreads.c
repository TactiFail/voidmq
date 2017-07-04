/*
** server_phtreads.c -- a stream socket server demo that uses pthreads as opposed to fork
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "3490"  // the port users will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10     // how many pending connections queue will hold
#define MAXTHREADS 10 // the maximum amount of threads to create for incoming connections

// Struct passed to new threads
struct t_args {
	int tid;
	int *comm_thread_scoreboard;
	int *thread_fd;
};

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// return the next available index of a passed scoreboard
// return -1 if no threads are available
int get_next_available_thread_from_scoreboard(int scoreboard[MAXTHREADS])
{
    int i;

    for (i = 0; i < MAXTHREADS; i++) {
        if (scoreboard[i] == 0) {
            return i;
        }
    }

    return -1;
}


// the function called by thread create
// * data will contain a file descripter from accept() from main listener loop
void * comm(void * data)
{
    int numbytes;
    char buf[MAXDATASIZE];
    int thread_fd = * (int *) data;

    if ((numbytes = recv(thread_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        return NULL;
    }

    buf[numbytes] = '\0';
    printf("server: received '%s'\n",buf);

    if (send(thread_fd, buf, numbytes, 0) == -1)
        perror("send");

    close(thread_fd);
    return NULL;
}

void * comm2(void *args)
{
	struct t_args *myargs;
	myargs = (struct t_args *) args;
	int myid = myargs->tid;
	int *comm_thread_scoreboard = myargs->comm_thread_scoreboard;

	// Debugging
	//printf("My ID is %d\n", myid);
	//printf("comm_thread_scoreboard resides at %p\n", (void *)comm_thread_scoreboard);
	//printf("My ID state is %d\n", (int *)comm_thread_scoreboard[myid]);

    int numbytes;
    char buf[MAXDATASIZE];
    //int thread_fd = * (int *) data;
    int thread_fd = * (int *)myargs->thread_fd;

    if ((numbytes = recv(thread_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
		comm_thread_scoreboard[myid] = 0;
        return NULL;
    }

    buf[numbytes] = '\0';
    printf("server: received '%s'\n",buf);

    if (send(thread_fd, buf, numbytes, 0) == -1)
        perror("send");

    close(thread_fd);
	comm_thread_scoreboard[myid] = 0;
    return NULL;
}

int main(void)
{
    int sockfd, new_fd;  // listen on sockfd,  new connection on new_fd (will be passed to thread create function)
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    pthread_t comm_thread[MAXTHREADS]; // the threads to communicate with

    // we use an array of values to determine if a thread is in use
    // value of 0 means not in use, any other value means it is in use
    // this would be better suited as a pointer and allocated at runtime then initialized
    // but we know we only allow 10 in this example
    int comm_thread_scoreboard[MAXTHREADS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int next_available_thread = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        next_available_thread = get_next_available_thread_from_scoreboard(comm_thread_scoreboard);
        printf("server: got next_available_thread: %d\n", next_available_thread);

        if (next_available_thread == -1) {

            // we're out of available threads. close the new_fd, sleep, and continue
            printf("server: out of available threads. waiting then trying again\n");
            close(new_fd);
            sleep(1);
            continue;
        }

        // update that this thread is used up!
        comm_thread_scoreboard[next_available_thread] = 1;

		// Pack struct with args for new thread
		// I have no idea if this is thread-safe
		struct t_args targs;
		targs.tid = next_available_thread;
		targs.comm_thread_scoreboard = &comm_thread_scoreboard;
		targs.thread_fd = &new_fd;
        //pthread_create(&comm_thread[next_available_thread], NULL, comm, (void *) &new_fd);
        pthread_create(&comm_thread[next_available_thread], NULL, comm2, (void *) &targs);
    }

    return 0;
}
