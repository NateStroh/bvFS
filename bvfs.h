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


//TODO: superblock struct?


//TODO: fileDescriptor struct
struct fdTable{
  int cursor;
  int mode;
  int isOpen;

} typedef fdTable;


//TODO: padding?
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
iNode iNodeArray[256];
fdTable fdtArr[256];
int num_files = 0;
int pFD;
//offset(address) of first block dedicated to pointing to free blocks
short SUPERPTR = 257;
short SPNEXT = 258;

// Prototypes
int bv_init(const char *fs_fileName);
int bv_destroy();
int bv_open(const char *fileName, int mode);
int bv_close(int bvfs_FD);
int bv_write(int bvfs_FD, const void *buf, size_t count);
int bv_read(int bvfs_FD, void *buf, size_t count);
int bv_unlink(const char* fileName);
void bv_ls();

void removeDiskMap(iNode i){
  //Loop through blockAddresses contained in the iNode while i< numberOfBlocks
  SUPERPTR = file.blockAddresses[0];
  lseek(pFD, 0, SEEK_SET);
  write(pFD, (void *)&SUPERPTR, sizeof(short));
  for(int i=0; i<file.numBlocks-1; i++;){
    //Seek to one of the file blocks (blockAddresses[i])
    lseek(pFD, (BLOCK_SIZE * blockAddresses[i]), SEEK_CUR);
    //Write blockAddresses[i+1] to the block we seeked to 
    write(pFD, (void *)&blockAddresses[i+1], sizeof(short));
  }

  //seek to the last item in the diskmap and add next items "address" after the last block
  lseek(pFD, (BLOCK_SIZE * blockAddresses[file.numBlocks]), SEEK_CUR);
  write(pFD, (void*)SPNEXT, sizeof(short));
  //set the next address
  SPNEXT = blockAddresses[1];
}

void buildMemStructs(int id){
  //Read super block ptr
  read(id, (void*)&SUPERPTR, sizeof(short));
  //Seek to second block
  seek(id, (BLOCK_SIZE -sizeof(short)), SEEK_CUR);
  short pos = 0;

  //Read iNodes from Disk
  for(int i=0; i<256; i++){
    iNode *newNode = malloc(sizeof(iNode));
    read(id, (void*)&newNode, sizeof(iNode));
    seek(id, (BLOCK_SIZE - sizeof(iNode)), SEEK_CUR);
    iNodeArray[i] = newNode;
  }
}
void getTime(){

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
    write(pFD, (void*)((short) 257), sizeof(short));
    //seek to next block
    lseek(pFD, 510, SEEK_CUR);

    //write inodes
    iNode node;
    for(int i=0; i<256; i++){
      //set iNode name to default of NULL
      node.name = "NULL\0";
      node.pos = i+1;

      //write it to file
      write(pFD, (void*)node, sizeof(iNode));

      //seek to next iNode location
      lseek(pFD, 512 - sizeof(iNode), SEEK_CUR); 
    }

    //write remaining superBlock pointers - 2 bytes pointing to the next super block
    for(int i=257; i<16384; i++){
      //write short of next free block (very next block in this case)
      write(pFD, (void*)(i+1), sizeof(short));
      //seek to the next block
      lseek(pFD, 510, SEEK_CUR);
    }

    //set up all data structures in memory
    buildMemStructs(pFD);

    printf("Created File\n");
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
  iNode node;
  //write iNodes to disk
  for(int i=0; i<256; i++){
    node =  iNodeArray[i];
    write(pFD, (void*)node, sizeof(iNode));
    lseek(pFD, 512 - sizeof(iNode), SEEK_CUR);
  }
  //free anything?
  for(int i=0; i<256; i++){
    free(iNodeArray[i]);
  }
  //TODO: free fdTABLE

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
  if(strlen(filename) >= 31){
    printf("Filename to long\n");
    return -1
  }
  if(mode > 2 || mode < 0){
    printf("Invalid  Mode\n");
    return -1;
  }

  //check if we need to create the file or not
  iNode file = NULL;
  fdTable fdt = NULL;
  for(int i=0; i<256; i++){
    if(!strcmp(iNodeArray[i]->name, filename)){
      //found a file with that name
      file = iNodeArray[i];
      fdt = fdTable[i];
      //check if the file is already open 
      if(fdt.isOpen == 1){
        printf("File is already open\n");
        return -1;
      }
      //set up file descriptor
      fdt.isOpen = 1;
      fdt.mode = mode;
            
      if(mode == BV_WCONCAT){
        ftd.cursor = iNodeArray[i].numBytes; 
      }
      else if(mode == BV_WTRUNC){
        removeDiskMap(iNodeArray[i]);
        fdt.cursor = 0;
      }
      else{
        fdt.cursor = 0;
      }
      //add it to the array
      fdTable[i] = fdt;
      //increment file count
      num_files ++;
      return 0; 
    }
  }
  if(file == NULL){
    if(num_files < 256){
      //file doesn't exist so make it
      iNode *node = malloc(sizeof(iNode));
      node->name = filename;
      node->time = time(NULL);
      for(int j=0; j<256; j++){
        if(strcmp(iNodeArray[j]->name, "NULL") == 0){
          iNodeArray[j] = node;
          //set up file descriptor
          fdt.isOpen = 1;
          fdt.mode = mode;
          fdt.cursor = 0;
          //add it to the array
          fdTable[j] = fdt;
          num_files ++;
          return 0;
        }
      }
    }
    else{
      printf("Too many files already exist - hit maximum\n");
      return -1;
    }
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
  if(!strcmp(iNodeArr[bvfs_ID]->name, NULL){
    printf("File doesn't exist");
  }else{
    
    //free up blocks used - add them back into the superblock
    //Adds newly freed blocks to disk
    removeDiskMap(iNodeArr[bvfs_ID]);

    //Reset the iNode
    iNodeArr[bvfs_ID]->name = NULL;
    iNodeArr[bvfs_ID]->numBytes = 0;
    iNodeArr[bvfs_ID]->numBlocks = 0;
    iNodeArr[bvfs_ID]->time = NULL;
    iNodeArr[bvfs_ID]->blockAdresses = NULL;
    
    //Reset the file descriptor
    fdTable[bvfs_ID]->mode = -1;
    fdTable[bvfs_ID]->cursor = 0;
    fdTable[bvfs_ID]->open = 0;
    
    //decrement file count
    num_files --;
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

  //take blocks out of superblock


}

/*
 * int bv_read(int bvfs_FD, void *buf, size_t count);
 *
 * This function will read count bytes from the location corresponding to the
 * cursor of the file (represented by bvfs_FD) to buf.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to read from.
 *   buf: The buffer that we will write the data to.
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

  //

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
  for(int i=0; i<256; i++;){
    if(!strcmp(iNodeArray[i]->name, fileName)){
      printf("Found the correct file\n");
      file = iNodeArray[i];
    }
  }

  //if file never got set, we didn't have that filename - so return -1
  if(file == NULL){
    return -1;
  }
  removeDiskMap(iNode file);
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
  printf("| %d Files\n", num_files);
  //Loop through iNodes and print info about them
  for(int i=0; i<MAX_FILES; i++){
    iNode curr = iNode[i]; 
    if(!strcmp(curr.name, "NULL")){
      printf("| bytes: %d, blocks: %d, %.24s, %c\n", curr.numBytes,  ceil(curr.numBytes/BLOCK_SIZE), curr.time, curr.name);
    }
  }
}
