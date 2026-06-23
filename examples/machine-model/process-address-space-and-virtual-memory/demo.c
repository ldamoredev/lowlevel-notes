// demo.c — una dirección virtual, dos valores independientes entre procesos.
// fork() duplica el proceso: padre e hijo comparten la MISMA dirección virtual
// para shared_global, pero cada uno tiene su propia página física (copy-on-write),
// así que escribir en el hijo no toca al padre. Eso es la memoria virtual.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int shared_global = 100;       // misma dirección virtual en padre e hijo

int main(void) {
    printf("page size: %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("&shared_global = %p (value %d)\n\n",
           (void*)&shared_global, shared_global);
    fflush(stdout);            // flush antes del fork, o el hijo hereda nuestro buffer

    pid_t pid = fork();
    if (pid == 0) {            // hijo: misma dirección, su propia página física (COW)
        shared_global = 999;
        printf("[child  pid=%d] &g = %p  value = %d\n",
               getpid(), (void*)&shared_global, shared_global);
        return 0;
    }
    wait(NULL);                // padre: la escritura del hijo no lo tocó
    printf("[parent pid=%d] &g = %p  value = %d\n",
           getpid(), (void*)&shared_global, shared_global);
    return 0;
}
