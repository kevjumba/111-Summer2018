//NAME: Kevin Zhang
//EMAIL: kevin.zhang.13499@gmail.com
//ID: 104939334

#include "stdio.h"
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

int numThreads = 1;
int numIterations = 1;
char lock = 'n'; //no lock
int opt_yield = 0;
pthread_mutex_t mutex;
int spin_flag = 0;
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;  
  if (opt_yield)
    sched_yield();
  *pointer = sum;
}

void * performAddition(void  * counter){
  int i = 0;
  long long prev, update;
  prev = (*((long long *) counter));
  for(; i < numIterations; i++){
    switch(lock){
    case 'n':
      add((long long *)counter, 1);break;
    case 'm': //mutex lock
      pthread_mutex_lock(&mutex);
      add((long long *)counter, 1);
      pthread_mutex_unlock(&mutex);
      break;
    case 's': //spin lock
      while(__sync_lock_test_and_set(&spin_flag, 1));
      add((long long *) counter, 1);
      __sync_lock_release(&spin_flag);
      break;
    case 'c': //compare and swap lock
      do {
	prev = *((long long *)counter);
        update = prev + 1;
	if (opt_yield) { sched_yield(); }
      } while (__sync_val_compare_and_swap((long long *)counter, prev, update) != prev);
      break;
    default:
      fprintf(stderr, "Error, lock unrecognized\n");
      exit(1);
    }
  }
  i = 0;
  for(; i < numIterations; i++){
    long long prev, update;
    prev = *((long long *)counter);
    switch(lock){
    case 'n':
      add((long long *)counter, -1);break;
    case 'm': //mutex lock
      pthread_mutex_lock(&mutex);
      add((long long *)counter, -1);
      pthread_mutex_unlock(&mutex);
      break;
    case 's': //spin lock
      while(__sync_lock_test_and_set(&spin_flag, 1));
      add((long long *) counter, -1);
      __sync_lock_release(&spin_flag);
      break;
    case 'c': //compare and swap lock
      do {
        prev = *((long long *)counter);
        update = prev - 1;
        if (opt_yield) { sched_yield(); }
      } while (__sync_val_compare_and_swap((long long *)counter, prev, update) != prev);
      break;
    default:
      fprintf(stderr, "Error, lock unrecognized\n");
      exit(1);
    }
  }
  return NULL;
}

int main(int argc, char ** argv){
  pthread_t *threads;
  static struct option long_options[] = {
	{"threads", required_argument, 0, 't'},
	{"iterations", required_argument, 0, 'i'},
	{"yield", no_argument, 0, 'y'},
	{"sync", required_argument, 0, 's'},
  };
  long long counter = 0;
  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"ti", long_options , &option_index);
  while(c!=-1){
    switch(c){
    case 't':
      numThreads = atoi(optarg);
      break;
    case 'i':
      numIterations = atoi(optarg);
      break;
    case 'y':
      opt_yield = 1;
      break;
    case 's':
      if(strlen(optarg)==1){
	switch(optarg[0]){
	case 's':
	case 'm':
	case 'c':
	  lock = optarg[0];
	  break;
	default:
	  fprintf(stderr, "ERROR: Unrecognized sync argument.\n");
	  exit(1);
	}
	break;
      }
    default:
	  fprintf(stderr, "ERROR: Unrecognized argument.\n Usage: ./lab2a [--threads=n] [--iterations=n] [--yield] [--sync=c]\n");
	  exit(1);
	  break;
    }
    c = getopt_long(argc,argv,"s", long_options , &option_index);
  }
  if(lock == 'm'){
    pthread_mutex_init(&mutex, NULL);
  }
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  threads = malloc(numThreads * sizeof(pthread_t));
  int t;
  for(t = 0; t < numThreads; t++){
	int rc = pthread_create(&threads[t], NULL, performAddition, &counter);
	if(rc){
	  fprintf(stderr, "Creation of thread number, %d, failed. ", t);
	  exit(1);
	}
  }

  for(t= 0; t<numThreads; t++){
	int rc = pthread_join(threads[t], NULL);
	if(rc){
      fprintf(stderr, "Joining of thread number, %d, failed. ", t);
      exit(1);
    }
  }
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  long long total = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
  int operations = numThreads * numIterations *2;
  long long avg = total/operations;
  char str[30] = "add";
  char temp[2];
  if (opt_yield) strcat(str, "-yield");
  switch(lock){
  case 'n':
    strcat(str, "-none");
    break;
  case 'm':
  case 's':
  case 'c':
    strcat(str, "-");
    temp[0] = lock;
    temp[1] = '\0';
    strcat(str, temp);
    break;
  }

  printf("%s,%d,%d,%d,%lld,%lld,%lld\n", str, numThreads, numIterations, 
		 operations, total , avg, counter);
  free(threads);
  pthread_mutex_destroy(&mutex);
}
