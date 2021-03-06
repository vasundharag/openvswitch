 /* Copyright (c) 2008, 2009, 2010 Nicira Networks
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>

#include "lockfile.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "coverage.h"
#include "hash.h"
#include "hmap.h"
#include "timeval.h"
#include "util.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(lockfile)

struct lockfile {
    struct hmap_node hmap_node;
    char *name;
    dev_t device;
    ino_t inode;
    int fd;
};

/* Lock table.
 *
 * We have to do this stupid dance because POSIX says that closing *any* file
 * descriptor for a file on which a process holds a lock drops *all* locks on
 * that file.  That means that we can't afford to open a lockfile more than
 * once. */
static struct hmap lock_table = HMAP_INITIALIZER(&lock_table);

static void lockfile_unhash(struct lockfile *);
static int lockfile_try_lock(const char *name, bool block,
                             struct lockfile **lockfilep);

/* Returns the name of the lockfile that would be created for locking a file
 * named 'file_name'.  The caller is responsible for freeing the returned
 * name, with free(), when it is no longer needed. */
char *
lockfile_name(const char *file_name)
{
    const char *slash = strrchr(file_name, '/');
    return (slash
            ? xasprintf("%.*s/.%s.~lock~",
                        (int) (slash - file_name), file_name, slash + 1)
            : xasprintf(".%s.~lock~", file_name));
}

/* Locks the configuration file against modification by other processes and
 * re-reads it from disk.
 *
 * The 'timeout' specifies the maximum number of milliseconds to wait for the
 * config file to become free.  Use 0 to avoid waiting or INT_MAX to wait
 * forever.
 *
 * Returns 0 on success, otherwise a positive errno value.  On success,
 * '*lockfilep' is set to point to a new "struct lockfile *" that may be
 * unlocked with lockfile_unlock().  On failure, '*lockfilep' is set to
 * NULL. */
int
lockfile_lock(const char *file, int timeout, struct lockfile **lockfilep)
{
    /* Only exclusive ("write") locks are supported.  This is not a problem
     * because the Open vSwitch code that currently uses lock files does so in
     * stylized ways such that any number of readers may access a file while it
     * is being written. */
    long long int warn_elapsed = 1000;
    long long int start, elapsed;
    char *lock_name;
    int error;

    COVERAGE_INC(lockfile_lock);

    lock_name = lockfile_name(file);
    time_refresh();
    start = time_msec();

    do {
        error = lockfile_try_lock(lock_name, timeout > 0, lockfilep);
        time_refresh();
        elapsed = time_msec() - start;
        if (elapsed > warn_elapsed) {
            warn_elapsed *= 2;
            VLOG_WARN("%s: waiting for lock file, %lld ms elapsed",
                      lock_name, elapsed);
        }
    } while (error == EINTR && (timeout == INT_MAX || elapsed < timeout));

    if (!error) {
        if (elapsed) {
            VLOG_WARN("%s: waited %lld ms for lock file",
                      lock_name, elapsed);
        }
    } else if (error == EINTR) {
        COVERAGE_INC(lockfile_timeout);
        VLOG_WARN("%s: giving up on lock file after %lld ms",
                  lock_name, elapsed);
        error = ETIMEDOUT;
    } else {
        COVERAGE_INC(lockfile_error);
        if (error == EACCES) {
            error = EAGAIN;
        }
        VLOG_WARN("%s: failed to lock file "
                  "(after %lld ms, with %d-ms timeout): %s",
                  lock_name, elapsed, timeout, strerror(error));
    }

    free(lock_name);
    return error;
}

/* Unlocks 'lockfile', which must have been created by a call to
 * lockfile_lock(), and frees 'lockfile'. */
void
lockfile_unlock(struct lockfile *lockfile)
{
    if (lockfile) {
        COVERAGE_INC(lockfile_unlock);
        lockfile_unhash(lockfile);
        free(lockfile->name);
        free(lockfile);
    }
}

/* Marks all the currently locked lockfiles as no longer locked.  It makes
 * sense to call this function after fork(), because a child created by fork()
 * does not hold its parents' locks. */
void
lockfile_postfork(void)
{
    struct lockfile *lockfile;

    HMAP_FOR_EACH (lockfile, struct lockfile, hmap_node, &lock_table) {
        if (lockfile->fd >= 0) {
            VLOG_WARN("%s: child does not inherit lock", lockfile->name);
            lockfile_unhash(lockfile);
        }
    }
}

static uint32_t
lockfile_hash(dev_t device, ino_t inode)
{
    return hash_bytes(&device, sizeof device,
                      hash_bytes(&inode, sizeof inode, 0));
}

static struct lockfile *
lockfile_find(dev_t device, ino_t inode)
{
    struct lockfile *lockfile;

    HMAP_FOR_EACH_WITH_HASH (lockfile, struct lockfile, hmap_node,
                             lockfile_hash(device, inode), &lock_table) {
        if (lockfile->device == device && lockfile->inode == inode) {
            return lockfile;
        }
    }
    return NULL;
}

static void
lockfile_unhash(struct lockfile *lockfile)
{
    if (lockfile->fd >= 0) {
        close(lockfile->fd);
        lockfile->fd = -1;
        hmap_remove(&lock_table, &lockfile->hmap_node);
    }
}

static struct lockfile *
lockfile_register(const char *name, dev_t device, ino_t inode, int fd)
{
    struct lockfile *lockfile;

    lockfile = lockfile_find(device, inode);
    if (lockfile) {
        VLOG_ERR("%s: lock file disappeared and reappeared!", name);
        lockfile_unhash(lockfile);
    }

    lockfile = xmalloc(sizeof *lockfile);
    lockfile->name = xstrdup(name);
    lockfile->device = device;
    lockfile->inode = inode;
    lockfile->fd = fd;
    hmap_insert(&lock_table, &lockfile->hmap_node,
                lockfile_hash(device, inode));
    return lockfile;
}

static int
lockfile_try_lock(const char *name, bool block, struct lockfile **lockfilep)
{
    struct flock l;
    struct stat s;
    int error;
    int fd;

    *lockfilep = NULL;

    /* Open the lock file, first creating it if necessary. */
    for (;;) {
        /* Check whether we've already got a lock on that file. */
        if (!stat(name, &s)) {
            if (lockfile_find(s.st_dev, s.st_ino)) {
                return EDEADLK;
            }
        } else if (errno != ENOENT) {
            VLOG_WARN("%s: failed to stat lock file: %s",
                      name, strerror(errno));
            return errno;
        }

        /* Try to open an existing lock file. */
        fd = open(name, O_RDWR);
        if (fd >= 0) {
            break;
        } else if (errno != ENOENT) {
            VLOG_WARN("%s: failed to open lock file: %s",
                      name, strerror(errno));
            return errno;
        }

        /* Try to create a new lock file. */
        VLOG_INFO("%s: lock file does not exist, creating", name);
        fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            break;
        } else if (errno != EEXIST) {
            VLOG_WARN("%s: failed to create lock file: %s",
                      name, strerror(errno));
            return errno;
        }

        /* Someone else created the lock file.  Try again. */
    }

    /* Get the inode and device number for the lock table. */
    if (fstat(fd, &s)) {
        VLOG_ERR("%s: failed to fstat lock file: %s", name, strerror(errno));
        close(fd);
        return errno;
    }

    /* Try to lock the file. */
    memset(&l, 0, sizeof l);
    l.l_type = F_WRLCK;
    l.l_whence = SEEK_SET;
    l.l_start = 0;
    l.l_len = 0;

    time_disable_restart();
    error = fcntl(fd, block ? F_SETLKW : F_SETLK, &l) == -1 ? errno : 0;
    time_enable_restart();

    if (!error) {
        *lockfilep = lockfile_register(name, s.st_dev, s.st_ino, fd);
    } else {
        close(fd);
    }
    return error;
}

