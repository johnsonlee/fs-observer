#include "observer.h"

#include <stdio.h>
#include <stdlib.h>

static void _event_listener(struct inotify_event *ie)
{
    printf(" + %s\n", ie->name);
}

/**
 * This daemon process will watch the /home directory and help the webdav
 * server to change the owner of the file which created by webdav server.
 */
int main(int argc, char *argv[])
{
//    switch (fork()) {
//    case -1:
//        perror("Fork child process failed");
//        exit(errno);
//    case 0:
//        umask(0);
//        break;
//    default:
//        exit(0);
//    }
//
//    // creat new session for child process
//    if (setsid() < 0) {
//        perror("Create new session failed");
//        exit(errno);
//    }
//
//    chdir("/");
//    close(STDIN_FILENO);
//    close(STDOUT_FILENO);
//    close(STDERR_FILENO);

    observer_t obsvr = observer_new();
    obsvr->watch(&obsvr, "/home", IN_CREATE | IN_ONLYDIR, &_event_listener);
    obsvr->free(&obsvr);

    return EXIT_SUCCESS;
}

