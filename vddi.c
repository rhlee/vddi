#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char usage[] =
  "Usage: to be written\n";

int
main(int argc, char *argv[])
{
  int optCount = 0;
  char opt;
  int infoMode = 0;
  
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
        break;
      case 's':
        break;
      case '?':
        perror(usage);
        exit(1);
        break;
    }
  }
  printf("%d args left\n", argc - optind);
  
  if((infoMode && (argc != optind)) ||
    (!infoMode && ((argc - optind) != 2)))
  {
    printf(usage);
    exit(1);
  }
  return 0;
}
