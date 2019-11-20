#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <fstream>
#include <errno.h>
#include <string.h>
#include "bvfs.h"
using namespace std;



bool fileUnreadable(const char* fileName);
void die(string str, string val="");
void redirectOutput();
string restoreOutput();

ostream* out;
ofstream os;
const char* defaultPartitionName = "tmpTest.bvfs";
char message[512];

void INIT(const char* fileName) {
  unlink(fileName); // Delete file if it already exists for some reason
  *out << "  bv_init(\"" << fileName << "\")" << endl;
  bv_init(fileName);

  if (fileUnreadable(fileName))
    die("Cannot open file after init. Error: ", strerror(errno));
}

void RE_INIT(const char* fileName) {
  *out << "  bv_init(\"" << fileName << "\")" << endl;
  bv_init(fileName);

  if (fileUnreadable(fileName))
    die("Cannot open file after another init. Error: ", strerror(errno));
}

void DESTROY(const char* fileName) {
  *out << "  bv_destroy()" << endl;
  bv_destroy();
  
  if (fileUnreadable(fileName))
    die("Cannot open file after destroy. Error: ", strerror(errno));
}

void WRITE(int fd, void* buf, int numBytes) {
  *out << "  bv_write(fd, buf, " << numBytes << ")" << endl;
  int retVal = bv_write(fd, buf, numBytes);
  if (retVal != numBytes) {
    sprintf(message, "bv_write expected to return %d, received %d", numBytes, retVal);
    die(message);
  }
}

void READ(int fd, void* buf, int numBytes) {
  *out << "  bv_read(fd, buf, " << numBytes << ")" << endl;
  int retVal = bv_read(fd, buf, numBytes);
  if (retVal != numBytes) {
    sprintf(message, "bv_read expected to return %d, received %d", numBytes, retVal);
    die(message);
  }
}

int OPEN(const char* fileName, int mode) {
  string strMode[] = {"BV_RDONLY", "BV_WCONCAT", "BV_WTRUNC"};
  *out << "  bv_open(\"" << fileName << "\", " << strMode[mode] << ")" << endl;
  int fd = bv_open(fileName, mode);
  if (fd <= -1)
    die("bv_open failed to open file and returning", to_string(fd));

  return fd;
}

void CLOSE(int fd) {
  *out << "  bv_close(" << fd << ")" << endl;
  int retVal = bv_close(fd);
  if (retVal != 0)
    die("bv_close should return 0, received: ", to_string(retVal));
}


//////////////////
//              //
//  TEST SUITE  //
//              //
//////////////////

vector<void (*)()> testSuite {
  []() {
    *out << "[Simple partition creation]" << endl;

    INIT(defaultPartitionName);
    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Fail RDONLY open on file that doesn't exist]" << endl;

    INIT(defaultPartitionName);

    *out << "  bv_open(\"DNE.txt\", BV_RDONLY)" << endl;
    redirectOutput();
    int fd = bv_open("DNE.txt", BV_RDONLY);
    string output = restoreOutput();
    if (fd != -1)
      die("bv_open failed to return -1 when RDONLY opening file that doesn't exist. Gave: ", to_string(fd));
    if (output.size() == 0)
      die("bv_open failure did not also print an error message");

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Fail on bv_close of file descriptor that isn't open]" << endl;

    INIT(defaultPartitionName);

    redirectOutput();
    int fd = bv_close(0);
    string output = restoreOutput();
    if (fd != -1)
      die("bv_close failed to return -1 when closinga  file descriptor that isn't open. Received: ", to_string(fd));
    if (output.size() == 0)
      die("bv_close failure did not also print an error message");

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Write int, read int]" << endl;
    int num1 = 1234567890, num2 = 0;

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &num1, sizeof(num1));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &num2, sizeof(num2));
    if (num1 != num2)
      die("data read does not match data written. Expected 1234567890, Received ", to_string(num2));
    CLOSE(fd);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Write int, fail to read 2 ints]" << endl;
    int wrNum1 = 1234567890, rdNum1 = 0, rdNum2 = 0;

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &wrNum1, sizeof(wrNum1));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &rdNum1, sizeof(rdNum1));
    if (wrNum1 != rdNum1)
      die("data read does not match data written. Expected 1234567890, Received ", to_string(rdNum1));

    *out << "  bv_read(fd, buf, " << sizeof(rdNum2) << ")" << endl;
    redirectOutput();
    int retVal = bv_read(fd, &rdNum2, sizeof(rdNum2));
    string output = restoreOutput();
    if (!(retVal == 0 || retVal == -1))
      die("bv_read expected to return -1 or 0, received: ", to_string(retVal));
    if (output.size() == 0)
      die("bv_read failure did not also print an error message");
    CLOSE(fd);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Write int, close/open, concat write int, close/open, verify]" << endl;
    int wrNum1 = 1234567890, wrNum2 = 543212345;
    int rdNum1 = 0, rdNum2 = 0;

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &wrNum1, sizeof(wrNum1));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &wrNum2, sizeof(wrNum2));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &rdNum1, sizeof(rdNum1));
    READ(fd, &rdNum2, sizeof(rdNum2));
    if (wrNum1 != rdNum1 || wrNum2 != rdNum2) {
      sprintf(message, "reads don't match writes [%d vs %d] and [%d vs %d]", wrNum1, rdNum1, wrNum2, rdNum2);
      die(message);
    }
    CLOSE(fd);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Write int, close/open, trunc write int, close/open, verify]" << endl;
    int wrNum1 = 1234567890, wrNum2 = 543212345;
    int rdNum1 = 0;

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &wrNum1, sizeof(wrNum1));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_WTRUNC );
    WRITE(fd, &wrNum2, sizeof(wrNum2));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &rdNum1, sizeof(rdNum1));
    if (wrNum2 != rdNum1) {
      sprintf(message, "read don't match write [%d vs %d]", wrNum2, rdNum1);
      die(message);
    }
    CLOSE(fd);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Fill a file (one write) then read it back]" << endl;
    int SZ = 16384;
    int inData[SZ], outData[SZ];
    for(int i=0; i < SZ; i++) inData[i] = rand();
    bzero(outData, sizeof(outData));

    INIT(defaultPartitionName);
    int fd = OPEN("full-file.data", BV_WCONCAT);
    WRITE(fd, inData, sizeof(inData));
    CLOSE(fd);

    fd = OPEN("full-file.data", BV_RDONLY);
    READ(fd, outData, sizeof(outData));

    for(int i=0; i < SZ; i++) {
      if (inData[i] != outData[i])
        die("data read differs from data written at byte ", to_string(i*4));
    }
    CLOSE(fd);
    
    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Fill a file (many full-block writes) then read it back]" << endl;
    int SZ = 16384;
    int inData[SZ], outData[SZ];
    for(int i=0; i < SZ; i++) inData[i] = rand();
    bzero(outData, sizeof(outData));

    INIT(defaultPartitionName);
    int fd = OPEN("full-file.data", BV_WCONCAT);
    for(int i=0; i < 128; i++)
      WRITE(fd, inData+(i*128), 512);
    CLOSE(fd);

    fd = OPEN("full-file.data", BV_RDONLY);
    for(int i=0; i < 128; i++)
      READ(fd, outData+(i*128), 512);

    for(int i=0; i < SZ; i++) {
      if (inData[i] != outData[i])
        die("data read differs from data written at byte ", to_string(i*4));
    }
    CLOSE(fd);
    
    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Fill a file (many staggered writes) then read it back]" << endl;
    int SZ = 16384;
    int inData[SZ], outData[SZ];
    char *inBytes = (char*)inData, *outBytes = (char*)outData;
    for(int i=0; i < SZ; i++) inData[i] = rand();
    bzero(outData, sizeof(outData));

    INIT(defaultPartitionName);
    int fd = OPEN("full-file.data", BV_WCONCAT);
    WRITE(fd, inBytes, 16); inBytes+=16;
    for(int i=0; i < 5; i++)
      { WRITE(fd, inBytes, 13104); inBytes += 13104; }
    CLOSE(fd);

    fd = OPEN("full-file.data", BV_RDONLY);
    READ(fd, outBytes, 16); outBytes+=16;
    for(int i=0; i < 5; i++)
      { READ(fd, outBytes, 13104); outBytes += 13104; }

    inBytes = (char*)inData; outBytes = (char*)outData;
    for(int i=0; i < 65536; i++) {
      if (inBytes[i] != outBytes[i])
        die("data read differs from data written at byte ", to_string(i));
    }
    CLOSE(fd);
    
    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Check for excessive output]" << endl;
    int num1 = 1234567890, num2 = 0;

    redirectOutput();
    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &num1, sizeof(num1));
    CLOSE(fd);

    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &num2, sizeof(num2));
    if (num1 != num2)
      die("data read does not match data written. Expected 1234567890, Received ", to_string(num2));
    CLOSE(fd);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);

    string output = restoreOutput();
    if (output.size() > 0)
      die("excessive output printed to the screen");
  },


  []() {
    *out << "[Check bv_ls functionality for no files]" << endl;

    INIT(defaultPartitionName);

    redirectOutput();
    bv_ls();
    string output = restoreOutput();

    if (output.size() == 0)
      die("bv_ls is not producing output");
    if (output.find("0 File") == string::npos)
      die("bv_ls is not correctly printing a header identifying the number of files. Execting: \"0 Files\". Received:\n",output);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[Check bv_ls functionality for 1 file]" << endl;
    int SZ = 51257;
    char inData[SZ];
    bzero(inData, sizeof(inData));

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, inData, sizeof(inData));
    CLOSE(fd);

    redirectOutput();
    bv_ls();
    string output = restoreOutput();

    if (output.size() == 0)
      die("bv_ls is not producing output");
    if (output.find("1 File") == string::npos)
      die("bv_ls is not correctly printing a header identifying the number of files. Execting: \"1 File\"");
    if (output.find("somefile.data") == string::npos)
      die("bv_ls is not correctly printing file names");
    if (output.find("51257") == string::npos)
      die("bv_ls reports an incorrect number of bytes in the file. Expecting 12345, received:\n", output);
    if (output.find("101") == string::npos)
      die("bv_ls reports an incorrect number of blocks in the file. Expecting 25, received:\n", output);

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[init, destroy, init, destroy]" << endl;
    INIT(defaultPartitionName);
    DESTROY(defaultPartitionName);

    RE_INIT(defaultPartitionName);
    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[init, write, destroy, init, read, destroy]" << endl;
    int num1 = 1234567890, num2 = 0;

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &num1, sizeof(num1));
    CLOSE(fd);
    DESTROY(defaultPartitionName);


    RE_INIT(defaultPartitionName);
    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &num2, sizeof(num2));
    if (num1 != num2)
      die("data read does not match data written. Expected 1234567890, Received ", to_string(num2));
    CLOSE(fd);
    DESTROY(defaultPartitionName);

    unlink(defaultPartitionName);
  },


  []() {
    *out << "[init, write multiple blocks, destroy, init, read, destroy]" << endl;
    const int SZ = 51256;
    char inBytes[SZ], outBytes[SZ];
    for(int i=0; i < SZ; i++) { inBytes[i] = (char)(rand() % 256); }
    bzero(outBytes, sizeof(outBytes));

    INIT(defaultPartitionName);
    int fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, inBytes, sizeof(inBytes));
    CLOSE(fd);
    DESTROY(defaultPartitionName);


    RE_INIT(defaultPartitionName);
    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, outBytes, sizeof(outBytes));

    for(int i=0; i < SZ; i++) {
      if (inBytes[i] != outBytes[i])
        die("data read does not match data written. Differs at byte ", to_string(i));
    }
    CLOSE(fd);
    DESTROY(defaultPartitionName);

    unlink(defaultPartitionName);
  },


  []() {
    *out << "[write to 2 files, read from 2 files]" << endl;
    int f1Num = 123456789, f2Num = 987654321;
    char *f1Ptr = (char*)(&f1Num);
    char *f2Ptr = (char*)(&f2Num);

    INIT(defaultPartitionName);
    int fd1 = OPEN("file1.data", BV_WCONCAT);
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    int fd2 = OPEN("file2.data", BV_WCONCAT);
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd1, f1Ptr, 1);
    CLOSE(fd1);
    WRITE(fd2, f2Ptr, 1);
    CLOSE(fd2);


    int out1Num, out2Num;
    fd2 = OPEN("file2.data", BV_RDONLY);
    fd1 = OPEN("file1.data", BV_RDONLY);
    READ(fd2, &out2Num, sizeof(out2Num));
    READ(fd1, &out1Num, sizeof(out1Num));

    if (f1Num != out1Num || f2Num != out2Num) {
      sprintf(message, "reads don't match writes [%d vs %d] and [%d vs %d]", f1Num, out1Num, f2Num, out2Num);
      die(message);
    }

    CLOSE(fd2);
    CLOSE(fd1);

    redirectOutput();
    bv_ls();
    string output = restoreOutput();

    if (output.size() == 0)
      die("bv_ls is not producing output");
    if (output.find("2 File") == string::npos)
      die("bv_ls is not correctly identifying the existence of the 2 files. Execting: \"2 Files\"");

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[write to 2 files, read from 2 files with destroy/init inbetween]" << endl;
    int f1Num = 123456789, f2Num = 987654321;
    char *f1Ptr = (char*)(&f1Num);
    char *f2Ptr = (char*)(&f2Num);

    INIT(defaultPartitionName);
    int fd1 = OPEN("file1.data", BV_WCONCAT);
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    int fd2 = OPEN("file2.data", BV_WCONCAT);
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    WRITE(fd1, f1Ptr, 1); f1Ptr++;
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd2, f2Ptr, 1); f2Ptr++;
    WRITE(fd1, f1Ptr, 1);
    CLOSE(fd1);
    WRITE(fd2, f2Ptr, 1);
    CLOSE(fd2);

    DESTROY(defaultPartitionName);
    RE_INIT(defaultPartitionName);

    int out1Num, out2Num;
    fd2 = OPEN("file2.data", BV_RDONLY);
    fd1 = OPEN("file1.data", BV_RDONLY);
    READ(fd2, &out2Num, sizeof(out2Num));
    READ(fd1, &out1Num, sizeof(out1Num));

    if (f1Num != out1Num || f2Num != out2Num) {
      sprintf(message, "reads don't match writes [%d vs %d] and [%d vs %d]", f1Num, out1Num, f2Num, out2Num);
      die(message);
    }

    CLOSE(fd2);
    CLOSE(fd1);

    redirectOutput();
    bv_ls();
    string output = restoreOutput();

    if (output.size() == 0)
      die("bv_ls is not producing output");
    if (output.find("2 File") == string::npos)
      die("bv_ls is not correctly identifying the existence of the 2 files. Execting: \"2 Files\"");

    DESTROY(defaultPartitionName);
    unlink(defaultPartitionName);
  },


  []() {
    *out << "[write to partion1, then partition2, read from p1, then from p2]" << endl;
    int inNum1 = 123456789, inNum2 = 987654321;
    int outNum1 = 0, outNum2 = 0;
    int fd;
    char *partition1Name = "testPartition1.bvfs";
    char *partition2Name = "testPartition2.bvfs";

    INIT(partition1Name);
    fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &inNum1, sizeof(inNum1));
    CLOSE(fd);
    DESTROY(partition1Name);

    INIT(partition2Name);
    fd = OPEN("somefile.data", BV_WCONCAT);
    WRITE(fd, &inNum2, sizeof(inNum2));
    CLOSE(fd);
    DESTROY(partition2Name);


    RE_INIT(partition1Name);
    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &outNum1, sizeof(outNum1));
    if (outNum1 != inNum1)
      die("data read does not match data written. Expected 123456789, Received ", to_string(inNum1));
    CLOSE(fd);
    DESTROY(partition1Name);
    unlink(partition1Name);

    RE_INIT(partition2Name);
    fd = OPEN("somefile.data", BV_RDONLY);
    READ(fd, &outNum2, sizeof(outNum2));
    if (outNum2 != inNum2)
      die("data read does not match data written. Expected 987654321, Received ", to_string(inNum1));
    CLOSE(fd);
    DESTROY(partition2Name);
    unlink(partition2Name);
  },
};

int main(int argc, char** argv) {
  printf("[BVFS Test Suite]\n");

  if (argc > 2) {
    cerr << "Usage: " << argv[0] << " [test #]" << endl; 
    return -1;
  }

  if (argc == 2) {
    int testNum = atoi(argv[1]);
    if (testNum < 0 || testNum >= testSuite.size()) {
      cerr << "Invalid testNum (" << testNum << ") chosen." << endl;
      return -1;
    }

    out = &cout;
    cout << "Test " << testNum << ": "; 
    testSuite[testNum]();
    cout << "\033[32mSUCCESS\033[0m" << endl;
    return 0;
  }

  // Run all tests

  int passed = 0;
  int failed = 0;
  int testNum = 0;
  for(auto test : testSuite) {
    
    int child_pID = fork();
    if (child_pID == 0) {
      // Test Process
    
      os = ofstream(".driverOutput.txt");
      out = &os;
      *out << "\nTest " << testNum << ": "; 
      test();
      exit(0);
    } else {
      // Main Process
      int retCode=1337;
      waitpid(child_pID, &retCode, 0);
      if (retCode) {
        failed++;
        ifstream fin(".driverOutput.txt");
        string line;
        while(getline(fin, line))
          cout << line << endl;
        if (retCode == 139) {
          cout << "\033[35mSegmentation Fault\033[0m" << endl;
        }
      } else {
        passed++;
      }
    }
    testNum++;
  }
  cout << endl;
  cout << "\033[32mPassed\033[0m: " << passed << endl;
  cout << "\033[31mFailed\033[0m: " << failed << endl;



  return 0;
}



bool fileUnreadable(const char* fileName) {
  int fd = open(fileName, O_RDONLY);
  if (fd >= 0)
    close(fd);
  return (fd < 0);
}

void die(string str, string val) {
  *out << "  \033[31mFailed\033[0m: " << str << val << endl;
  unlink(defaultPartitionName);
  exit(1);
}

int __tempSTDOUT;
int __tempSTDERR;
void redirectOutput() {
  __tempSTDOUT = dup(1); // Copy stdout
  __tempSTDERR = dup(2); // Copy stderr
  close(1);  // Close original stdout
  close(2);  // Close original stderr

  unlink(".stdout-stderr.txt");
  open(".stdout-stderr.txt", O_WRONLY | O_CREAT, 0644);
  dup2(1,2); // Open file for writing for stdout & stderr
}

string restoreOutput() {
  close(1);  // Close file stdout
  close(2);  // Close file stderr
  dup2(__tempSTDOUT,1); // Bring original stdout back to 1
  dup2(__tempSTDERR,2); // Bring original stderr back to 2
  close(__tempSTDOUT); // Close temp stdout
  close(__tempSTDERR); // Close temp stderr

  string output = "", line;
  ifstream fin(".stdout-stderr.txt");
  bool first = true;
  while(getline(fin, line)){
    if (first)
      first = false;
    else
      output.append("\n");
    output += line;
  }
  fin.close();
  unlink(".stdout-stderr.txt");
  return output;
}

