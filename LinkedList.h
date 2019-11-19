include<stdlib.h>
#include<stdio.h>

Node *head = NULL; 

struct Node{
  struct Node *next;
  struct iNode fileNode;
}typedef Node;


void list_print(Node *head) {
  struct Node *curr;
  printf("{");
  for(curr=head; curr != NULL; curr=curr->next) {
    printf("[%d : %d : %p]", curr->start, curr->end, curr->next);
    if (curr->next != NULL)
      printf(", ");
  }
  printf("}\n");
}
void add_Node(iNode i){
  //CASE WHERE HEAD IS NULL
  if(head == NULL){
    head->next = NULL;
    head->fileNode = i;
  }else{
    struct Node *curr;
    struct Node *newNode;
    for(curr=head; curr!=NULL; curr=curr->next){
      if(curr->next == NULL){
        newNode->fileNode = i;
        newNode->next = NULL;
        curr->next = newNode;
      }
    }  
  }
}
void remove_Node(const char *fileName){
  if(head = NULL) return;
  if(head->next == NULL){
    head = NULL;
    return;
  }
}
