#include "observer.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>


#define MASK (IN_CREATE | IN_DELETE | IN_ONLYDIR)


static int running = 1;

/**
 * inotify event listener
 */
static void _on_notify(observer_t *obsvr, const char *path, uint32_t mask)
{
    if (!(mask & IN_CREATE))
        return;

    char buf[PATH_MAX];
    char *home;
    struct stat sb;

    strcpy(buf, path);
    home = strchr(buf + 6, '/');
    *home = '\0';

    printf("HOME: %s\n", buf);
    if (stat(buf, &sb)) {
        perror("stat");
        return;
    }

    if (chown(path, sb.st_uid, sb.st_gid)) {
        perror("chown");
        return;
    }
}

/**
 * Handle Ctrl-C
 */
static void _signal_handler(int signum)
{
    running = 0;
}

/**
 * This daemon process will watch the /home directory and help the webdav
 * server to change the owner of the file which created by webdav server.
 */
int main(int argc, char *argv[])
{
    if (SIG_IGN == signal(SIGINT, _signal_handler)) {
        signal(SIGINT, SIG_IGN);
    }

#ifdef DAEMONLIZE
    switch (fork()) {
    case -1:
        perror("Fork child process failed");
        exit(errno);
    case 0:
        umask(0);
        break;
    default:
        exit(0);
    }

    // creat new session for child process
    if (setsid() < 0) {
        perror("Create new session failed");
        exit(errno);
    }

    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif

    observer_t obsvr = observer_new(&running);
    obsvr->watch(&obsvr, "/home", MASK, _on_notify);
    obsvr->free(&obsvr);

    return EXIT_SUCCESS;
}

