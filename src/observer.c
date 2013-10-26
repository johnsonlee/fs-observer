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


struct watch
{
    struct watch *prev;
    struct watch *next;

    int fd;
    char *path;
};

struct _observer
{
    struct observer super;

    /**
     * Running status, read only
     */
    int const *status;

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
    struct watch *watches;

    observer_on_notify_f listener;
};

struct _scan_task
{
    struct _observer *self;
    const char *path;
};

static const char* observer_get_path(struct _observer *self, int wd)
{
    struct watch *w;

    for (w = self->watches; w; w = w->next) {
        if (w->fd == wd)
            return w->path;
    }

    return NULL;
}

/**
 * Watch the specified file, and append the watched file descriptor into observer's watches
 */
static int observer_add_watch(struct _observer *self, const char *path)
{
    struct watch *w = calloc(1, sizeof(struct watch));

    w->prev = NULL;
    w->next = self->watches;
    w->path = strdup(path);
    printf("\033[32mWatching %s\033[m\n", w->path);
    w->fd = inotify_add_watch(self->ifd, w->path, self->mask);

    if (self->watches) {
        self->watches->prev = w;
    }

    return (self->watches = w)->fd;
}

/**
 * Remove the specified path from inotify watch
 */
static int observer_remove_watch(struct _observer *self, const char *path)
{
    int result;
    struct watch *w;

    for (w = self->watches; w; w = w->next) {
        if (!strcmp(w->path, path)) {
            if (w->prev) {
                w->prev->next = w->next;
            }

            if (w->next) {
                w->next->prev = w->prev;
            }

            printf("\033[32mRemove Watch %s\033[m\n", w->path);
            result = inotify_rm_watch(self->ifd, w->fd);
            free(w->path);
            free(w);

            return result;
        }
    }

    return -1;
}

/**
 * Scan the specified path recursively
 */
static void observer_deep_scan(struct _observer *self, const char *path)
{
    char fullpath[PATH_MAX];
    struct dirent *entp;
    DIR *dirp = opendir(path);

    if (NULL == dirp) {
        perror("opendir");
        return;
    }

    while (NULL != (entp = readdir(dirp))) {
        if (DT_DIR != entp->d_type || !strcmp(".", entp->d_name) || !strcmp("..", entp->d_name)) {
            // printf("\033[33mIgnore %s/%s\033[m\n", path, entp->d_name);
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entp->d_name);
        observer_add_watch(self, fullpath);
        observer_deep_scan(self, fullpath);
    }

    closedir(dirp);
}

/**
 * Scan the root path that specified by {@link observer_watch},
 * this function runs with a thread
 */
static void* observer_scan(void *arg)
{
    struct _scan_task *task = (struct _scan_task*) arg;

    observer_deep_scan(task->self, task->path);

    return task;
}

static void observer_process_event(struct _observer *self, struct inotify_event *ie)
{
    char fullpath[PATH_MAX];
    const char *path = observer_get_path(self, ie->wd);

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ie->name);

    switch (ie->mask & (IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED)) {
    case IN_ACCESS:
        break;
    case IN_CREATE:
        if (ie->mask & IN_ISDIR) {
            observer_add_watch(self, fullpath);
            observer_deep_scan(self, fullpath);
        }
        break;
    case IN_DELETE:
        if (ie->mask & IN_ISDIR) {
            observer_remove_watch(self, fullpath);
        }
        break;
    case IN_DELETE_SELF:
        break;
    case IN_MODIFY:
        break;
    case IN_ATTRIB:
        break;
    case IN_OPEN:
        break;
    case IN_CLOSE_WRITE:
        break;
    case IN_CLOSE_NOWRITE:
        break;
    default:
        break;
    }

    if (self->listener) {
        observer_t obsvr = &self->super;
        self->listener(&obsvr, fullpath, ie->mask);
    }
}

/**
 * Read inotify events
 */
static int observer_read_event(struct _observer *self, int fd)
{
    int off;
    int nevt;
    int8_t *tmp;
    size_t nbytes;
    int8_t buf[EVTBUFSIZE];
    struct inotify_event *ie;

    if ((nbytes = read(fd, buf, EVTBUFSIZE)) < 0) {
        perror("Read event buffer error");
        return 0;
    }

    off = 0;
    nevt = 0;
    tmp = buf;

    for (nevt = 0; nbytes > 0; nevt++) {
        ie = (struct inotify_event*) tmp;
        off = EVENT_SIZE + ie->len;
        tmp += off;
        nbytes -= off;
        observer_process_event(self, ie);
        nevt++;
    }

    printf("The Number of Event: %d\n", nevt);

    return nevt;
}

/**
 * Wait for events
 */
static int observer_loop_event(struct _observer *self)
{
    int i;
    int nfds;
    struct epoll_event *evt;
    struct epoll_event events[MAX_EVENTS];

    while (self && *(self->status)) {
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

    printf("\033[35mObserver exit\033[m\n");

    return 0;
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

    result = observer_loop_event(obsvr);
    pthread_join(tid, NULL);

    return result;

out_free_task:
    free(task);
out_close_efd:
	close(obsvr->efd);
    obsvr->efd = -1;
out_close_ifd:
	close(obsvr->ifd);
    obsvr->efd = -1;
    return errno;
}

static void observer_free(observer_t *self)
{
    struct watch *next;
    struct _observer *obsvr = (struct _observer*) *self;

    while (obsvr->watches) {
        next = obsvr->watches->next;

        if (obsvr->ifd > 0) {
            inotify_rm_watch(obsvr->ifd, obsvr->watches->fd);
        }

        free(obsvr->watches);
        obsvr->watches = next;
    }

    if (obsvr->efd > 0) {
        close(obsvr->efd);
    }

    if (obsvr->ifd > 0) {
        close(obsvr->ifd);
    }

    free(*self);
}

static const struct observer clazz = {
    .free  = observer_free,
    .watch = observer_watch,
};

observer_t observer_new(int *status)
{
    struct _observer *obsvr = malloc(sizeof(struct _observer));

    memset(obsvr, 0, sizeof(*obsvr));
    memcpy(&obsvr->super, &clazz, sizeof(struct observer));
    obsvr->status = status;

    return &obsvr->super;
}
