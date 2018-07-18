//NAME: Kevin Zhang
//EMAIL: kevin.zhang.13499@gmail.com
//ID: 104939334

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>
void restoreTerminal();
void resetTerminal();
void closePipes();
struct termios terminalSave;

void restoreTerminal(){
  tcsetattr(0, TCSANOW, &terminalSave);
}

void resetTerminal(){
  tcgetattr(0, &terminalSave);
  atexit(restoreTerminal);
  struct termios term;
  tcgetattr(0, &term); //get another copy
                                                                                                          
  term.c_iflag = ISTRIP;/* only lower 7 bits*/
  term.c_oflag = 0;/* no processing*/
  term.c_lflag = 0;/* no processing*/
  //reset terminal                                                                                                          
  if (tcsetattr(0, TCSANOW, &term) < 0){
    fprintf(stderr, "ERROR: Cannot initialize terminal.\n");
    exit(1);
  }
}
pid_t pid;
void signal_handler(int sig){
  if(sig == SIGINT){
	kill(pid, SIGINT);
	int status;
    waitpid(pid, &status, 0);
    int retStatus = WEXITSTATUS(status);
    int retState = WTERMSIG(status);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);
    exit(0);
  }
  if(sig == SIGPIPE){
	int status;
    int retStatus = WEXITSTATUS(status);
    int retState = WTERMSIG(status);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);
	exit(0);
  }
  if(sig == SIGABRT){
	int status;
	int retStatus = WEXITSTATUS(status);
    int retState = WTERMSIG(status);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);
  }
}

char n = '\n';
void setup_server();

int compression_flag;
int portNum = 0;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
int writeDesc = 1;
int readDesc = 0;
int numPolls = 2;

int pipeFT[2];
int pipeTT[2];

int sockfd, client_sockfd;

z_stream server_strm;
z_stream keyboard_strm;

void process(){

  close(pipeFT[readDesc]);
  close(pipeTT[writeDesc]);
  struct pollfd polls[2];
  polls[0].fd = client_sockfd;
  polls[1].fd = pipeTT[readDesc];
  polls[0].events = POLLIN | POLLHUP | POLLERR;
  polls[1].events = POLLIN | POLLHUP | POLLERR;

  unsigned char  buf[256];
  unsigned char compressed_buffer[256];
  while (1) {
	int status;
	if (waitpid(pid, &status, WNOHANG) == -1) {
	  fprintf(stderr, "Error: Child Process Ended\n\r");
	  int retStatus = WEXITSTATUS(status);
	  int retState = WTERMSIG(status);
	  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
      exit(1);
	}
	
	int ret = poll(polls, numPolls, 0); //2 polls and timeout = 0 as instructed
	if (ret < 0) {
	  fprintf(stderr, "Error: Polling failed unexpectedly.\n");
	  exit(1);
	}

	
	if (polls[0].revents & POLLIN) {//if 1st poll is in POLLIN mode    
      //read from keyboard
	  int bytes = read(client_sockfd, buf, 256);
	  if (bytes < 0){
		fprintf(stderr, "Error: failed to read from keyboard\n");
		exit(1);
	  } 
	  
	  if (compression_flag){
		keyboard_strm.zalloc = Z_NULL;
		keyboard_strm.zfree = Z_NULL;
		keyboard_strm.opaque = Z_NULL;

		if (inflateInit(&keyboard_strm) != Z_OK){
		  fprintf(stderr, "Error: Cannot initialize decompression protocol. \n\r");
		  exit(1);
		}		
		keyboard_strm.avail_in = bytes; 
		keyboard_strm.next_in = buf;
		keyboard_strm.avail_out = 256; 
		keyboard_strm.next_out = compressed_buffer;


		do{
		  int ret = inflate(&keyboard_strm, Z_SYNC_FLUSH);
		  if (ret != Z_OK){
			fprintf(stderr, "Zlib Error # %d", ret);
			exit(1);
		  }
		} while (keyboard_strm.avail_in > 0);
		
		unsigned int size = 256 - keyboard_strm.avail_out;
		unsigned int i;
		for (i = 0;i<size;i++){
		  switch (compressed_buffer[i]){
		  case 0x04:
		  case 0x03:
			closePipes();
			break;
		  case '\r':
		  case '\n':
			write(pipeFT[writeDesc], &n, 1);
			continue;
		  default:
			write(pipeFT[writeDesc], &compressed_buffer[i], 1);
		  }
		}
		inflateEnd(&keyboard_strm);
	  }
	  else{
		int i;
		for (i= 0;i<bytes;i++){
		  switch (buf[i]){
		  case 0x04: //ctrl d character
			//close the writing pipe to the shell so that no other input can be transferred         
			close(pipeFT[writeDesc]);
			//int status;
			//pid_t waitpid(pid_t pid, int *status, int options);                      
            /*print                                                                    
              SHELL EXIT SIGNAL=# STATUS=#                                             
              where the first # is the low order 7-bits (0x007f) of the shell's exit status and
			  the second # is the next higher order byte (0xff00) of the shell's exit  
              status (both decimal integers).                                          
			*/

			waitpid(pid, &status, 0);
            int retStatus = WEXITSTATUS(status);
            int retState = WTERMSIG(status);
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);
			exit(0);
		  case 0x03: //ctrl c character
			kill(pid, SIGINT);
			/*              close(pipeFT[writeDesc]);                                  
            waitpid(pid, &status, 0);                                                  
            retStatus = WEXITSTATUS(status);                                           
            retState = WTERMSIG(status);                                               
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);  
            */
			break;
		  case '\r':
		  case '\n':
			write(pipeFT[writeDesc], &n, 1);
			continue;
		  default:
			write(pipeFT[writeDesc], &buf, 1);
		  }
		}
	  }
	}

	if ((polls[1].revents & POLLIN)) {
	  int bytes = read(pipeTT[0],buf, 256);
 
	  if (bytes < 0){
		fprintf(stderr, "ERROR: Shell process polled but read failed.\n");
		exit(1);
	  } 
	  if (compression_flag){
		server_strm.zalloc = Z_NULL;
		server_strm.zfree = Z_NULL;
		server_strm.opaque = Z_NULL;

		if (deflateInit(&server_strm, Z_DEFAULT_COMPRESSION) != Z_OK){
          fprintf(stderr, "Error: Cannot initialize compression paramters.\n\r");
		  exit(1);
		}
		server_strm.avail_in = bytes;
		server_strm.next_in = buf;
		server_strm.avail_out = 256;
		server_strm.next_out = compressed_buffer;

		do{
          int ret = deflate(&server_strm, Z_SYNC_FLUSH);
          if (ret != Z_OK){
            fprintf(stderr, "Zlib Error # %d", ret);
            exit(1);
          }
		} while (server_strm.avail_in > 0);
		/*int i;
		for(i = 0; i< (256 - server_strm.avail_out); i++){
		  write(client_sockfd, &compressed_buffer[i], 1);
		  }*/
		write(client_sockfd, &compressed_buffer, 256 - server_strm.avail_out);
		
		deflateEnd(&server_strm);
	  }
	  else{
		int i;
		for (i = 0; i < bytes; i++){
		  write(client_sockfd, &buf[i], 1);
		}
	  }
	  //read from shell process
	  if ((polls[1].revents & (POLLHUP | POLLERR))) { //if 2nd poll is in POLLIN mode
        int status;
        waitpid(pid, &status, 0);
        int retStatus = WEXITSTATUS(status);
        int retState = WTERMSIG(status);
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
        break;
      }
	  if ((polls[0].revents & (POLLHUP | POLLERR))) {
        int status;
        waitpid(pid, &status, 0);
        int retStatus = WEXITSTATUS(status);
        int retState = WTERMSIG(status);
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
        break;
		}
	}
  }
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);
  static struct option long_options[] = {
    {"port", required_argument, 0, 'p'},
    {"compress", no_argument, 0, 'c'},
  };
  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"s", long_options , &option_index);
  while(c!=-1){
    switch(c){
    case 'p':
      portNum = atoi(optarg);
      break;
    case 'c':
      compression_flag = 1;
      break;
    default:
      fprintf(stderr, "ERROR: Unrecognized argument.\n Usage: ./lab1a [--shell]\n");
      exit(1);
      break;
    }
    c = getopt_long(argc,argv,"s", long_options , &option_index);
  }

  if(portNum == -1){
    fprintf(stderr, "Required argument: --port not found. \n\r");
    exit(1);
  }

  
  setup_server();
  atexit(closePipes);
  if (pipe(pipeFT) == -1 || pipe(pipeTT) == -1) {
    fprintf(stderr, "ERROR: Unable to create pipe. \n");
    exit(1);
  }
  pid = fork(); //fork new process
  if (pid < 0){
	fprintf(stderr, "Error: child process was not created correctly.\n\r");
	exit(1);
  }
  else if (pid == 0){ //child process 
	close(pipeFT[writeDesc]);
    close(pipeTT[readDesc]);
    dup2(pipeFT[readDesc], 0);
    dup2(pipeTT[writeDesc], 1);
    close(pipeFT[readDesc]);
    close(pipeTT[writeDesc]);
    char *name[2] = { "Bash Terminal", NULL };
    if(execvp("/bin/bash", name) == -1)
      {
        fprintf(stderr, "Cannot start new bash terminal session...Exiting.\n");
        exit(1);
      }
  }
  else{ //parent process 
	process();
  }
  exit(0);
}




void setup_server(){
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      fprintf(stderr, "ERROR creating server socket\n\r");
      exit(1);
    }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr .sin_port = htons(portNum);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr, "Error, cannot bind server socket. \n");
    exit(1);
  }
  listen(sockfd, 5);
  clilen = sizeof(cli_addr);
  client_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if(client_sockfd < 0){
    fprintf(stderr, "Client socket refused to connect. \n\r");
    close(sockfd);
    exit(1);
  }
}


void closePipes() {
  close(pipeFT[1]);
  close(pipeTT[0]);
  close(sockfd);
  close(client_sockfd);
  
  int status;
  waitpid(pid, &status, 0);
  int retStatus = WEXITSTATUS(status);
  int retState = WTERMSIG(status);
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
  exit(0);
}
