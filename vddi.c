#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


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
  int optCount = 0;
  char opt;
  int infoMode = 0;
  unsigned char headerBuffer[HEADER_SIZE];
  FILE *vdi, *raw;
  int vdiFd, rawFd;
  char *input, *output;
  long blockOffset, dataOffset, blockSize;
  long long diskSize, blockCount;
  long *map;
  long i;
  unsigned char *block;
  int seek = 0;
  
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
        seek = 1;
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
  
  if((vdi = fopen(input, "r")) == NULL)
    error(__LINE__, __FILE__);
  vdiFd = fileno(vdi);
  
  if(read(vdiFd, headerBuffer, HEADER_SIZE) != HEADER_SIZE)
    error(__LINE__, __FILE__);
  
  if(strncmp(headerBuffer, HEADER_STRING, strlen(HEADER_STRING)))
  {
    printf("Could not find header string\n");
    exit(1);
  }
  printf("VDI type: %lu\n", quadToULong(headerBuffer + 0x4c));
  printf("Block offset: 0x%x\n", blockOffset = quadToULong(headerBuffer + 0x154));
  printf("Data offset: 0x%x\n", dataOffset = quadToULong(headerBuffer + 0x158));
  printf("Disk size: %llu\n", diskSize = quadToULong(headerBuffer + 0x170) +
    ((unsigned long long)quadToULong(headerBuffer + 0x174) << 040));
  printf("Block size: %lu\n", blockSize = quadToULong(headerBuffer + 0x178));
  printf("Block Count: %llu\n", blockCount = (diskSize / blockSize));
  
  if(infoMode) exit(0);
  
  if(lseek(vdiFd, blockOffset, SEEK_SET) != blockOffset)
    error(__LINE__, __FILE__);
  //printf("pos: 0x%x\n", ftell(vdi));
  map = malloc(blockCount * 4);
  if(fread(map, 4, blockCount, vdi) != blockCount)
    error(__LINE__, __FILE__);
  
  if(lseek(vdiFd, dataOffset, SEEK_SET) != dataOffset)
    error(__LINE__, __FILE__);
  printf("pos: 0x%x\n", ftell(vdi));
  if((raw = fopen(output, "w")) == NULL)
    error(__LINE__, __FILE__);
  rawFd = fileno(raw);

  if(setvbuf(vdi, NULL, _IOFBF, blockSize) ||
    setvbuf(raw, NULL, _IOFBF, blockSize))
    error(__LINE__, __FILE__);
  block = malloc(blockSize);
  
  for(i = 0; i < blockCount; i++)
  {
    fread(block, blockSize, 1, vdi);
    fwrite(block, blockSize, 1, raw);
    if(i == 0) break;
  }
  
  close(vdiFd);
  close(rawFd);
  
  return 0;
}
