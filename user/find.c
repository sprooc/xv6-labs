#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
get_filename_from_path(char *path)
{
  uint32 len = strlen(path);
  uint32 loc = len - 1;
  while (loc > 0 && path[loc - 1] != '/') {
    loc--;
  }
  return path + loc;
}

void 
find(char *path, char *target)
{
  int fd = open(path, 0);
  struct stat st;
  struct dirent de;
  char buf[512], *p;

  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
    case T_FILE:
      if (strcmp(get_filename_from_path(path), target) == 0) {
        printf("%s\n", path);
      }
      break;
    
    case T_DIR:
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) 
          continue;
        if (strcmp(de.name, ".") == 0 ||
            strcmp(de.name, "..") == 0) 
          continue;

        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        find(buf, target);
      }
      break;

    default:
      break;
  }
  close(fd);
  return;
}

int
main(int argc, char *argv[]) 
{
  if (argc < 3) {
    fprintf(2, "find: missing operand\n");
    exit(1);
  }

  char *path = argv[1];
  char *target = argv[2];
  find(path, target);
  exit(0);
}