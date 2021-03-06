#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>


#define HEADER_SIZE 0x200
#define HEADER_STRING "<<< Oracle VM VirtualBox Disk Image >>>"
#define TIME_BUFFER_SZ 10
#define BAR_SZ 40

const char usage[] =
  "Usage:\n"
  "  vddi [-i/-s] innputFile [outputFile]\n"
  "    -i only print info don't copy\n"
  "    -s write sparse data (don't use this for physical devices)";


int vdi, raw;
long *map;
unsigned char *block, *zero;


void error(int line, char * file)
{
  fprintf(stderr, "[%s:%i] Last set error code is %i: %s\n"
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

long long now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000000) + tv.tv_usec;
}

void finally()
{
  free(zero);
  free(block);
  free(map);
  close(vdi);
  close(raw);
}

void sigInt(int signal)
{
  fprintf(stderr, "\x1b[?25h\nAborted");
  finally();
  exit(2);
}


int
main(int argc, char *argv[])
{
  char opt;
  int infoMode = 0;
  char headerBuffer[HEADER_SIZE];
  char *input, *output;
  long blockOffset, dataOffset, blockSize;
  long long diskSize, blockCount, seekTarget, i, back;
  int sparse = 0;
  long mapSize;
  long long time_buffer[TIME_BUFFER_SZ], deltaT, lastPrint = now();
  float speed;
  int bars, j;
  int timeRemaining;
  long skip = 0;
  long long firstBlock = 0;
  long offsetInFirstBlock, firstBlockSize;
  
  signal(SIGINT, sigInt);
  
  if(sizeof(long) != 4)
  {
    fprintf(stderr, "Error: long is not 4 bytes long\n");
    exit(1);
  }
  
  while((opt = getopt(argc, argv, "i:sp:")) != -1)
  {
    switch(opt)
    {
      case 'i':
        if(*optarg == '-')
        {
          fprintf(stderr, usage);
          exit(1);
        }
        infoMode = 1;
        input = optarg;
        break;
      case 's':
        sparse = 1;
        break;
      case 'p':
        skip = atoi(optarg);
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
    fprintf(stderr, usage);
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
    fprintf(stderr, "Could not find header string\n");
    exit(1);
  }
  fprintf(stderr, "VDI type: %lu\n", quadToULong(headerBuffer + 0x4c));
  fprintf(stderr, "Block offset: %#lx\n",
    blockOffset = quadToULong(headerBuffer + 0x154));
  fprintf(stderr, "Data offset: %#lx\n", dataOffset = quadToULong(headerBuffer + 0x158));
  fprintf(stderr, "Disk size: %llu\n", diskSize = quadToULong(headerBuffer + 0x170) +
    ((unsigned long long)quadToULong(headerBuffer + 0x174) << 040));
  fprintf(stderr, "Block size: %lu\n", blockSize = quadToULong(headerBuffer + 0x178));
  fprintf(stderr, "Block Count: %llu\n\n\x1b""7", blockCount = (diskSize / blockSize));
  
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
  time_buffer[0] = now();
  
  firstBlock = skip / blockSize;
  offsetInFirstBlock = skip % blockSize;
  firstBlockSize = blockSize - offsetInFirstBlock;
  seekTarget = dataOffset + (map[firstBlock] * blockSize) + offsetInFirstBlock;
  if(lseek(vdi, seekTarget, SEEK_SET) != seekTarget)
    error(__LINE__, __FILE__);
  if(read(vdi, block, firstBlockSize) != firstBlockSize)
    error(__LINE__, __FILE__);
  if(write(raw, block, firstBlockSize) != firstBlockSize)
    error(__LINE__, __FILE__);
  
  for(i = 0; i < (blockCount - (firstBlock + 1)); i++)
  {
    if(map[i] == -1)
    {
      if(sparse)
      {
        if(lseek(raw, blockSize, SEEK_CUR) != (((i + 1) * blockSize) + firstBlockSize))
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
    
    back = i - TIME_BUFFER_SZ + 1;
    back = (0 > back) ? 0 : back;
    if((now() - lastPrint) > 250000)
    {
      bars = (i / (float)blockCount * BAR_SZ) + 0.5;
      fprintf(stderr, "\x1b""8[");
      for(j = 0; j < bars; j++) fprintf(stderr, "=");
      for(j = bars; j < BAR_SZ; j++) fprintf(stderr, "-");
      fprintf(stderr, "] %.1f%%, ", i / (float)blockCount * 100);

      if((deltaT = (now() - time_buffer[back % TIME_BUFFER_SZ])) == 0)
      {
        fprintf(stderr, "v.fast xfer rate");
      }
      else
      {
        speed = TIME_BUFFER_SZ / (deltaT / 1000000.0);
        timeRemaining = ((blockCount - i) / speed) + 0.5;
        fprintf(stderr, "%.2fMB/s, eta %02d:%02d:%02d ",
          blockSize * speed / (float)0x100000,
          timeRemaining / 3600, timeRemaining / 60, timeRemaining % 60);
      }
      fflush(stdout);
      lastPrint = now();
    }
    time_buffer[(i) % TIME_BUFFER_SZ] = now();
  }
  
  if(sparse && ftruncate(raw, blockSize * blockCount))
    error(__LINE__, __FILE__);
  
  finally();
  
  return 0;
}
