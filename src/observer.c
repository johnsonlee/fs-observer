#include "observer.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/types.h>


#define MAX_EVENTS 0xff

#define ISEVENT(events, flag) ((events) & flag)

#define EVENT_SIZE sizeof(struct inotify_event)

#define EVTBUFSIZE ((EVENT_SIZE + FILENAME_MAX) * 1024)



struct _fds
{
    struct _fds *next;
    int fd;
    char *path;
};

struct _observer
{
    struct observer super;

    /**
     * inotify fd
     */
    int ifd;

    /**
     * epoll fd
     */
    int efd;

    /**
     * watch mask
     */
    int mask;

    /**
     * inotify watched fds
     */
    struct _fds *wfds;

    observer_on_notify_f listener;
};

struct _scan_task
{
    struct _observer *self;
    const char *path;
};

/**
 * Watch the specified file, and append the watched file descriptor into observer's wfds
 */
static void observer_add_watch(struct _observer *self, const char *parent, const char *child)
{
    struct _fds *fds = calloc(1, sizeof(struct _fds));
    size_t len = strlen(parent) + strlen(child) + 2;

    self->wfds = fds;
    fds->next = self->wfds;
    fds->path = calloc(sizeof(char), len);
    snprintf(fds->path, len, "%s/%s", parent, child);
    printf("Watching %s\n", fds->path);
    fds->fd = inotify_add_watch(self->ifd, fds->path, self->mask);
}

static void observer_deep_scan(struct _observer *self, const char *path)
{
    char buf[4096];
    struct dirent *entp;
    DIR *dirp = opendir(path);

    while (NULL != (entp = readdir(dirp))) {
        if (DT_DIR != entp->d_type || !strcmp(".", entp->d_name) || !strcmp("..", entp->d_name)) {
            printf("Ignore %s/%s\n", path, entp->d_name);
            continue;
        }

        snprintf(buf, sizeof(buf), "%s/%s", path, entp->d_name);
        observer_add_watch(self, path, entp->d_name);
        observer_deep_scan(self, buf);
    }

    closedir(dirp);
}

static void* observer_scan(void *arg)
{
    struct _scan_task *task = (struct _scan_task*) arg;

    observer_deep_scan(task->self, task->path);

    return NULL;
}

static void observer_read_event(struct _observer *self, int fd)
{
    int off;
    int8_t *tmp;
    size_t nbytes;
    int8_t buf[EVTBUFSIZE];
    struct inotify_event *ie;

    if ((nbytes = read(fd, buf, EVTBUFSIZE)) < 0) {
        return;
    }

    off = 0;
    tmp = buf;

    do {
        ie = (struct inotify_event*) tmp;
        off = EVENT_SIZE + ie->len;
        tmp += off;
        nbytes -= off;

        if (self->listener) {
            self->listener(ie);
        }
    } while (nbytes > 0);
}

static int observer_event_loop(struct _observer *self)
{
    int i;
    int nfds;
    struct epoll_event *evt;
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        nfds = epoll_wait(self->efd, events, MAX_EVENTS, -1);

        for (i = 0; i < nfds; ++i) {
            evt = events + i;

            // couldn't be happen
            if (!ISEVENT(evt->events, EPOLLIN) || self->ifd != evt->data.fd) {
                close(evt->data.fd);
                continue;
            }

            observer_read_event(self, evt->data.fd);
        }
    }
}

static int observer_watch(observer_t *self, const char *path, int mask, observer_on_notify_f fn)
{
    int result;
    pthread_t tid;
    struct epoll_event evt;
    struct _scan_task *task;
    struct _observer *obsvr = (struct _observer*) *self;

    obsvr->mask = mask;
    obsvr->listener = fn;

    if ((obsvr->ifd = inotify_init()) < 0) {
        perror("inotify_init");
        return errno;
    }

    if ((obsvr->efd = epoll_create(MAX_EVENTS)) < 0) {
        perror("epoll_create");
        goto out_close_ifd;
    }

    evt.events = EPOLLIN;
    evt.data.fd = obsvr->ifd;

    if (epoll_ctl(obsvr->efd, EPOLL_CTL_ADD, obsvr->ifd, &evt) < 0) {
        perror("epoll_ctl");
        goto out_close_efd;
    }

    task = calloc(1, sizeof(struct _scan_task));
    task->self = obsvr;
    task->path = strdup(path);

    if (pthread_create(&tid, NULL, observer_scan, (void*) task) < 0) {
        perror("pthread_create");
        goto out_free_task;
    }

    observer_event_loop(obsvr);
    pthread_join(tid, NULL);

    return 0;

out_free_task:
    free(task);
out_close_efd:
	close(obsvr->efd);
out_close_ifd:
	close(obsvr->ifd);
    return errno;
}

static void observer_free(observer_t *self)
{
    struct _fds *next;
    struct _observer *obsvr = (struct _observer*) *self;

    while (obsvr->wfds) {
        next = obsvr->wfds->next;
        inotify_rm_watch(obsvr->ifd, obsvr->wfds->fd);
        free(obsvr->wfds);
        obsvr->wfds = next;
    }

    close(obsvr->efd);
    close(obsvr->ifd);
    free(*self);
}

static const struct observer clazz = {
    .free  = observer_free,
    .watch = observer_watch,
};

observer_t observer_new()
{
    struct _observer *obsvr = malloc(sizeof(struct _observer));

    memset(obsvr, 0, sizeof(*obsvr));
    memcpy(&obsvr->super, &clazz, sizeof(struct observer));

    return &obsvr->super;
}
