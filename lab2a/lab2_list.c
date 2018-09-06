//NAME: Kevin Zhang
//EMAIL: kevin.zhang.13499@gmail.com
//ID: 104939334

#include "SortedList.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

int numThreads = 1;
int numIterations = 1;
char lock = 'n'; //no lock
char yield[10]; //no yield
int opt_yield = 0;
pthread_mutex_t mutex;
int spin_flag = 0;
int length = 0;
SortedList_t* linked_list;
SortedListElement_t * elements;
void setYield(char * opt){
  int i;
  int len = strlen(opt);
  for(i = 0; i < len; i++){
    switch (opt[i]){
	case 'i': 
	  opt_yield |= INSERT_YIELD;
	  break;
	case 'd': 
	  opt_yield |= DELETE_YIELD;
	  break;
	case 'l': 
	  opt_yield |= LOOKUP_YIELD;
	  break;
	default:
	  fprintf(stderr, "Cannot recognize yield argument!\n");
	  exit(1);
	}
  }
  strcat(yield, "-");
  strcat(yield, opt);
}

void * list_update(void * threadId){
  int i;
  //  printf("%d %d", numThreads, numIterations);

  //printf("I'm executing list update\n");
  //threadId must be void * to fulfill pthread argument requirements
  for (i = (*(int *) threadId); i < numThreads*numIterations; i += numThreads){
    //    printf("Im in the for loop\n");
    switch (lock){
    case 'n':
      //printf("i'm inserting");
      SortedList_insert(linked_list, &elements[i]);
      break;
    case 'm':
      pthread_mutex_lock(&mutex);
      SortedList_insert(linked_list, &elements[i]);
      pthread_mutex_unlock(&mutex);
      break;
    case 's':
      while (__sync_lock_test_and_set(&spin_flag, 1));
      SortedList_insert(linked_list, &elements[i]);
      __sync_lock_release(&spin_flag);
      break;
    default:
      fprintf(stderr, "ERROR: Invalid Lock. \n");
      exit(1);
    }
  }
  length = SortedList_length(linked_list);
  /*  
  SortedListElement_t * node = linked_list->next;
  while(node != linked_list){
    printf("%s\n", node->key);
    node =node->next;
    }*/
  SortedListElement_t * t;
  for(i = *(int *) threadId; i < numThreads*numIterations; i+=numThreads){
    switch (lock){
    case 'n':
      t = SortedList_lookup(linked_list, elements[i].key);
      if(t){
	//	printf("i'm deleting\n");
        if(SortedList_delete(t)){
	  fprintf(stderr, "Cannot Delete\n");
	  exit(2);
	};
      }
      else{
        fprintf(stderr, "Invalid deletion, no such element, %d", i);
        exit(2);
      }
      break;
    case 'm':
      pthread_mutex_lock(&mutex);
      t = SortedList_lookup(linked_list, elements[i].key);
      if(t){
        SortedList_delete(t);
      }
      else{
        fprintf(stderr, "Invalid deletion, no such element, %d", i);
        exit(2);
      }
      pthread_mutex_unlock(&mutex);
      break;
    case 's':
      while (__sync_lock_test_and_set(&spin_flag, 1));
      t = SortedList_lookup(linked_list, elements[i].key);
      if(t){
        SortedList_delete(t);
      }
      else{
        fprintf(stderr, "Invalid deletion, no such element, %d", i);
        exit(2);
      }
      __sync_lock_release(&spin_flag);
      break;
    default:
      fprintf(stderr, "ERROR: Invalid Lock. \n");
      exit(1);
    }
  }
  return NULL;
}

void signal_handler(){
  fprintf(stderr, "SIGNAL SEGFAULT\n");
  exit(1);
}

int main(int argc, char ** argv){
  srand(time(NULL));
  signal(SIGSEGV, signal_handler);
  pthread_t *threads;
  static struct option long_options[] = {
	{"threads", required_argument, 0, 't'},
	{"iterations", required_argument, 0, 'i'},
	{"yield", required_argument, 0, 'y'},
	{"sync", required_argument, 0, 's'},
  };

  int c;
  int option_index = 0;
  c = getopt_long(argc,argv,"tiys", long_options , &option_index);
  while(c!=-1){
    switch(c){
    case 't':
      numThreads = atoi(optarg);
      break;
    case 'i':
      numIterations = atoi(optarg);
      break;
    case 'y':
      setYield(optarg);
      //i for insert, d for delete, l for lookup
      break;
    case 's':
      if(strlen(optarg)==1){
	switch(optarg[0]){
	case 'm':
	case 's':
	  lock = optarg[0];
	  break;
	default:
	  fprintf(stderr, "ERROR: Unrecognized sync arguments.\n");
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
  //  printf("%c", lock);
  // printf("%d %d", numThreads, numIterations);

  linked_list = malloc(sizeof(SortedList_t));
  linked_list->next = linked_list;
  linked_list->prev = linked_list;
  linked_list->key = NULL; //circular
  //creates and initializes (with random keys) the required number (threads x iterations) of list elements
  elements = malloc(sizeof(SortedListElement_t)*numThreads*numIterations);
  int i;
  for(i = 0; i <numThreads*numIterations; i++){
    char * randStr = malloc(2);
    char letter = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[(int)(rand()) % 26];
    randStr[0] = letter;
    randStr[1] = '\0';
    elements[i].key = randStr;
  }
  //printf("we finished generating rand keys\n");
  struct timespec start;
  threads = malloc(numThreads * sizeof(pthread_t));
  int t;
  int * threadIds = malloc(numThreads * sizeof(int));
  clock_gettime(CLOCK_MONOTONIC, &start);

  for(t = 0; t < numThreads; t++){
    threadIds[t]  = t;  
    int rc = pthread_create(&threads[t], NULL, list_update, &threadIds[t]);
    if(rc){
      fprintf(stderr, "Creation of thread number, %d, failed. ", t);
      exit(1);
    }
  }

  for( t = 0; t<numThreads; t++){
    int rc = pthread_join(threads[t], NULL);
    if(rc){
      fprintf(stderr, "Joining of thread number, %d, failed. ", t);
      exit(1);
    }
  }

  //printf("We finished all threads and length didnt give an error. \n");
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  length = SortedList_length(linked_list);
  if(SortedList_length(linked_list)!=0){
    fprintf(stderr, "Linked List did not end up with length 0\n");
    exit(2);
  }
  long long total = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
  int operations = numThreads*numIterations*3;
  long long avg = total/operations;
  char str[30] = "list";
  if (!opt_yield)
    strcat(str, "-none");
  
  strcat(str, yield);

  switch (lock){
  case 'n': strcat(str, "-none"); break;
  case 'm': strcat(str, "-m"); break;
  case 's': strcat(str, "-s"); break;
  }

  printf("%s,%d,%d,1,%d,%lld,%lld\n", str, numThreads, numIterations, operations, total, avg);
  free(elements);
  free(threadIds);
  free(threads);
  pthread_mutex_destroy(&mutex);
  return 0;
}
