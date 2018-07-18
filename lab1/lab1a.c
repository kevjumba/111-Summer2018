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

pid_t pid;
int readDesc = 0;
int writeDesc = 1;
//Pipes
int pipeFT[2];
int pipeTT[2];
int shell_flag;
char newline[2] = {'\r', '\n'};
const int maxReadSize = 256;

struct termios terminalSave;

void closePipes();

void restoreTerminal(){
  tcsetattr(0, TCSANOW, &terminalSave);
  closePipes();
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

void closePipes(){
  close(pipeTT[0]);
  close(pipeTT[1]);
  close(pipeFT[0]);
  close(pipeFT[1]);
}


void process(){
  /*
       int poll(struct pollfd *fds, nfds_t nfds, int timeout);
	   nfds is number of fds
	   timeout is blocking time

  */
  //create two polling objects for both sides
  struct pollfd polls[2];
  int numPolls = 2;
  close(pipeFT[readDesc]);
  close(pipeTT[writeDesc]);
  polls[readDesc].fd = STDIN_FILENO;
  polls[writeDesc].fd = pipeTT[readDesc];
  //POLLIN is a poll instruction
  //POLLERR is an asynchronous error
  //POLLHUP is a disconnected socket
  polls[readDesc].events = POLLIN | POLLERR | POLLHUP;
  polls[writeDesc].events = POLLIN | POLLERR | POLLHUP;
  
  char buf[256];
  int ret;
  while(1){
	ret = poll(polls, numPolls, 0);
	if(ret == -1){
	  fprintf(stderr, "Polling failure\n");
	  restoreTerminal();
	  exit(1);
	}
	if((polls[0].revents & POLLIN)) { //if 1st poll is in POLLIN mode
	  //read from keyboard
	  int bytes = read(STDIN_FILENO, buf, maxReadSize);
	  if (bytes < 0){
		fprintf(stderr, "ERROR: Keyboard polled but read failed.\n");
		exit(1);
	  } 
	  int i;
	  char n = '\n';
	  for(i = 0; i< bytes; i++){
		switch(buf[i]){
		  case 0x04: //ctrl d character
			//close the writing pipe to the shell so that no other input can be transferred
			close(pipeFT[writeDesc]);
			int status;
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
			fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
			exit(0);
		  case 0x03: //ctrl c character
			kill(pid, SIGINT);
			/*   			close(pipeFT[writeDesc]);
			waitpid(pid, &status, 0);
			retStatus = WEXITSTATUS(status);
			retState = WTERMSIG(status);
			fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", retState, retStatus);
			*/
			break;
		  case '\r':
		  case '\n':
  			write(pipeFT[1], &n, 1);
			write(STDOUT_FILENO, &newline, 2);
			continue;
	      default:
			//printf("%c\n", buf[i]);
			write(STDOUT_FILENO, &buf[i], 1);
			write(pipeFT[1], &buf[i], 1);
			continue;
		}
	  }
	}
	//read from shell process
	if ((polls[1].revents & POLLIN)) { //if 2nd poll is in POLLIN mode
	  int bytes = read(pipeTT[readDesc], buf, maxReadSize);
	  if (bytes < 0){
		fprintf(stderr, "ERROR: Shell process polled but read failed.\n");
		exit(1);
	  } 
	  int i;
	  for (i = 0; i < bytes; i++){
		switch (buf[i]){
		case '\r':
		case '\n':
		  write(1, &newline, 2);
		  continue;
		  //no eof or ctrl-c character to process when reading from shell
		default:
		  write(1, &buf[i], 1);
		}
	  }
	}

	if(polls[1].revents & (POLLERR | POLLHUP)){
		//poll interrupt or poll error occurred
		int status; 
		waitpid(pid, &status, 0);
		int retStatus = WEXITSTATUS(status);
	    int retState = WTERMSIG(status);
		fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
		break;

	}

    if(polls[0].revents & (POLLERR | POLLHUP)){
	  //poll interrupt or poll error occurred                                                                    
	  int status;
	  waitpid(pid, &status, 0);
	  int retStatus = WEXITSTATUS(status);
	  int retState = WTERMSIG(status);
	  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", retState, retStatus);
	  break;

    }


  }

  
}

int main(int argc, char ** argv)
{
  shell_flag = 0;
  static struct option long_options[] = {
	{"shell", no_argument, 0, 's'}
  };
  resetTerminal();
  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"s", long_options , &option_index);
  while(c!=-1){
	switch(c){
	  case 's':
		shell_flag = 1;
		break;
	  default:
		fprintf(stderr, "ERROR: Unrecognized argument.\n Usage: ./lab1a [--shell]\n");
		exit(1);
		break;
	}
	c = getopt_long(argc,argv,"s", long_options , &option_index);
  }
  /*
fork to create a new process, and then exec a shell (/bin/bash, with no arguments other than its name), 
whose standard input is a pipe from the terminal process, and whose standard output and standard error
 are (dups of) a pipe to the terminal process. 
(You will need two pipes, one for each direction of communication, as pipes are unidirectional.)
  */
  
  if (pipe(pipeFT) == -1 || pipe(pipeTT) == -1) {
	fprintf(stderr, "ERROR: Unable to create pipe. \n");
	exit(1);
  }
  if(shell_flag){
	pid = fork();
	if(pid < -1){
	  fprintf(stderr, "ERROR: %s Forked process unsuccessful.\n", strerror(errno));
	  exit(1);
	}
	else if(pid == 0){
	  //execution finished
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
	else{
	  process();
	}
  }
  else{
	//Reading from fd0 and outputting to fd1
	char buf[256];
	while(1){
	  int bytes = read(STDIN_FILENO, &buf, maxReadSize);
	  if(bytes < 0){
		break;
	  }
	  int i;
	  for(i = 0; i < bytes; i++){
		switch(buf[i]){
	      case '\r':
	      case '\n':
		    write(STDOUT_FILENO, &newline, 2);
			continue;
	 	  case 0x03:
			kill(pid, SIGINT);
	      case 0x04:
			closePipes();
			exit(0);
	      default:
			write(STDOUT_FILENO, &buf[i], 1);
			continue;
		}
	  }
	}
  }
  closePipes();
  return 0;
}
