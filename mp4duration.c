#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* bytes movie header box = long unsigned offset + long ASCII text string 'mvhd'
->1 byte version = 8-bit unsigned value
  ->if version is 1 then data and duration values are 8 bytes in length
->3 bytes flags = 24 bit hex flags (current = 0)
->4 bytes created mac UTC date
  = long unsigned value in seconds since beginning 1904 to 2040
->4 bytes modified mac UTC date
  = long unsigned value in seconds since beginning 1904 to 2040
OR
-> 8 bytes created mac UTC date
  = 64 bit unsigned value in seconds since beginning 1904
-> 8 bytes modified mac UTC date
  = 64 bit unsigned value in seconds since beginning 1904

-> 4 bytes time scale = long unsigned time unit per second (default = 600)

-> 4 bytes duration = long unsigned time length (in time units)
OR
-> 8 bytes duration = 64 bit unsigned time length (in time units) */
unsigned long info_flip(unsigned long val) {
  unsigned long new = 0;
  new += (val & 0x000000FF) << 24;
  new += (val & 0xFF000000) >> 24;
  new += (val & 0x0000FF00) << 8;
  new += (val & 0x00FF0000) >> 8;

  return new;
}

unsigned long mp4_duration(const char* filename)
{
  int fd;
  struct stat fileinfo;
  void* p;                      // p was used to point to the beginning of the whole mp4 mmap file
  char* result = NULL;                 // result point to the beginning of the mvhd
  
  unsigned long create_date = 0;
  unsigned long modify_date = 0;
  unsigned long timescale = 0;
  unsigned long timeunits = 0;
  int bytesize = 4;             // default bytesize = 4 if version != 1, didn't consider what if bytesize is 8

  if ((fd = open(filename, O_RDONLY)) < 0) {
    perror("Open Error: ");
    exit(1);
  }

  if (fstat(fd, &fileinfo) < 0) {
    perror("Get File Meta Info: ");
    exit(2);
  }
  
  if ((p = mmap(NULL, fileinfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == (void *) -1) {
    perror("mmap: ");
    exit(3);
  }

  if ((result = (char *) memmem(p, fileinfo.st_size, "mvhd", 4)) == NULL) {
    perror("No mvhd found: ");
    exit(4);
  }
  
  result += 4;          // result point to the 1 byte version byte
  if (*result == 1)
    bytesize = 8;
  result += 4;           // Skip 3 bytes flags, now result point to the created date(8 bytes or 4 bytes)

  memcpy(&create_date, result, bytesize);   // We Get the created date of the video
  result += bytesize;
  memcpy(&modify_date, result, bytesize);
  result += bytesize;    // result now point to the time scale(4 bytes);

  memcpy(&timescale, result, 4);
  result += 4;
  memcpy(&timeunits, result, bytesize);

  timescale = info_flip(timescale);
  timeunits = info_flip(timeunits);
  
  munmap(p, fileinfo.st_size);
  return timeunits / timescale;
}


int main(int argc, char* argv[])
{
  if (argc < 2) {
    printf("Usage: mp4duration .mp4\n");
    exit(0);
  }

  unsigned long seconds = mp4_duration(argv[1]);
  printf("The duration is %d minutes %d seconds\n", seconds / 60, seconds % 60);
}
