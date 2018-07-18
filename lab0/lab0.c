/*
  Kevin Zhang
  kevin.zhang.13499@gmail.com
  104939334
 */

#include <unistd.h>
#include "stdlib.h"
#include <fcntl.h>
#include "stdio.h"
#include "getopt.h"
#include "signal.h"
#include "unistd.h"
#include "errno.h"
#include "sys/stat.h"
#include "string.h" 

void segfault_handler(){
	fprintf(stderr, "Segmentation fault caught: SIGSEGV\n");
	exit(4);
}

int main(int argc, char ** argv){
	int input_fd;
	int output_fd;
	int segfault = 0;
	//using the given example code from the official long_options documentation
	static struct option long_options[] =
	{
		{"input", 	required_argument, 0, 'a'},
		{"output", 	required_argument, 0, 'b'},
		{"segfault", no_argument, 	   0, 'c'},
		{"catch", no_argument,         0, 'd'},
		{0, 0, 0, 0} //ending 0 array
	};

	char * in  = NULL;
	char * out = NULL;
	int c;
	int option_index = 0;
	c = getopt_long(argc, argv, "",
					long_options, &option_index); 
	while(c != -1){
  		if(c == -1){
			break;
		}
		/*		if(c!=0){
		  fprintf(stderr, "Unrecognized argument. \nusage: ./lab0 [--input inputFile] [--output outputFile] [--segfault] [--catch] \n");
		  exit(1);
		  }*/
		switch(c){
			case 'a':
			//input filename
				in = optarg;
				break;
			case 'b': 
			//create file and report any errors in creation
				out = optarg; 
				break;
			case 'c': 
				segfault = 1;
				break;
			case 'd': 
				signal(SIGSEGV, segfault_handler);
				break;
			default:
			  fprintf(stderr, "Unrecognized argument. \nusage: ./lab0 [--input inputFile] [--output outputFile] [--segfault] [--catch] \n");                                 exit(1);
			  break;
		}
		c = getopt_long(argc, argv, "",
						long_options, &option_index);
	}

	if(in != NULL){
		input_fd = open(in, O_RDONLY); //open the argument file in read only mode
		if(input_fd>=0){
			close(0);
			dup(input_fd); //duplicates the file descriptor
			close(input_fd);
		}
		else{
			fprintf(stderr, "ERROR: %s Unable to open input file %s. \n",strerror(errno), in);
			exit(2);
		}
	}
	if(out != NULL){
		mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		output_fd = creat(out, mode);
		if(output_fd>=0){
			close(1);
			dup(output_fd); //duplicates the output file descriptor
			close(output_fd);
		}
		else{
			fprintf(stderr, "Error %s, unable to create file %s. \n", strerror(errno), out);
			exit(3);
		}
	}

    if(segfault){
	  //generate a segfault by attempting to store to a null pointer                                                        
	  char * str = NULL;
	  *str = 't';
	  //*str = "temp"
    }

	char * buf;
	buf = (char *) malloc(sizeof(char));
	while (read(STDIN_FILENO, buf, 1)!=0){
	  write(STDOUT_FILENO, buf, 1);
	}
	free(buf);
	exit(0);
	
}

