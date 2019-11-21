/* CMSC 432 - Homework 7
 * Assignment Name: bvfs - the BV File System
 * Due: Thursday, November 21st @ 11:59 p.m.
 */


/*
 * [Requirements / Limitations]
 *   Partition/Block info
 *     - Block Size: 512 bytes
 *     - Partition Size: 8,388,608 bytes (16,384 blocks)
 *
 *   Directory Structure:
 *     - All files exist in a single root directory
 *     - No subdirectories -- just names files
 *
 *   File Limitations
 *     - File Size: Maximum of 65,536 bytes (128 blocks)
 *     - File Names: Maximum of 32 characters including the null-byte
 *     - 256 file maximum -- Do not support more
 *
 *   Additional Notes
 *     - Create the partition file (on disk) when bv_init is called if the file
 *       doesn't already exist.
 */
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
//fileDescriptor struct
struct fdTable{
  int cursor;
  int mode;
  int isOpen;

} typedef fdTable;

//Structs
struct iNode{
  char name[32];
  int numBytes;
  short numBlocks;
  time_t time;
  //File can only be 128 blocks long
  short blockAddresses[128];

}typedef iNode;


//Constants
//BLOCK_SIZE and FILE_NAME_SIZE are Bytes
//PARTION_SIZE and FILE_SIZE are Blocks
const int BLOCK_SIZE = 512;
const int FILE_NAME_SIZE = 32;
const int PARTION_SIZE = 16384;
const int FILE_SIZE = 128;
const int MAX_FILES = 256;

//Globals
iNode* iNodeArray[256];
fdTable* fdtArr[256];
int num_files = 0;
int pFD;
//offset(address) of first block dedicated to pointing to free blocks
short SUPERPTR = 257;

// Prototypes
int bv_init(const char *fs_fileName);
int bv_destroy();
int bv_open(const char *fileName, int mode);
int bv_close(int bvfs_FD);
int bv_write(int bvfs_FD, const void *buf, size_t count);
int bv_read(int bvfs_FD, void *buf, size_t count);
int bv_unlink(const char* fileName);
void bv_ls();

//function to remove blocks from an iNodes diskmap - put them back in super block
void removeDiskMap(iNode* file){
  int SPNEXT = SUPERPTR;
  
  //set head to begining of freed blocks
  SUPERPTR = file->blockAddresses[0];
  lseek(pFD, 0, SEEK_SET);
  write(pFD, (void*)&SUPERPTR, sizeof(short));
  
  //link blocks in iNode together
  for(int i=0; i<file->numBlocks-1; i++){
    //Seek to one of the file blocks (blockAddresses[i])
    lseek(pFD, (BLOCK_SIZE * file->blockAddresses[i]), SEEK_SET);
    //Write blockAddresses[i+1] to the block we seeked to 
    write(pFD, (void *)&(file->blockAddresses[i+1]), sizeof(short));
  }
  
  //set end of now free blocks to the rest of the super block list
  lseek(pFD, (BLOCK_SIZE * file->blockAddresses[file->numBlocks-1]), SEEK_SET);
  write(pFD, (void *)&SPNEXT, sizeof(short));
}

//function to get the next free block and take it out of the super block list - returns block that is "theirs" to write to 
short getSuperBlock(){
  //No super blocks left
  if(SUPERPTR == 16385){ 
    return -1;
  }
  
  short val = SUPERPTR;
  short tmp;
  
  //seek to next free block - put it in tmp
  lseek(pFD,  SUPERPTR * BLOCK_SIZE, SEEK_SET);
  read(pFD, (void *)&tmp, sizeof(short));
  
  //Seek to beginning and reset Head ptr
  lseek(pFD, 0, SEEK_SET);
  write(pFD, (void*)&tmp, sizeof(short));
  
  //set SUPER global and return
  SUPERPTR = tmp;
  return val;  
}

//helper function to load data structures we use from disk into memory
void buildMemStructs(int id){
  //seek to begining of partition - posistion of super block
  lseek(id, 0, SEEK_SET);
  //Read super block ptr
  read(id, (void*)&SUPERPTR, sizeof(short));
  //Seek to second block
  lseek(id, (BLOCK_SIZE -sizeof(short)), SEEK_CUR);
  //Read iNodes from Disk
  for(int i=0; i<256; i++){
    iNode *newNode =(iNode *) malloc(sizeof(iNode));
    read(id, (void*)newNode, sizeof(iNode));
    lseek(id, (BLOCK_SIZE - sizeof(iNode)), SEEK_CUR); 
    iNodeArray[i] = newNode;
    
    //malloc and intialize filedescriptors
    fdTable *fd = (fdTable *) malloc(sizeof(fdTable));
    fd->mode = -1;
    fd->cursor = 0;
    fd->isOpen = 0;
    fdtArr[i] = fd;
  }
}

/*
 * int bv_init(const char *fs_fileName);
 *
 * Initializes the bvfs file system based on the provided file. This file will
 * contain the entire stored file system. Invocation of this function will do
 * one of two things:
 *
 *   1) If the file (fs_fileName) exists, the function will initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 *   2) If the file (fs_fileName) does not exist, the function will create that
 *   file as the representation of a new file system and initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 * Input Parameters
 *   fs_fileName: A c-string representing the file on disk that stores the bvfs
 *   file system data.
 *
 * Return Value
 *   int:  0 if the initialization succeeded.
 *        -1 if the initialization failed (eg. file not found, access denied,
 *           etc.). Also, print a meaningful error to stderr prior to returning.
 */
int bv_init(const char *fs_fileName) {

  pFD = open(fs_fileName, O_CREAT | O_RDWR | O_EXCL, 0644);
  if (pFD < 0) {
    if (errno == EEXIST) {
      // File already exists. Open it and read info (integer) back
      pFD = open(fs_fileName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);
      //read files from 
      buildMemStructs(pFD);

    }
    else {
      // Something bad must have happened... check errno?
      fprintf(stderr, "%s", strerror(errno));
      return -1;
    }

  } else {
    // File did not previously exist but it does now. Write data to it 
    //write 2 bytes for first superBlock ptr
    short temp = 257;
    write(pFD, (void*)&temp, sizeof(short));
    //seek to next block
    lseek(pFD, 510, SEEK_CUR);

    //write inodes
    iNode node;
    for(int i=0; i<256; i++){
      bzero(&node, sizeof(node));
      node.numBytes = -1;
      //node.pos = i+1;

      //write it to file
      write(pFD, (void*)&node, sizeof(iNode));

      //seek to next iNode location
      lseek(pFD, 512 - sizeof(iNode), SEEK_CUR); 
    }

    //write remaining superBlock pointers - 2 bytes pointing to the next super block
    for(int i=257; i<16384; i++){
      //write short of next free block (very next block in this case)
      int j=i+1;
      write(pFD, (void*)&j, sizeof(short));
      //seek to the next block
      lseek(pFD, 510, SEEK_CUR);
    }

    //set up all data structures in memory
    buildMemStructs(pFD);
   
    //intialize numBytes for every iNode - its used to check if that iNode is assigned a file
    for(int i=0; i<256; i++){
      iNodeArray[i]->numBytes = -1;
    }
  }
}


/*
 * int bv_destroy();
 *
 * This is your opportunity to free any dynamically allocated resources and
 * perhaps to write any remaining changes to disk that are necessary to finalize
 * the bvfs file before exiting.
 *
 * Return Value
 *   int:  0 if the clean-up process succeeded.
 *        -1 if the clean-up process failed (eg. bv_init was not previously,
 *           called etc.). Also, print a meaningful error to stderr prior to
 *           returning.
 */
int bv_destroy() {
  iNode *node;
  //seek past first superblock
  lseek(pFD, BLOCK_SIZE, SEEK_SET);
  //write iNodes to disk
  for(int i=0; i<256; i++){
    node = iNodeArray[i];
    write(pFD, (void*)node, sizeof(iNode));
    lseek(pFD, 512 - sizeof(iNode), SEEK_CUR);
  }

  //free fdTABLE and iNodes
  for(int i=0; i<256; i++){
    free(iNodeArray[i]);
    free(fdtArr[i]);
  }

  //close file descriptor
  close(pFD);
}

// Available Modes for bvfs (see bv_open below)
int BV_RDONLY = 0;
int BV_WCONCAT = 1;
int BV_WTRUNC = 2;

/*
 * int bv_open(const char *fileName, int mode);
 *
 * This function is intended to open a file in either read or write mode. The
 * above modes identify the method of access to utilize. If the file does not
 * exist, you will create it. The function should return a bvfs file descriptor
 * for the opened file which may be later used with bv_(close/write/read).
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *   mode: The access mode to use for accessing the file
 *           - BV_RDONLY: Read only mode
 *           - BV_WCONCAT: Write only mode, appending to the end of the file
 *           - BV_WTRUNC: Write only mode, replacing the file and writing anew
 *
 * Return Value
 *   int: >=0 Greater-than or equal-to zero value representing the bvfs file
 *           descriptor on success.
 *        -1 if some kind of failure occurred. Also, print a meaningful error to
 *           stderr prior to returning.
 */
int bv_open(const char *fileName, int mode) {
  if(strlen(fileName) >= 31){
    printf("Filename to long\n");
    return -1;
  }
  if(mode > 2 || mode < 0){
    printf("Invalid  Mode\n");
    return -1;
  }

  //check if we need to create the file or not
  iNode *file = NULL;
  fdTable *fdt = NULL;
  for(int i=0; i<256; i++){
    if(!strcmp(iNodeArray[i]->name, fileName) && iNodeArray[i]->numBytes != -1){
      //found a file with that name
      file = iNodeArray[i];
      fdt = fdtArr[i];
      //check if the file is already open 
      if(fdt->isOpen == 1){
        printf("File is already open\n");
        return -1;
      }
      //set up file descriptor
      fdt->isOpen = 1;
      fdt->mode = mode;
            
      if(mode == BV_WCONCAT){
        fdt->cursor = iNodeArray[i]->numBytes; 
      }
      else if(mode == BV_WTRUNC){
        removeDiskMap(iNodeArray[i]);
        fdt->cursor = 0;
      }
      else{
        fdt->cursor = 0;
      }
      //add it to the array
      fdtArr[i] = fdt;
      return i; 
    }
  }
  if(mode == BV_RDONLY){
    printf("Tried to read a file that deosn't exist\n");
    return -1;
  }
  if(num_files < 256){
    //file doesn't exist so make it
    for(int j=0; j<256; j++){
      if(iNodeArray[j]->numBytes == -1){
        strcpy(iNodeArray[j]->name, fileName);
        iNodeArray[j]->time = time(NULL);
        iNodeArray[j]->numBytes = 0;
        //set up file descriptor
        fdtArr[j]->isOpen = 1;
        fdtArr[j]->mode = mode;
        fdtArr[j]->cursor = 0;
        num_files ++;
        
        return j;
      }
    }
  }
  else{
    printf("Too many files already exist - hit maximum\n");
    return -1;
  }
}

/*
 * int bv_close(int bvfs_FD);
 *
 * This function is intended to close a file that was previously opened via a
 * call to bv_open. This will allow you to perform any finalizing writes needed
 * to the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *
 * Return Value
 *   int:  0 if open succeeded.
 *        -1 if some kind of failure occurred (eg. the file was not previously
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_close(int bvfs_FD) {
  //check if file exits - if not return -1
  if(fdtArr[bvfs_FD]->isOpen == 0){
    printf("File is not open\n");
    return -1;
  }

  if(iNodeArray[bvfs_FD]->numBytes == -1){
    printf("File doesn't exist\n");
    return -1;
  }else{
  
    //Reset the file descriptor
    fdtArr[bvfs_FD]->mode = -1;
    fdtArr[bvfs_FD]->cursor = 0;
    fdtArr[bvfs_FD]->isOpen = 0;
    
    
    return 0;
  }
}

/*
 * int bv_write(int bvfs_FD, const void *buf, size_t count);
 *
 * This function will write count bytes from buf into a location corresponding
 * to the cursor of the file represented by bvfs_FD.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to write to.
 *   buf: The buffer containing the data we wish to write to the file.
 *   count: The number of bytes we intend to write from the buffer to the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes written to the file.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_write(int bvfs_FD, const void *buf, size_t count) {
  //checking if file is open
  if(fdtArr[bvfs_FD]->isOpen == 0){
    printf("File is not open -- WRITE %d\n",bvfs_FD); 
    return -1;
  }
  //checking if their is a file for this fd
  if(iNodeArray[bvfs_FD]->numBytes == -1){
    printf("File Doesn't exist\n");
    return -1;
  }
  //checking mode
  if(fdtArr[bvfs_FD]->mode == BV_RDONLY){
    printf("File opened in wrong mode\n");
    return -1;
  }
  else{
    //should be to the point where we can right 
    
    int bytesToWrite = count;
    int totalBytesWritten = 0;  
    while(bytesToWrite != 0){
      int targetBlock = fdtArr[bvfs_FD]->cursor / BLOCK_SIZE;
      int blockOffset = fdtArr[bvfs_FD]->cursor % BLOCK_SIZE;
      int spaceLeft = BLOCK_SIZE-blockOffset; 
      int bytesWritten = 0;
      if(blockOffset == 0){
        short newBlockID = getSuperBlock();
        if(newBlockID == -1){
          //printf("NO BLOCKS LEFT");
          return bytesWritten;
        }
        iNodeArray[bvfs_FD]->blockAddresses[targetBlock] = newBlockID;
      }

      int offset = iNodeArray[bvfs_FD]->blockAddresses[targetBlock]; 
      lseek(pFD, (offset*BLOCK_SIZE) + blockOffset, SEEK_SET);
      if(bytesToWrite < spaceLeft){
        bytesWritten = write(pFD, buf+totalBytesWritten, bytesToWrite);
        
      }else{
        bytesWritten = write(pFD, buf+totalBytesWritten, spaceLeft);
      }

      totalBytesWritten += bytesWritten;
      bytesToWrite -= bytesWritten;
      fdtArr[bvfs_FD]->cursor += bytesWritten; 
    }
    
    //Update iNode with appropriate numBytes and timestamp
    iNodeArray[bvfs_FD]->numBytes += totalBytesWritten;
    iNodeArray[bvfs_FD]->time = time(NULL);
    return totalBytesWritten;
  }
}

/*
 * int bv_read(int bvfs_FD, void *buf, size_t count);
 *
 * This function will read count bytes from the location corresponding to the
 * cursor of the file (represented by bvfs_FD) to buf.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to read from.
 *   buf: Th/e buffer that we will write the data to.
 *   count: The number of bytes we intend to write to the buffer from the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes written to buf.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_read(int bvfs_FD, void *buf, size_t count) {
  //check if file is open
  if(fdtArr[bvfs_FD]->isOpen == 0){
    printf("File is not open\n");
    return -1;
  }
  //check if file exists
  if(iNodeArray[bvfs_FD]->numBytes == -1){
    printf("File doesn't exist\n");
    return -1;
  }
  if(fdtArr[bvfs_FD]->cursor + count >  iNodeArray[bvfs_FD]->numBytes){
    printf("Asking to read more than the size of current file\n");
    return -1;
  }
  //check mode
  if(fdtArr[bvfs_FD]->mode == BV_RDONLY){
    //while theres still blocks to read
    int bytesLeft = count;
    int totalBytesRead =0;  
    while(bytesLeft != 0){
      int bytesRead = 0;
      int targetBlock = fdtArr[bvfs_FD]->cursor / BLOCK_SIZE;
      int blockOffset = fdtArr[bvfs_FD]->cursor % BLOCK_SIZE;
      int spaceLeft = BLOCK_SIZE-blockOffset; 
      int offset = iNodeArray[bvfs_FD]->blockAddresses[targetBlock]; 
      //Seek to new block
      lseek(pFD, offset*BLOCK_SIZE+blockOffset, SEEK_SET); 
      //If spaace left in current block read it all 
      if(bytesLeft <= spaceLeft){
        bytesRead += read(pFD, buf + totalBytesRead, bytesLeft);
      }
      //Not enough space left in current block reading what we can then
      else{
        bytesRead += read(pFD, buf + totalBytesRead,spaceLeft);
      }
      //if(bytesRead == 0) {
      //  printf("Read 0\n");
      //  return -1;
      //}
      //Decrease the bytes left to read
      bytesLeft -= bytesRead;
      
      //Increase cursor count 
      fdtArr[bvfs_FD]->cursor += bytesRead;
      
      //Increase the number of bytes read
      totalBytesRead += bytesRead;

    }
    return totalBytesRead;
  }
  else{
    printf("File wasn't opened in read mode\n");
    return -1;
  }
   
}

/*
 * int bv_unlink(const char* fileName);
 *
 * This function is intended to delete a file that has been allocated within
 * the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to delete
 *             from the bvfs file system.
 *
 * Return Value
 *   int:  0 if the delete succeeded.
 *        -1 if some kind of failure occurred (eg. the file does not exist).
 *           Also, print a meaningful error to stderr prior to returning.
 */
int bv_unlink(const char* fileName) {
  iNode *file = NULL;
  //Loop through iNode Array
  for(int i=0; i<256; i++){
    if(!strcmp(iNodeArray[i]->name, fileName)){
      //printf("Found the correct file\n");
      file = iNodeArray[i];
    }
  }

  //if file never got set, we didn't have that filename - so return -1
  if(file == NULL){
    printf("couldn't find that file to delete\n");
    return -1;
  }
  removeDiskMap(file);
  file->numBytes = -1;

  num_files--;
  return 0;
}

/*
 * void bv_ls();
 *
 * This function will list the contests of the single-directory file system.
 * First, you must print out a header that declares how many files live within
 * the file system. See the example below in which we print "2 Files" up top.
 * Then display the following information for each file listed:
 *   1) the file size in bytes
 *   2) the number of blocks occupied within bvfs
 *   3) the time and date of last modification (derived from unix timestamp)
 *   4) the name of the file.
 * An example of such output appears below:
 *    | 2 Files
 *    | bytes:  276, blocks: 1, Tue Nov 14 09:01:32 2017, bvfs.h
 *    | bytes: 1998, blocks: 4, Tue Nov 14 10:32:02 2017, notes.txt
 *
 * Hint: #include <time.h>
 * Hint: time_t now = time(NULL); // gets the current unix timestamp (32 bits)
 * Hint: printf("%s\n", ctime(&now));
 *
 * Input Parameters
 *   None
 *
 * Return Value
 *   void
 */
void bv_ls() {
  printf("beep\n");
  printf("| %d Files\n", num_files);
  //Loop through iNodes and print info about them
  for(int i=0; i<MAX_FILES; i++){
    iNode *curr = iNodeArray[i]; 
    if(curr->numBytes != -1){
      printf("ping\n");
      int numBlocks = curr->numBytes / BLOCK_SIZE;
      if (curr->numBytes % BLOCK_SIZE != 0)
        numBlocks++;
      //printf("| bytes: %d, blocks: %d, %.24s, %c\n", curr->numBytes,  ceil(curr->numBytes/BLOCK_SIZE), curr->time, curr->name);
      printf("| bytes: %d, blocks: %d, %.24s, %s\n", curr->numBytes,  numBlocks, ctime(&(curr->time)), curr->name);
    }
  }
}
