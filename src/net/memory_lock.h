#ifndef MEMORY_LOCK_H
#define MEMORY_LOCK_H


#include <errno.h>
#include <sys/mman.h>

static int
lock_process_memory(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        perror("mlockall");
        return -1;
    }
    return 0;
}

#endif  // MEMORY_LOCK_H
