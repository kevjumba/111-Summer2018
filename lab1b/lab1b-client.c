/*
NAME: Kevin Zhang                                                   
EMAIL: kevin.zhang.13499@gmail.com
ID: 104939334                                                                  
*/
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "getopt.h"
#include "unistd.h"
#include "stdlib.h"
#include "termios.h"
#include "poll.h"
#include "signal.h"
#include "sys/wait.h" //WEXITSTATUS etc.                                        
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "ulimit.h"
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "zlib.h"

void resetTerminal();
void restoreTerminal();
void setup_client();
int compression_flag;
int numPolls = 2;
int portNum;
int log_flag = 0;
char* logFile = NULL;
char newline[2] = { '\n', '\r' };
struct termios terminalSave;
struct sockaddr_in serv_addr;
struct hostent *server;
int sockfd;
char c = '\3';

z_stream keyboard_strm;
z_stream server_strm;
void restoreInput();

int log_fd;

void process() {
  struct pollfd polls[2];

  polls[0].fd = STDIN_FILENO;
  polls[1].fd = sockfd;
  polls[0].events = POLLIN | POLLHUP | POLLERR;
  polls[1].events = POLLIN | POLLHUP | POLLERR;

  unsigned char buf[256];
  unsigned char compressed_buffer[256];
  while (1) {
	int ret = poll(polls, numPolls, 0);
	if (ret < 0) {
	  fprintf(stderr, "ERROR: Cannot poll\n\r");
	  exit(1);
	}
	if ((polls[0].revents & POLLIN)) {  //if 1st poll is in POLLIN mode
      //read from keyboard
	  int bytes = read(STDIN_FILENO, buf, 256);
	  if (bytes < 0){
		fprintf(stderr, "Error: failed to read from keyboard\n");
		exit(1);
	  } 
	  
	  if (compression_flag  && bytes > 0){
		keyboard_strm.zalloc = Z_NULL;
		keyboard_strm.zfree = Z_NULL;
		keyboard_strm.opaque = Z_NULL;

		if (deflateInit(&keyboard_strm, Z_DEFAULT_COMPRESSION) != Z_OK){
		  fprintf(stderr, "Error: deflation unsuccessful\n\r");
		  exit(1);
		}
		keyboard_strm.avail_in = bytes; 
		keyboard_strm.next_in = buf;
		keyboard_strm.avail_out = 256;
		keyboard_strm.next_out = compressed_buffer;
		
		do{
		  int ret = deflate(&keyboard_strm, Z_SYNC_FLUSH);
		  if (ret != Z_OK){
			fprintf(stderr, "Error: Zlib error #%d", ret);
			exit(1);
		  }
		} while (keyboard_strm.avail_in > 0);
		
		write(sockfd, &compressed_buffer, 256 - keyboard_strm.avail_out);
		if(log_flag && bytes>0){
		  dprintf(log_fd, "SENT %d bytes: ", 256 - keyboard_strm.avail_out);
		  write(log_fd, &compressed_buffer, 256 - keyboard_strm.avail_out);
		  write(log_fd, "\n", 1);
		}	
		deflateEnd(&keyboard_strm);
	  }
	  int i;
	  for (i = 0;i<bytes;i++){
		if(buf[i] == '\r' || buf[i] =='\n'){
		  write(1, &newline, 2);
		  if (!compression_flag){ 
			write(sockfd, &buf[i], 1); 
  		  }
		}
		else{
		  write(1, &buf[i], 1);
		  if (!compression_flag){ 
			write(sockfd, &buf[i], 1); 
		  }
		}
	  }
      if(log_flag && bytes > 0 && !compression_flag){
        dprintf(log_fd, "SENT %d bytes: ", bytes);
        write(log_fd, buf, bytes);
        write(log_fd, "\n", 1);
      }
	}

                                            
	//read from socket, can be either compressed or regular
	if ((polls[1].revents & POLLIN)) {//if 2nd poll is in POLLIN mode        
	  int bytes = read(sockfd, buf, 256);
      if (bytes < 0){
        fprintf(stderr, "ERROR: Shell process polled but read failed.\n");
        close(sockfd);
        exit(1);
      }
	  if (bytes == 0){
		exit(0);
	  }
      if(log_flag && bytes > 0){
        dprintf(log_fd, "RECEIVED %d bytes: ", bytes);
        write(log_fd, buf, bytes);
        write(log_fd, "\n", 1);
      }
	  
	  if (compression_flag && bytes > 0){
		server_strm.zalloc = Z_NULL;
		server_strm.zfree = Z_NULL;
		server_strm.opaque = Z_NULL;

		if (inflateInit(&server_strm) != Z_OK){
		  fprintf(stderr, "error on inflateInit");
		  exit(1);
		}
		
		server_strm.avail_in = bytes; 
		server_strm.next_in = buf;
		server_strm.avail_out = 256;
		server_strm.next_out = compressed_buffer;

		do{
		  int ret = inflate(&server_strm, Z_SYNC_FLUSH);

          if (ret != Z_OK){
            fprintf(stderr, "error inflating error number: %d", ret);
            //write(sockfd, "\3", 1);
            exit(1);
          }

		} while (server_strm.avail_in > 0);

		inflateEnd(&server_strm);

		int size = 256 - server_strm.avail_out;
		int i;
		for (i = 0; i < size; i++){
		  switch (compressed_buffer[i]){
		  case '\n':
		  case '\r':
			write(1, &newline, 2);
			continue;
		  default:
			write(1, &compressed_buffer[i], 1);
		  }
		}
	  }
	  else{
		int i;
		for (i = 0; i < bytes; i++){
		  switch (buf[i]){
		  case '\n':
		  case '\r':
			write(STDOUT_FILENO, &newline, 2);
			continue;
		  default:
			write(STDOUT_FILENO, &buf[i], 1);
		  }
		}
	  }
	}

    if(polls[1].revents & (POLLERR | POLLHUP)){
      fprintf(stderr, "Server Connection Ended.\n\r");
	  exit(0);
    }

    if(polls[0].revents & (POLLERR | POLLHUP)){
      fprintf(stderr, "Server Connection Ended.\n\r");
      exit(0);
    }
  }
}

int main(int argc, char **argv) {
  static struct option long_options[] = {
    {"shell", no_argument, 0, 's'},
    {"port", required_argument, 0, 'p'},
    {"log", required_argument, 0, 'l'},
    {"compression", no_argument, 0, 'c'},
  };

  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"s", long_options , &option_index);
  while(c!=-1){
    switch(c){
	case 'p':
	  portNum = atoi(optarg);
	  break;
	case 'l':
	  logFile = optarg;
	  log_flag = 1;
	  break;
	case 'c':
	  compression_flag = 1;
	  break;
	default:
	  fprintf(stderr, "ERROR: Invalid argument \n\r Usage: ./lab1b-client [--shell] [--port=portnum]\n\r");
	  exit(1);
	  break;
    }
    c = getopt_long(argc,argv,"s", long_options , &option_index);
  }

  if(portNum == -1){
    fprintf(stderr, "Required argument: --port not found. \n\r");
    exit(1);
  }
  setup_client();

  if (logFile!=NULL){
    log_fd = creat(logFile, 0666);;
    if(log_fd < 0){
      fprintf(stderr, "ERROR: Unable to open log file.\n\r");
      write(sockfd, &c, 1);
      exit(1);
    }
  }

  struct rlimit rlim = {10000, 10000};
  if(setrlimit(RLIMIT_FSIZE, &rlim)){
    fprintf(stderr, "Cannot set rlimit.\n");
    exit(1);
  }

  resetTerminal(); //reset terminal must be behind log checking because terminal setting takes too long and fails the test script
  process();
  
  exit(0);
}


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
    fprintf(stderr, "ERROR: Cannot initialize terminal.\n\r");
    exit(1);
  }
}

void setup_client(){
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "ERROR opening socket\n\r");
    exit(1);
  }
  server= gethostbyname("localhost");
  if(server == NULL){
    fprintf(stderr, "ERROR: Cannot connect to server at hostname localhost\n\r");
    exit(1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(portNum);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    fprintf(stderr, "ERROR connecting to client socket exiting\n\r");
    exit(1);
  }
}
