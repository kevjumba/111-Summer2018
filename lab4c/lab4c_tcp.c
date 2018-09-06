//NAME: Kevin Zhang
//EMAIL: kevin.zhang.13499@gmail.com     
//ID: 104939334  
#include "stdio.h"
#include <ctype.h>
#include "getopt.h"
#include "mraa.h"
#include <mraa/aio.h>
#include <mraa/gpio.h>
#include <sys/time.h>
#include "math.h"
#include "sys/types.h"
#include "poll.h"
#include "string.h"
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

mraa_aio_context temp;
mraa_gpio_context button;

char * logFile = NULL;
int log_flag = 0;
int period = 1;
char scale = 'F';
int logfd = 0;
int stopped = 0;
FILE * f = NULL;
char * id = "";
char * hostname = "";
int port = 0;
struct hostent *server;
struct sockaddr_in serv_addr;
int sockfd;

void output(char * str, int dest){
  if(dest){
    dprintf(sockfd, "%s", str);
  }
  fprintf(stderr, "%s", str);
  if(log_flag){
    fprintf(f, "%s", str);
    fflush(f);
  }
}
void shutDown(){
  struct timeval start;
  struct tm* formattedTime;
  gettimeofday(&start, 0);
  formattedTime = localtime(&(start.tv_sec));
  char buf[100];
  sprintf(buf, "%02d:%02d:%02d SHUTDOWN\n", formattedTime->tm_hour, formattedTime->tm_min, formattedTime->tm_sec);
  output(buf, 1);
  int status = mraa_aio_close(temp);
  if (status != MRAA_SUCCESS) {
    fprintf(stderr, "Cannot close temperature sensor.\n");
    exit(1);	
  }
  /* release gpio's */
  status = mraa_gpio_close(button);
  if (status != MRAA_SUCCESS) {
    fprintf(stderr, "Cannot close gpio button\n");
    exit(1);
  }
  fclose(f);
  exit(0);
}

double getTemp(){
  int reading = mraa_aio_read(temp);
  int B = 4275;                                                                                   
  double R = 1023.0 / ((double)reading) - 1.0;
  R *= 100000.0;
  //float temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15;
  double celsius = 1.0 / (log(R/100000.0)/B + 1/298.15) - 273.15;
  double fahrenheit = celsius * 9/5.0 + 32;
  switch (scale){
  case 'F': return fahrenheit; break;
  case 'C': return celsius; break;
  default: 
    fprintf(stderr, "Temperature Scaling Error, No Such Scaling.\n");
    shutDown();
    exit(1);
    break;
  }
}

int getPeriod(char * buf){
  int length = 0;
  int i ;
  int len = strlen(buf);
  for(i = 7; i<len;i++){
    if(isdigit(buf[i])){
      length++;
    }
    else{
      break;
    }
  }
  if(length == 0){
    return -1;
  }
  char * subbuff = malloc((length+1)*sizeof(int));
  memcpy( subbuff, &buf[7], length);
  subbuff[length] = '\0';
  return atoi(subbuff);
}
void process(char * buf);
void processServerInput(char * buf, int size){
  char * i = buf;
  while(i < &buf[size]){
    char * j = i;
    int len = 0;
    while(j < &buf[size] && *j != '\n'){
      len++;
      j++;
    }
    char * subbuff = malloc(len+2);
    memcpy(subbuff, i, len);
    subbuff[len]='\n';
    subbuff[len+1] = '\0';
    process(subbuff);
    i = &j[1];
  }
}
void process(char * buf){
//  fprintf(stderr, "Processing Buffer %s\n", buf);
  if(strcmp(buf, "OFF\n")==0){
    if(log_flag){
      fprintf(f, "%s", buf);
    }
    shutDown();
  }
  else if(strcmp(buf, "START\n") == 0){
    //printf("Start\n");
    stopped = 0;
    if(log_flag){
      fprintf(f, "%s", buf);
    }
  }
  else if(strcmp(buf, "STOP\n") == 0){
    //printf("stop\n");
    stopped = 1;
    if(log_flag){
      fprintf(f, "%s", buf);
    }
  }
  else if(strcmp(buf, "SCALE=F\n") ==0){
    scale = 'F';
    if(log_flag){
      fprintf(f, "%s", buf);
    }
  }
  else if(strcmp(buf, "SCALE=C\n") == 0){
    scale = 'C';
    if(log_flag){
      fprintf(f, "%s", buf);
    }
  }
  else if(strlen(buf)>4 && buf[0] == 'L' && buf[1] == 'O' && buf[2] == 'G'){
    if(log_flag){
      fprintf(f, "%s", buf);
    }
  }
  else if(strlen(buf)>7){
    char subbuff[8];
    memcpy( subbuff, buf, 7 );
    subbuff[8] = '\0';
    if(strcmp(subbuff, "PERIOD=")==0){
      fprintf(f, "%s", buf);		
      int newPeriod = getPeriod(buf);
      if(newPeriod!=-1){
	period = newPeriod;
      } 
    }
    else{
      fprintf(stderr, "Invalid Command %s\n", buf);
    }
  }
  else{
    fprintf(stderr, "Invalid Command %s\n", buf);
  }
  fflush(f);    
}

void setup_client(){
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "ERROR opening socket\n\r");
    exit(1);
  }
  server = gethostbyname(hostname);
  if(server == NULL){
    fprintf(stderr, "ERROR: Cannot connect to server at hostname %s\n\r", hostname);
    exit(1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    fprintf(stderr, "ERROR connecting to client socket exiting\n\r");
    exit(1);
  }
  
}
int main(int argc, char** argv){
  /* initialize AIO */
  static struct option long_options[] = {
    {"period", required_argument, 0, 'p'},
    {"scale", required_argument, 0, 's'},
    {"log", required_argument, 0, 'l'},
    {"id", required_argument, 0, 'i'},
    {"host", required_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"pslih", long_options , &option_index);
  while(c!=-1){
    switch(c){
    case 'p':
      period = atoi(optarg);
      break;
    case 's':
      switch(optarg[0]){
      case 'F':
      case 'C':
        scale = optarg[0];
	break;
      default:
        fprintf(stderr, "Not a recognized scale. Use F/C for Fahrenheit/Celsius only.\n");
        exit(1);
      }
      break;
    case 'l':
      log_flag = 1;
      logFile = optarg;
      break;
    case 'i':
      id = optarg;
      break;
    case 'h':
      hostname= optarg;
      break;
    default:
      fprintf(stderr, "ERROR: Unrecognized argument.\n Usage: ./lab4b [--period=num] [--scale=c] [--log=FILE]\n");
      exit(1);
      break;
    }
    c = getopt_long(argc,argv,"psl", long_options , &option_index);
  }
  if(optind <argc){
    port = atoi(argv[optind]);
    if(port < 0) {
      fprintf(stderr, "Invalid Port\n");
      exit(1);
    }
  }
  if (logFile!=NULL){
    //printf("Creating log file, %s\n", logFile);
    f = fopen(logFile, "w");
    if(f==NULL){
      fprintf(stderr, "ERROR: Unable to open log file.\n");
    	exit(1);
    }
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "ERROR opening socket\n\r");
    exit(1);
  }
  server = gethostbyname(hostname);
  if(server == NULL){
    fprintf(stderr, "ERROR: Cannot connect to server at hostname %s\n\r", hostname);
    exit(1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    fprintf(stderr, "ERROR connecting to client socket exiting\n\r");
    exit(1);
  }
  char buf[20];
  sprintf(buf, "ID=%s", id);
  output(buf, 1);
  
  temp = mraa_aio_init(1);
  if (temp == NULL) {
    fprintf(stderr, "Failed to initialize AIO\n");
    mraa_deinit();
    exit(1);
  }
  button = mraa_gpio_init(60);
  if (button == NULL) {
    fprintf(stderr, "Failed to initialize GPIO 60\n");
    mraa_deinit();
    exit(1);
  }
  mraa_gpio_dir(button, MRAA_GPIO_IN);
  
  struct pollfd pollfd[1];
  pollfd[0].fd = sockfd;
  pollfd[0].events = POLLIN | POLLHUP | POLLERR;
  struct timeval t;
  time_t next = 0;
  struct tm* formattedTime;
  char cmd[256];
  while(1){
    int ret = poll(pollfd, 1, 0);
    if(ret < 0) {
      fprintf(stderr, "ERROR: Polling failed.\n");
      exit(1);
    }
    if((pollfd[0].revents & POLLIN)){
      /*int bytes = read(STDIN_FILENO, buf, 100);
      if(bytes<=0){
      continue;
      }
      char * temp = malloc((bytes+1)*sizeof(char));
      memcpy(temp, buf, bytes);
      temp[bytes]='\0';*/
      int bytes = read(sockfd, cmd, 256);
      if(bytes < 0){
	fprintf(stderr, "Error: Failed to read from server\n");
	exit(1);
      }
      processServerInput(cmd, bytes);
    }
    gettimeofday(&t, 0);

    double temperature = getTemp();

    if (t.tv_sec >= next && !stopped){
      formattedTime = localtime(&(t.tv_sec));
      char buf[100];
      sprintf(buf, "%02d:%02d:%02d %.1f\n", formattedTime->tm_hour, formattedTime->tm_min, formattedTime->tm_sec, temperature);
      output(buf, 1);
      next = t.tv_sec + period;
    }
  }
  shutDown();
  exit(0);
}

