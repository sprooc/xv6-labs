#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[128];
  char *p = buf;

  int argv_n = 0; 
  char *exec_argv[32];  

  for (int i = 1; i < argc; i++) {
    exec_argv[argv_n++] = argv[i];
  }
  
  int pre_n = argv_n;

  while (1) {
    int n = read(0, p, 1);
    if (!n) break;
    if (*p == '\n') {
      *p = 0;
      int pid = fork();
      if (!pid) {
        exec_argv[argv_n++] = buf;
        char *tp = buf;
        while (*tp) {
          if (*tp == ' ') {
            *tp = 0;
            exec_argv[argv_n++] = tp + 1;
          }
          tp++;
        }
        exec_argv[argv_n] = 0;
        exec(exec_argv[0], exec_argv);
      }
      int status;
      wait(&status);

      p = buf;
      argv_n = pre_n;
    } else {
      p++;
    }
  }
  exit(0);
}