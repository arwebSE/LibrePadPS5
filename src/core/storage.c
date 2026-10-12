#include "core/storage.h"
#include "core/paths.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* Older payloads still lock this directory. Keep their rollback settings. */
#ifndef LIBREPAD_LEGACY_STATE_DIR
#define LIBREPAD_LEGACY_STATE_DIR "/data/pdp-pad"
#endif
#define STORAGE_JOURNAL LIBREPAD_STATE_DIR "/.storage-migration"
#define STORAGE_TEMP LIBREPAD_STATE_DIR "/.storage-copy.tmp"

static char g_storage_stage[192];
static int g_current_lock=-1, g_legacy_lock=-1;

static void storage_stage(const char *operation, const char *path)
{
    snprintf(g_storage_stage,sizeof(g_storage_stage),"%s: %s",operation,path);
}

const char *librepad_storage_error_stage(void) { return g_storage_stage; }

void librepad_storage_close(int fd)
{
    if (fd==g_current_lock) {
        if (g_legacy_lock>=0) close(g_legacy_lock);
        g_current_lock=g_legacy_lock=-1;
    }
    if (fd>=0) close(fd);
}

static int storage_lock(const char *directory)
{
    char path[PATH_MAX];
    if (snprintf(path,sizeof(path),"%s/instance.lock",directory)>=(int)sizeof(path)) {
        errno=ENAMETOOLONG; return -1;
    }
    storage_stage("open instance lock",path);
    int fd=open(path,O_RDWR|O_CREAT,0600);
    if (fd>=0) {
        storage_stage("acquire instance lock",path);
        if (flock(fd,LOCK_EX|LOCK_NB)) {
            int error=errno; close(fd); errno=error; return -1;
        }
    }
    return fd;
}

static int storage_internal(const char *name)
{
    return !strcmp(name,".") || !strcmp(name,"..") || !strcmp(name,"instance.lock") ||
           !strcmp(name,"stop") || !strcmp(name,".storage-migration") ||
           !strcmp(name,".storage-copy.tmp");
}

static int storage_has_data(const char *directory)
{
    storage_stage("inspect stored settings",directory);
    DIR *dir=opendir(directory);
    if (!dir) return -1;
    int found=0; struct dirent *entry;
    errno=0;
    while ((entry=readdir(dir))) {
        if (!storage_internal(entry->d_name)) { found=1; break; }
    }
    int error=errno; closedir(dir);
    if (error) { errno=error; return -1; }
    return found;
}

/* The journal records which snapshot owns an interrupted migration. */
static int storage_journal(void)
{
    storage_stage("read migration journal",STORAGE_JOURNAL);
    struct stat info;
    if (lstat(STORAGE_JOURNAL,&info)) return errno==ENOENT ? 0 : -1;
    if (!S_ISREG(info.st_mode)) { errno=EINVAL; return -1; }
    FILE *file=fopen(STORAGE_JOURNAL,"rb");
    if (!file) return -1;
    char value[16]={0}; size_t count=fread(value,1,sizeof(value)-1,file);
    int error=ferror(file); fclose(file);
    if (error) { errno=EIO; return -1; }
    if (count==7 && !memcmp(value,"legacy\n",7)) return 1;
    if (count==8 && !memcmp(value,"current\n",8)) return 2;
    if (count==5 && !memcmp(value,"done\n",5)) return 3;
    errno=EINVAL; return -1;
}

static int storage_write_journal(const char *text)
{
    storage_stage("write migration journal",STORAGE_JOURNAL);
    int fd=open(STORAGE_TEMP,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) return -1;
    size_t length=strlen(text);
    int result=0;
    ssize_t count=write(fd,text,length);
    if (count!=(ssize_t)length) { if (count>=0) errno=EIO; result=-1; }
    else if (fsync(fd)) result=-1;
    int error=errno;
    if (close(fd) && !result) { result=-1; error=errno; }
    if (!result && rename(STORAGE_TEMP,STORAGE_JOURNAL)) { result=-1; error=errno; }
    if (result) { unlink(STORAGE_TEMP); errno=error ? error : EIO; }
    return result;
}

static int storage_same_file(const char *source, const char *destination)
{
    FILE *a=fopen(source,"rb"), *b=fopen(destination,"rb");
    if (!a || !b) {
        int error=errno; if (a) fclose(a); if (b) fclose(b); errno=error; return -1;
    }
    unsigned char x[4096], y[4096]; int same=1;
    for (;;) {
        size_t nx=fread(x,1,sizeof(x),a), ny=fread(y,1,sizeof(y),b);
        if (ferror(a) || ferror(b)) { same=-1; errno=EIO; break; }
        if (nx!=ny || memcmp(x,y,nx)) { same=0; break; }
        if (!nx) break;
    }
    int error=errno; fclose(a); fclose(b); errno=error; return same;
}

static int storage_copy_file(const char *source, const char *destination, mode_t mode)
{
    struct stat info;
    storage_stage("check migration destination",destination);
    if (!lstat(destination,&info)) {
        if (!S_ISREG(info.st_mode)) { errno=EEXIST; return -1; }
        int same=storage_same_file(source,destination);
        if (same==1) return 0;
        if (!same) errno=EEXIST;
        return -1;
    }
    if (errno!=ENOENT) return -1;
    storage_stage("copy saved settings",source);
    int input=open(source,O_RDONLY);
    if (input<0) return -1;
    int output=open(STORAGE_TEMP,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (output<0) { int error=errno; close(input); errno=error; return -1; }
    char buffer[8192]; int result=0;
    for (;;) {
        ssize_t count=read(input,buffer,sizeof(buffer));
        if (count<0) { if (errno==EINTR) continue; result=-1; break; }
        if (!count) break;
        ssize_t offset=0;
        while (offset<count) {
            ssize_t written=write(output,buffer+offset,(size_t)(count-offset));
            if (written<0 && errno==EINTR) continue;
            if (written<=0) { if (!written) errno=EIO; result=-1; break; }
            offset+=written;
        }
        if (result) break;
    }
    if (!result && (fchmod(output,mode & 0777) || fsync(output))) result=-1;
    int error=errno; close(input);
    if (close(output) && !result) { result=-1; error=errno; }
    if (!result) {
        storage_stage("finish settings copy",destination);
        if (!lstat(destination,&info)) { result=-1; error=EEXIST; }
        else if (errno!=ENOENT) { result=-1; error=errno; }
        else if (rename(STORAGE_TEMP,destination)) { result=-1; error=errno; }
    }
    if (result) { unlink(STORAGE_TEMP); errno=error ? error : EIO; }
    return result;
}

static const char *storage_file_name(const char *name, int to_current)
{
    if (to_current) {
        if (!strcmp(name,"pdp-user")) return "controller-user";
        if (!strcmp(name,"pdp-pad.log")) return "LibrePadPS5.log";
    } else {
        if (!strcmp(name,"controller-user")) return "pdp-user";
        if (!strcmp(name,"LibrePadPS5.log")) return "pdp-pad.log";
    }
    return name;
}

static int storage_copy_tree(const char *source, const char *destination, int direction, int root)
{
    storage_stage("open migration source",source);
    DIR *dir=opendir(source);
    if (!dir) return -1;
    int result=0; struct dirent *entry;
    for (;;) {
        errno=0; entry=readdir(dir);
        if (!entry) { if (errno) result=-1; break; }
        const char *name=entry->d_name;
        if (!strcmp(name,".") || !strcmp(name,"..") || (root && storage_internal(name))) continue;
        char from[PATH_MAX], to[PATH_MAX];
        const char *new_name=root ? storage_file_name(name,direction==1) : name;
        if (snprintf(from,sizeof(from),"%s/%s",source,name)>=(int)sizeof(from) ||
            snprintf(to,sizeof(to),"%s/%s",destination,new_name)>=(int)sizeof(to)) {
            errno=ENAMETOOLONG; result=-1; break;
        }
        storage_stage("inspect migration source",from);
        struct stat info;
        if (lstat(from,&info)) { result=-1; break; }
        if (S_ISDIR(info.st_mode)) {
            storage_stage("create settings subdirectory",to);
            if (mkdir(to,info.st_mode & 0777) && errno!=EEXIST) { result=-1; break; }
            struct stat target;
            if (lstat(to,&target)) { result=-1; break; }
            if (!S_ISDIR(target.st_mode)) { errno=EEXIST; result=-1; break; }
            if (storage_copy_tree(from,to,direction,0)) { result=-1; break; }
        } else if (S_ISREG(info.st_mode)) {
            if (storage_copy_file(from,to,info.st_mode)) { result=-1; break; }
        } else { errno=ENOTSUP; result=-1; break; }
    }
    int error=errno; closedir(dir); errno=error; return result;
}

/* Finish a v1.1.2 directory move without creating any new links. */
static int storage_rename_file(const char *old_name, const char *new_name)
{
    char old_path[PATH_MAX], new_path[PATH_MAX]; struct stat old, current;
    if (snprintf(old_path,sizeof(old_path),"%s/%s",LIBREPAD_STATE_DIR,old_name)>=(int)sizeof(old_path) ||
        snprintf(new_path,sizeof(new_path),"%s/%s",LIBREPAD_STATE_DIR,new_name)>=(int)sizeof(new_path)) {
        errno=ENAMETOOLONG; return -1;
    }
    storage_stage("normalize migrated filename",old_path);
    if (lstat(old_path,&old)) return errno==ENOENT ? 0 : -1;
    if (S_ISLNK(old.st_mode)) {
        if (!stat(old_path,&old) && !stat(new_path,&current) &&
            old.st_dev==current.st_dev && old.st_ino==current.st_ino) return 0;
        errno=EINVAL; return -1;
    }
    if (!S_ISREG(old.st_mode)) { errno=EINVAL; return -1; }
    if (!lstat(new_path,&current)) { errno=EEXIST; return -1; }
    if (errno!=ENOENT) return -1;
    return rename(old_path,new_path);
}

int librepad_storage_open(void)
{
    struct stat current, legacy; int fd=-1, legacy_fd=-1;
    storage_stage("inspect current directory",LIBREPAD_STATE_DIR);
    int have_current=lstat(LIBREPAD_STATE_DIR,&current)==0;
    if (!have_current && errno!=ENOENT) return -1;
    storage_stage("inspect legacy directory",LIBREPAD_LEGACY_STATE_DIR);
    int have_legacy=lstat(LIBREPAD_LEGACY_STATE_DIR,&legacy)==0;
    if (!have_legacy && errno!=ENOENT) return -1;
    storage_stage("validate storage directories",LIBREPAD_STATE_DIR);
    if (have_current && !S_ISDIR(current.st_mode)) { errno=ENOTDIR; return -1; }
    if (have_legacy && S_ISLNK(legacy.st_mode)) {
        struct stat target;
        if (!have_current || stat(LIBREPAD_LEGACY_STATE_DIR,&target)) return -1;
        if (target.st_dev!=current.st_dev || target.st_ino!=current.st_ino) { errno=EINVAL; return -1; }
        /* Preserve an already valid alias on filesystems that support it. */
        fd=storage_lock(LIBREPAD_STATE_DIR);
        if (fd<0) return -1;
        g_current_lock=fd; g_legacy_lock=-1; return fd;
    }
    if (have_legacy && !S_ISDIR(legacy.st_mode)) { errno=ENOTDIR; return -1; }
    storage_stage("create legacy lock directory",LIBREPAD_LEGACY_STATE_DIR);
    if (!have_legacy && mkdir(LIBREPAD_LEGACY_STATE_DIR,0755)) return -1;
    /* Lock the old namespace first, so older and newer payloads cannot overlap. */
    legacy_fd=storage_lock(LIBREPAD_LEGACY_STATE_DIR);
    if (legacy_fd<0) return -1;
    storage_stage("create current directory",LIBREPAD_STATE_DIR);
    if (!have_current && mkdir(LIBREPAD_STATE_DIR,0755)) goto failed;
    fd=storage_lock(LIBREPAD_STATE_DIR);
    if (fd<0) goto failed;
    int journal=storage_journal();
    if (journal<0) goto failed;
    if (!journal) {
        storage_stage("check migration temporary file",STORAGE_TEMP);
        struct stat temp;
        if (!lstat(STORAGE_TEMP,&temp)) { errno=EEXIST; goto failed; }
        if (errno!=ENOENT) goto failed;
        int old_data=storage_has_data(LIBREPAD_LEGACY_STATE_DIR);
        if (old_data<0) goto failed;
        int new_data=storage_has_data(LIBREPAD_STATE_DIR);
        if (new_data<0) goto failed;
        storage_stage("check unrelated settings directories",LIBREPAD_STATE_DIR);
        if (old_data && new_data) { errno=EEXIST; goto failed; }
        journal=old_data ? 1 : (new_data ? 2 : 3);
        if (storage_write_journal(journal==1 ? "legacy\n" : journal==2 ? "current\n" : "done\n")) goto failed;
    } else {
        /* This filename belongs to the recorded migration, never to settings. */
        storage_stage("resume interrupted settings copy",STORAGE_TEMP);
        if (unlink(STORAGE_TEMP) && errno!=ENOENT) goto failed;
    }
    if (journal!=3) {
        if (storage_copy_tree(journal==1 ? LIBREPAD_LEGACY_STATE_DIR : LIBREPAD_STATE_DIR,
                              journal==1 ? LIBREPAD_STATE_DIR : LIBREPAD_LEGACY_STATE_DIR,journal,1)) goto failed;
        if (storage_rename_file("pdp-user","controller-user") ||
            storage_rename_file("pdp-pad.log","LibrePadPS5.log")) goto failed;
        if (storage_write_journal("done\n")) goto failed;
    }
    g_current_lock=fd; g_legacy_lock=legacy_fd; return fd;
failed: {
    int error=errno;
    if (fd>=0) close(fd);
    if (legacy_fd>=0) close(legacy_fd);
    errno=error; return -1;
}
}
