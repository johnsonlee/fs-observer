#ifndef __OBSERVER_H__
#define __OBSERVER_H__

#include <sys/inotify.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*observer_on_notify_f)(struct inotify_event *ie);

typedef struct observer* observer_t;
struct observer
{
    /**
     * Watch the specified directory
     */
    int (*watch)(observer_t *self, const char *root, int mask, observer_on_notify_f fn);

    /**
     * Free this obsever
     */
    void (*free)(observer_t *self);
};

extern observer_t observer_new();

#ifdef __cplusplus
extern "C" {
#endif

#endif /* __OBSERVER_H__ */

