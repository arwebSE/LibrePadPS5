/* Migration must work on filesystems where symbolic links are unsupported. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
static int interrupted_copy;
static int migration_rename(const char *source, const char *destination);
#define rename migration_rename
#define symlink LIBREPAD_STORAGE_MUST_NOT_CREATE_LINKS
#define readlink LIBREPAD_STORAGE_MUST_NOT_READ_LINKS
#define realpath LIBREPAD_STORAGE_MUST_NOT_REQUIRE_REALPATH
#define LIBREPAD_STATE_DIR "build/storage-current"
#define LIBREPAD_LEGACY_STATE_DIR "build/storage-legacy"
#include "../src/core/storage.c"
#undef rename
#undef symlink
#undef readlink
#undef realpath
#include <assert.h>

static int migration_rename(const char *source, const char *destination)
{
    if (interrupted_copy && strstr(destination,"/native-user")) {
        errno=ENOSPC; return -1;
    }
    return rename(source,destination);
}

static void remove_tree(const char *path)
{
    assert(!strncmp(path,"build/storage-",14));
    struct stat info;
    if (lstat(path,&info)) { assert(errno==ENOENT); return; }
    if (!S_ISDIR(info.st_mode)) { assert(!unlink(path)); return; }
    DIR *dir=opendir(path); assert(dir); struct dirent *entry;
    while ((entry=readdir(dir))) {
        if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        char child[PATH_MAX];
        assert(snprintf(child,sizeof(child),"%s/%s",path,entry->d_name)<(int)sizeof(child));
        remove_tree(child);
    }
    closedir(dir); assert(!rmdir(path));
}
static void reset(void)
{
    remove_tree(LIBREPAD_LEGACY_STATE_DIR);
    remove_tree(LIBREPAD_STATE_DIR);
    remove_tree("build/storage-other");
}
static void put(const char *path, const char *text)
{
    FILE *file=fopen(path,"w"); assert(file);
    assert(fputs(text,file)>=0); assert(!fclose(file));
}
static void check(const char *path, const char *text)
{
    FILE *file=fopen(path,"r"); assert(file);
    char data[128]={0}; assert(fread(data,1,sizeof(data)-1,file)==strlen(text));
    assert(!strcmp(data,text)); fclose(file);
}
static void locked(const char *directory)
{
    char path[PATH_MAX]; snprintf(path,sizeof(path),"%s/instance.lock",directory);
    int fd=open(path,O_RDWR); assert(fd>=0);
    assert(flock(fd,LOCK_EX|LOCK_NB)==-1 && (errno==EWOULDBLOCK || errno==EAGAIN));
    close(fd);
}
static void seed_legacy(void)
{
    assert(!mkdir(LIBREPAD_LEGACY_STATE_DIR,0700));
    put(LIBREPAD_LEGACY_STATE_DIR "/native-user","1ab32fc4\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/native-users","1ab32fc7\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pdp-user","1ab32fc5\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pdp-pad.log","previous log\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/bluetooth.db","private bond\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pause-3","");
    put(LIBREPAD_LEGACY_STATE_DIR "/unknown-setting","keep me\n");
    assert(!mkdir(LIBREPAD_LEGACY_STATE_DIR "/xbox",0700));
    put(LIBREPAD_LEGACY_STATE_DIR "/xbox/firmware.bin","firmware bytes\n");
    assert(!mkdir(LIBREPAD_LEGACY_STATE_DIR "/maps",0700));
    put(LIBREPAD_LEGACY_STATE_DIR "/maps/abcd_1234.map","mapping\n");
}
static void migrated(void)
{
    check(LIBREPAD_NATIVE_USER_PATH,"1ab32fc4\n");
    check(LIBREPAD_NATIVE_USERS_PATH,"1ab32fc7\n");
    check(LIBREPAD_CONTROLLER_USER_PATH,"1ab32fc5\n");
    check(LIBREPAD_LOG_PATH,"previous log\n");
    check(LIBREPAD_XBOX_FIRMWARE_PATH,"firmware bytes\n");
    check(LIBREPAD_STATE_DIR "/bluetooth.db","private bond\n");
    check(LIBREPAD_STATE_DIR "/maps/abcd_1234.map","mapping\n");
    check(LIBREPAD_STATE_DIR "/unknown-setting","keep me\n");
    assert(!access(LIBREPAD_STATE_DIR "/pause-3",F_OK));
    check(LIBREPAD_LEGACY_STATE_DIR "/pdp-user","1ab32fc5\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/pdp-pad.log","previous log\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/xbox/firmware.bin","firmware bytes\n");
    check(STORAGE_JOURNAL,"done\n");
}
int main(void)
{
    mkdir("build",0700); reset();
    int fd=librepad_storage_open(); assert(fd>=0);
    struct stat info; assert(!lstat(LIBREPAD_LEGACY_STATE_DIR,&info) && S_ISDIR(info.st_mode));
    locked(LIBREPAD_LEGACY_STATE_DIR); locked(LIBREPAD_STATE_DIR);
    assert(librepad_storage_open()==-1); librepad_storage_close(fd);
    fd=librepad_storage_open(); assert(fd>=0); librepad_storage_close(fd); reset();

    seed_legacy();
    int old_lock=storage_lock(LIBREPAD_LEGACY_STATE_DIR); assert(old_lock>=0);
    assert(librepad_storage_open()==-1 && access(LIBREPAD_STATE_DIR,F_OK)==-1);
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","1ab32fc4\n"); close(old_lock);
    fd=librepad_storage_open(); assert(fd>=0); migrated();
    locked(LIBREPAD_LEGACY_STATE_DIR); locked(LIBREPAD_STATE_DIR);
    assert(librepad_storage_open()==-1); librepad_storage_close(fd);
    /* A normal restart must not import the older rollback snapshot again. */
    put(LIBREPAD_CONTROLLER_USER_PATH,"new preferred profile\n");
    fd=librepad_storage_open(); assert(fd>=0);
    check(LIBREPAD_CONTROLLER_USER_PATH,"new preferred profile\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/pdp-user","1ab32fc5\n");
    librepad_storage_close(fd); reset();

    /* Recover files moved by v1.1.2 before unsupported symlink creation failed. */
    assert(!mkdir(LIBREPAD_STATE_DIR,0700));
    put(LIBREPAD_STATE_DIR "/pdp-user","200\n");
    put(LIBREPAD_STATE_DIR "/native-user","201\n");
    put(LIBREPAD_STATE_DIR "/pdp-pad.log","old log\n");
    fd=librepad_storage_open(); assert(fd>=0);
    check(LIBREPAD_CONTROLLER_USER_PATH,"200\n");
    check(LIBREPAD_LOG_PATH,"old log\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/pdp-user","200\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","201\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/pdp-pad.log","old log\n");
    librepad_storage_close(fd); reset();

    /* A failed copy preserves originals, releases both locks and resumes. */
    seed_legacy(); interrupted_copy=1;
    assert(librepad_storage_open()==-1 && errno==ENOSPC);
    assert(strstr(librepad_storage_error_stage(),"finish settings copy"));
    check(STORAGE_JOURNAL,"legacy\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","1ab32fc4\n");
    old_lock=storage_lock(LIBREPAD_LEGACY_STATE_DIR); assert(old_lock>=0); close(old_lock);
    put(STORAGE_TEMP,"interrupted temporary copy\n"); interrupted_copy=0;
    fd=librepad_storage_open(); assert(fd>=0); migrated(); librepad_storage_close(fd); reset();

    /* Conflicting directories/files are preserved rather than overwritten. */
    assert(!mkdir(LIBREPAD_STATE_DIR,0700)); assert(!mkdir(LIBREPAD_LEGACY_STATE_DIR,0700));
    put(LIBREPAD_STATE_DIR "/native-user","new\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/native-user","old\n");
    assert(librepad_storage_open()==-1 && errno==EEXIST);
    check(LIBREPAD_STATE_DIR "/native-user","new\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","old\n"); reset();
    assert(!mkdir(LIBREPAD_STATE_DIR,0700));
    put(LIBREPAD_STATE_DIR "/pdp-user","old preferred\n");
    put(LIBREPAD_CONTROLLER_USER_PATH,"new preferred\n");
    assert(librepad_storage_open()==-1 && errno==EEXIST);
    check(LIBREPAD_STATE_DIR "/pdp-user","old preferred\n");
    check(LIBREPAD_CONTROLLER_USER_PATH,"new preferred\n"); reset();
    assert(!mkdir(LIBREPAD_STATE_DIR,0700)); put(STORAGE_JOURNAL,"invalid journal\n");
    assert(librepad_storage_open()==-1 && errno==EINVAL); reset();
    assert(!mkdir(LIBREPAD_STATE_DIR,0700)); put(STORAGE_TEMP,"unrelated file\n");
    assert(librepad_storage_open()==-1 && errno==EEXIST);
    check(STORAGE_TEMP,"unrelated file\n"); reset();
    put(LIBREPAD_STATE_DIR,"not a directory\n");
    assert(librepad_storage_open()==-1 && errno==ENOTDIR);
    check(LIBREPAD_STATE_DIR,"not a directory\n"); reset();

    /* Accept an existing valid directory alias, without requiring new links. */
    assert(!mkdir(LIBREPAD_STATE_DIR,0700));
    char cwd[PATH_MAX], target[PATH_MAX]; assert(getcwd(cwd,sizeof(cwd)));
    assert(snprintf(target,sizeof(target),"%s/%s",cwd,LIBREPAD_STATE_DIR)<(int)sizeof(target));
    assert(!symlink(target,LIBREPAD_LEGACY_STATE_DIR));
    fd=librepad_storage_open(); assert(fd>=0); locked(LIBREPAD_LEGACY_STATE_DIR);
    librepad_storage_close(fd); reset();
    assert(!mkdir(LIBREPAD_STATE_DIR,0700)); assert(!mkdir("build/storage-other",0700));
    assert(snprintf(target,sizeof(target),"%s/build/storage-other",cwd)<(int)sizeof(target));
    assert(!symlink(target,LIBREPAD_LEGACY_STATE_DIR));
    assert(librepad_storage_open()==-1 && errno==EINVAL); reset();
    puts("Storage: no links, both locks, settings/firmware, rollback copies, interrupted migration and conflicts passed");
}
