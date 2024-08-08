#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main()
{
  int fds[2];
  pipe(fds);

  int pid = fork();

  if (pid == 0) {
    // child process
    printf("%d: received ping\n", getpid());
    write(fds[1], "p", 1);
    exit(0);
  } 
  
  // parent process
  char buf[1];
  read(fds[0], buf, 1);
  printf("%d: received pong\n", getpid());
  exit(0);

}