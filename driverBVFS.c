#include<stdio.h>
#include<stdlib.h>
#include "bvfs.h"

int main(){
  char * file1 = "LOL.txt";
  int fileDesc;
  //Create new file write a file and close it
  bv_init(file1);
  //TEST 1
  //Try to open a file with a name longer than 32 bytes
  if(bv_open("lololololololololololollololooiiii", 0) == -1){
    printf("Test 1 Passed\n");
  }else{
    printf("Test 1 Failed\n");
  }
  //TEST 2
  //Try to close a file that doesnt exist
  if(bv_close(776) == -1){
    printf("Test 2 Passed\n");
  }else{
    printf("Test 2 Failed\n");
  }
  //Open and then write to 256 files

  for(int i=0; i<256; i++){
    char fileName[4];
    sprintf(fileName, "%d",i)
    int fileDesc = bvOpen(fileName,0); 
    if(fileDesc== -1){
      printf("Open Failed on test #%d \n", i); 
    }
    
    if(bv_write(fileDesc, (void *)457, 4) == -1){ 
      printf("Write Failed on test #%d \n", i); 
    }
    if(bv_read(fileDesc, (void *)457, 4) == -1){ 
      printf("Write Failed on test #%d \n", i); 
    }
    if(bvClose(fileDesc) == -1){
      printf("Close Failed on test #%d \n", i); 
    }
  }
  bv_destroy();
  return 0;
  

}
