// Author: backman
// Date:   11/7/2019
// 
// This example program takes a filename, determines if it exists, and then
// either:
//   1) creates the file and writes an int to it
//   or
//   2) reads the existing files and fetches the int within it
//
// This should be helpful in setting up the file that represents your partition


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: %s <partition name to read/write>\n", argv[0]);
    return 1;
  }

  // Get partition name from command line argument
  char *partitionName = argv[1];

  int pFD = open(partitionName, O_CREAT | O_RDWR | O_EXCL, 0644);
  if (pFD < 0) {
    if (errno == EEXIST) {
      // File already exists. Open it and read info (integer) back
      pFD = open(partitionName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);

      int num;
      read(pFD, (void*)&num, sizeof(num));
      printf("Read from file: %d\n", num);

    }
    else {
      // Something bad must have happened... check errno?
    }

  } else {
    // File did not previously exist but it does now. Write data to it
    int num = 12345678;
    write(pFD, (void*)&num, sizeof(num));
    printf("Created File\n");
  }


  return 0;
}

