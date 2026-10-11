#include "core/storage.h"
#include "core/paths.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* These names are used only by migration and older released payloads. */
#ifndef LIBREPAD_LEGACY_STATE_DIR
#define LIBREPAD_LEGACY_STATE_DIR "/data/pdp-pad"
#endif

static int storage_lock(const char *directory)
{
    char path[PATH_MAX];
    if (snprintf(path,sizeof(path),"%s/instance.lock",directory)>=(int)sizeof(path)) {
        errno=ENAMETOOLONG; return -1;
    }
    int fd=open(path,O_RDWR|O_CREAT,0600);
    if (fd>=0 && flock(fd,LOCK_EX|LOCK_NB)) {
        int error=errno; close(fd); errno=error; return -1;
    }
    return fd;
}

/* Rename before creating the old-name alias. A failed alias creation can be
 * retried on the next startup; it never overwrites an existing file. */
static int storage_file_alias(const char *old_name, const char *new_name)
{
    char old_path[PATH_MAX], new_path[PATH_MAX], target[PATH_MAX];
    if (snprintf(old_path,sizeof(old_path),"%s/%s",LIBREPAD_STATE_DIR,old_name)>=(int)sizeof(old_path) ||
        snprintf(new_path,sizeof(new_path),"%s/%s",LIBREPAD_STATE_DIR,new_name)>=(int)sizeof(new_path)) {
        errno=ENAMETOOLONG; return -1;
    }
    struct stat info;
    if (!lstat(old_path,&info)) {
        if (S_ISLNK(info.st_mode)) {
            ssize_t length=readlink(old_path,target,sizeof(target)-1);
            if (length<0) return -1;
            target[length]=0;
            if (!strcmp(target,new_name)) return 0;
            errno=EINVAL; return -1;
        }
        if (!S_ISREG(info.st_mode)) { errno=EINVAL; return -1; }
        if (!lstat(new_path,&info)) { errno=EEXIST; return -1; }
        if (errno!=ENOENT || rename(old_path,new_path)) return -1;
    } else if (errno!=ENOENT) return -1;
    return symlink(new_name,old_path);
}

int librepad_storage_open(void)
{
    struct stat current, legacy;
    int have_current=lstat(LIBREPAD_STATE_DIR,&current)==0;
    if (!have_current && errno!=ENOENT) return -1;
    int have_legacy=lstat(LIBREPAD_LEGACY_STATE_DIR,&legacy)==0;
    if (!have_legacy && errno!=ENOENT) return -1;
    if (have_current && !S_ISDIR(current.st_mode)) { errno=ENOTDIR; return -1; }
    if (have_legacy && !S_ISDIR(legacy.st_mode) && !S_ISLNK(legacy.st_mode)) {
        errno=ENOTDIR; return -1;
    }
    /* Two real directories may contain different reservations. Never guess
     * which one to keep or merge them silently. */
    if (have_current && have_legacy && S_ISDIR(legacy.st_mode)) { errno=EEXIST; return -1; }
    if (have_legacy && S_ISLNK(legacy.st_mode)) {
        struct stat target;
        if (!have_current || stat(LIBREPAD_LEGACY_STATE_DIR,&target)) return -1;
        if (target.st_dev!=current.st_dev || target.st_ino!=current.st_ino) {
            errno=EINVAL; return -1;
        }
    }

    int fd;
    if (!have_current && have_legacy) {
        /* Refuse to move storage while an older payload holds its lock. */
        fd=storage_lock(LIBREPAD_LEGACY_STATE_DIR);
        if (fd<0) return -1;
        if (rename(LIBREPAD_LEGACY_STATE_DIR,LIBREPAD_STATE_DIR)) goto failed;
        have_legacy=0;
    } else {
        if (!have_current && mkdir(LIBREPAD_STATE_DIR,0755) && errno!=EEXIST) return -1;
        fd=storage_lock(LIBREPAD_STATE_DIR);
        if (fd<0) return -1;
    }
    if (!have_legacy) {
        char absolute[PATH_MAX];
        if (!realpath(LIBREPAD_STATE_DIR,absolute) || symlink(absolute,LIBREPAD_LEGACY_STATE_DIR)) goto failed;
    }
    if (storage_file_alias("pdp-user","controller-user") ||
        storage_file_alias("pdp-pad.log","LibrePadPS5.log")) goto failed;
    return fd;
failed: {
    int error=errno; close(fd); errno=error; return -1;
}
}
