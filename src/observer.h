#ifndef __OBSERVER_H__
#define __OBSERVER_H__

#include <sys/inotify.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct observer* observer_t;

typedef void (*observer_on_notify_f)(observer_t *observer, const char *path, uint32_t mask);

struct observer
{
    /**
     * Watch the specified directory
     * 
     * @param self {@link observer_t}
     *           obsersver_t object
     * @param root
     *           the path to watch
     * @param mask
     *           event mask, see {@code inotify}
     * @param fn
     *           event listener
     * @return -1 on error occurred or 0 on success
     */
    int (*watch)(observer_t *self, const char *path, int mask, observer_on_notify_f fn);

    /**
     * Free this obsever
     * 
     * @param self {@link observer_t}
     *           obsersver_t object
     */
    void (*free)(observer_t *self);
};

/**
 * New an instance of {@link observer_t}
 * 
 * @return an instance of {@link observer_t}
 */
extern observer_t observer_new(int *status);

#ifdef __cplusplus
}
#endif

#endif /* __OBSERVER_H__ */

