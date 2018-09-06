#define _GNU_SOURCE //to enable pthread_yield
#include "SortedList.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "assert.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element){
  if(list == NULL || element == NULL){
    return;
  }
  //loop through circular list, therefore loop until you hit the head node
  SortedList_t * node = list->next;
  while(node != list){
    if (strcmp(element->key, node->key) <= 0)
      break;
    node = node->next;
    //    if (!node || node->next->prev != node || node->prev->next != node)
    //return;
  }

  if(opt_yield & INSERT_YIELD)
    sched_yield();
  element->next = node;
  element->prev = node->prev;
  node->prev->next = element;
  node->prev = element;
}

int SortedList_delete( SortedListElement_t *element){
  if (!element)
    return 1;
  if(element->next->prev != element || element->prev->next != element){
    //    printf("%s", element->next->prev->key);
    //    printf("%s", element->prev->next->key);
    return 1;
  }
  if(opt_yield & DELETE_YIELD)
    sched_yield();
  element->prev->next = element->next;
  element->next->prev = element->prev;
  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
  if(list == NULL || key == NULL){
    return NULL;
  }
  SortedList_t * node = list->next;
  while(node != list){
    if(strcmp(node->key, key) == 0){
      return node;
    }
    if(opt_yield & LOOKUP_YIELD)
      sched_yield();
    node = node->next;
  }
  return NULL;
  
}

int SortedList_length(SortedList_t *list){
  int len = 0;
  if(list == NULL) { 
    return -1; 
  }
  SortedListElement_t *node = list->next;
  while(node != list)
  {
      len++;
      if(opt_yield & LOOKUP_YIELD)
	sched_yield();
      node = node->next;
  }
  return len;
}

/*int main(){
  SortedList_t* list;
  list = malloc(sizeof(SortedList_t));
  list->next = list;
  list->prev = list;
  list->key = NULL; //circular  
  SortedListElement_t element[5];
  element[0].key = "a";
  element[1].key = "b";
  element[2].key = "c";
  element[3].key  = "d";
  element[4].key = "e";
  int i;
  for (i = 0; i < 5; i++){
    SortedList_insert(list, &element[i]);
  }
  
  char * c = "a";
  SortedListElement_t * node = list->next;
  while(node!=list){
    printf("%s\n", node->key);
    node = node->next;
  }
  printf("%d\n", SortedList_delete(&element[3]));
  SortedListElement_t * f = SortedList_lookup(list, "b");
  assert(SortedList_lookup(list, "z") == NULL);
  printf("%s\n", f->key);
  node = list->next;
  while(node!=list){
    printf("%s\n", node->key);
    node = node->next;
  }
  printf("%d\n", SortedList_length(list));
  return 0;
}
*/
