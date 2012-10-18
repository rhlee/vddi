#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define HEADER_SIZE 0x200
#define HEADER_STRING "<<< Oracle VM VirtualBox Disk Image >>>"

const char usage[] =
  "Usage: to be written\n";


void error(int line, char * file)
{
  printf("[%s:%i] Last set error code is %i: %s\n"
    "Use gdb to catch this SIGTRAP\n",
    file, line, errno, strerror(errno));
  __asm__("int3");
  exit(errno);
}

unsigned long quadToULong(char* quad)
{
  return
    (*quad & 0xff) + 
    ((*(quad + 1) & 0xff) << 010) + 
    ((*(quad + 2) & 0xff) << 020) + 
    ((*(quad + 3) & 0xff) << 030);
}


int
main(int argc, char *argv[])
{
  char opt;
  int infoMode = 0;
  char headerBuffer[HEADER_SIZE];
  int vdi, raw;
  char *input, *output;
  long blockOffset, dataOffset, blockSize;
  long long diskSize, blockCount, seekTarget, i;
  long *map;
  unsigned char *block, *zero;
  int sparse = 0;
  long mapSize;
  int percent = 0, bars;
  long long percentThreshold = -1;
  int j;
  
  if(sizeof(long) != 4)
  {
    printf("Error: long is not 4 bytes long\n");
    exit(1);
  }
  
  while((opt = getopt(argc, argv, "i:s")) != -1)
  {
    switch(opt)
    {
      case 'i':
        if(*optarg == '-')
        {
          printf(usage);
          exit(1);
        }
        infoMode = 1;
        input = optarg;
        break;
      case 's':
        sparse = 1;
        break;
      case '?':
        perror(usage);
        exit(1);
        break;
    }
  }
  
  if((infoMode && (argc != optind)) ||
    (!infoMode && ((argc - optind) != 2)))
  {
    printf(usage);
    exit(1);
  }
  
  if(!infoMode)
  {
    input = argv[optind];
    output = argv[optind + 1];
  }
  
  if((vdi = open(input, O_RDONLY)) == -1)
    error(__LINE__, __FILE__);
  
  if(read(vdi, headerBuffer, HEADER_SIZE) != HEADER_SIZE)
    error(__LINE__, __FILE__);
  
  if(strncmp(headerBuffer, HEADER_STRING, strlen(HEADER_STRING)))
  {
    printf("Could not find header string\n");
    exit(1);
  }
  printf("VDI type: %lu\n", quadToULong(headerBuffer + 0x4c));
  printf("Block offset: %#lx\n",
    blockOffset = quadToULong(headerBuffer + 0x154));
  printf("Data offset: %#lx\n", dataOffset = quadToULong(headerBuffer + 0x158));
  printf("Disk size: %llu\n", diskSize = quadToULong(headerBuffer + 0x170) +
    ((unsigned long long)quadToULong(headerBuffer + 0x174) << 040));
  printf("Block size: %lu\n", blockSize = quadToULong(headerBuffer + 0x178));
  printf("Block Count: %llu\n", blockCount = (diskSize / blockSize));
  
  if(infoMode) exit(0);
  
  if(lseek(vdi, blockOffset, SEEK_SET) != blockOffset)
    error(__LINE__, __FILE__);
  mapSize = blockCount * 4;
  map = malloc(mapSize);
  if(read(vdi, map, mapSize) != mapSize)
    error(__LINE__, __FILE__);
  
  if(lseek(vdi, dataOffset, SEEK_SET) != dataOffset)
    error(__LINE__, __FILE__);
  if((raw = open(output, O_WRONLY | O_CREAT, 0666)) == -1)
    error(__LINE__, __FILE__);

  block = malloc(blockSize);
  zero = malloc(blockSize);
  memset(zero, 0, blockSize);
  for(i = 0; i < blockCount; i++)
  {
    if(map[i] == -1)
    {
      if(sparse)
      {
        if(lseek(raw, blockSize, SEEK_CUR) != ((i + 1) * blockSize))
          error(__LINE__, __FILE__);
      }
      else
      {
        if(write(raw, zero, blockSize) != blockSize)
          error(__LINE__, __FILE__);
      }
    }
    else
    {
      seekTarget = dataOffset + (map[i] * blockSize);
      if(lseek(vdi, seekTarget, SEEK_SET) != seekTarget)
        error(__LINE__, __FILE__);
      if(read(vdi, block, blockSize) != blockSize)
        error(__LINE__, __FILE__);
      if(write(raw, block, blockSize) != blockSize)
        error(__LINE__, __FILE__);
    }
    
    if(i > percentThreshold)
    {
      bars = (percent / 2.0) + 0.5;
      printf("[");
      for(j = 0; j < bars; j++) printf("=");
      for(j = bars; j < 50; j++) printf("-");
      printf("] %i%%\n", percent);
      fflush(stdout);
      percentThreshold = blockCount * (percent + 0.5) / 100;
      percent++;
    }
  }
  
  //free
  
  if(sparse && ftruncate(raw, blockSize * blockCount))
    error(__LINE__, __FILE__);
  
  close(vdi);
  close(raw);
  
  return 0;
}
