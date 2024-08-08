#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define UPPER 35

void
pipeline(int n, int in_fd)
{
  char buf[1];
  char out_buf[1];
  uint8 flag = 0;
  int pid = 0;
  int fds[2] = {0};
  while (read(in_fd, buf, 1)) {
    int num = (int)buf[0];
    if (num == 0) break;
    if (!flag) {
      printf("prime %d\n", num);
      flag = 1;
      continue;
    }
    if (num % n != 0) {    

      if (!pid) {
        pipe(fds);
        pid = fork();
        if (pid == 0) {
          close(fds[1]);
          pipeline(n + 1, fds[0]);
          exit(0);
        }
        close(fds[0]);
      }
      out_buf[0] = buf[0];
      write(fds[1], out_buf, 1);
    }
  }

  int status;
  if (pid) {
    close(fds[1]);
    wait(&status);
  }
  exit(0);
}

int
main()
{
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid == 0) {
    pipeline(2, fds[0]);
    exit(0);
  }
  close(fds[0]);

  for (char i = 2; i <= UPPER; i++) {
    write(fds[1], &i, 1);
  }
  char buf[1] = {0};
  write(fds[1], buf, 1);
  close(fds[1]);
  int status;
  wait(&status);
  exit(0);
}