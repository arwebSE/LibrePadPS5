/* Storage migration must retain every file, native reservation and lock. */
#define _DEFAULT_SOURCE
#define LIBREPAD_STATE_DIR "build/storage-current"
#define LIBREPAD_LEGACY_STATE_DIR "build/storage-legacy"
#include "../src/core/storage.c"
#include <assert.h>
#include <dirent.h>

static void remove_tree(const char *path)
{
    assert(!strncmp(path,"build/storage-",14));
    struct stat info;
    if (lstat(path,&info)) { assert(errno==ENOENT); return; }
    if (!S_ISDIR(info.st_mode)) { assert(!unlink(path)); return; }
    DIR *dir=opendir(path); assert(dir);
    struct dirent *entry;
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
    FILE *f=fopen(path,"w"); assert(f);
    assert(fputs(text,f)>=0); assert(!fclose(f));
}
static void check(const char *path, const char *text)
{
    FILE *f=fopen(path,"r"); assert(f);
    char data[128]={0}; assert(fread(data,1,sizeof(data)-1,f)==strlen(text));
    assert(!strcmp(data,text)); fclose(f);
}
static void same(const char *a, const char *b)
{
    struct stat x,y; assert(!stat(a,&x) && !stat(b,&y));
    assert(x.st_dev==y.st_dev && x.st_ino==y.st_ino);
}
int main(void)
{
    mkdir("build",0700); reset();
    int fd=librepad_storage_open(); assert(fd>=0);
    same(LIBREPAD_STATE_DIR,LIBREPAD_LEGACY_STATE_DIR);
    int old_lock=open(LIBREPAD_LEGACY_STATE_DIR "/instance.lock",O_RDWR);
    assert(old_lock>=0 && flock(old_lock,LOCK_EX|LOCK_NB)==-1);
    assert(errno==EWOULDBLOCK || errno==EAGAIN);
    close(old_lock);
    assert(librepad_storage_open()==-1); close(fd);
    fd=librepad_storage_open(); assert(fd>=0); close(fd); reset();

    assert(!mkdir(LIBREPAD_LEGACY_STATE_DIR,0700));
    put(LIBREPAD_LEGACY_STATE_DIR "/native-user","1ab32fc4\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/native-users","1ab32fc7\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pdp-user","1ab32fc5\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pdp-pad.log","previous log\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/bluetooth.db","private bond\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/pause-3","");
    put(LIBREPAD_LEGACY_STATE_DIR "/unknown-setting","keep me\n");
    mkdir(LIBREPAD_LEGACY_STATE_DIR "/xbox",0700);
    put(LIBREPAD_LEGACY_STATE_DIR "/xbox/firmware.bin","firmware bytes\n");
    mkdir(LIBREPAD_LEGACY_STATE_DIR "/maps",0700);
    put(LIBREPAD_LEGACY_STATE_DIR "/maps/abcd_1234.map","mapping\n");
    old_lock=storage_lock(LIBREPAD_LEGACY_STATE_DIR); assert(old_lock>=0);
    assert(librepad_storage_open()==-1 && access(LIBREPAD_STATE_DIR,F_OK)==-1);
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","1ab32fc4\n");
    close(old_lock);
    fd=librepad_storage_open(); assert(fd>=0);
    check(LIBREPAD_NATIVE_USER_PATH,"1ab32fc4\n");
    check(LIBREPAD_NATIVE_USERS_PATH,"1ab32fc7\n");
    check(LIBREPAD_CONTROLLER_USER_PATH,"1ab32fc5\n");
    check(LIBREPAD_LOG_PATH,"previous log\n");
    check(LIBREPAD_XBOX_FIRMWARE_PATH,"firmware bytes\n");
    check(LIBREPAD_STATE_DIR "/bluetooth.db","private bond\n");
    check(LIBREPAD_STATE_DIR "/maps/abcd_1234.map","mapping\n");
    check(LIBREPAD_STATE_DIR "/unknown-setting","keep me\n");
    assert(!access(LIBREPAD_STATE_DIR "/pause-3",F_OK));
    same(LIBREPAD_CONTROLLER_USER_PATH,LIBREPAD_LEGACY_STATE_DIR "/pdp-user");
    same(LIBREPAD_LOG_PATH,LIBREPAD_LEGACY_STATE_DIR "/pdp-pad.log");
    same(LIBREPAD_STATE_DIR "/instance.lock",LIBREPAD_LEGACY_STATE_DIR "/instance.lock");
    close(fd); reset();

    /* Interrupted migration: directory moved, aliases not written yet. */
    mkdir(LIBREPAD_STATE_DIR,0700); put(LIBREPAD_STATE_DIR "/pdp-user","200\n");
    fd=librepad_storage_open(); assert(fd>=0);
    check(LIBREPAD_CONTROLLER_USER_PATH,"200\n"); close(fd); reset();

    /* Conflicts and unrelated symlinks are refused without losing settings. */
    mkdir(LIBREPAD_STATE_DIR,0700); mkdir(LIBREPAD_LEGACY_STATE_DIR,0700);
    put(LIBREPAD_STATE_DIR "/native-user","new\n");
    put(LIBREPAD_LEGACY_STATE_DIR "/native-user","old\n");
    assert(librepad_storage_open()==-1 && errno==EEXIST);
    check(LIBREPAD_STATE_DIR "/native-user","new\n");
    check(LIBREPAD_LEGACY_STATE_DIR "/native-user","old\n"); reset();
    mkdir(LIBREPAD_STATE_DIR,0700); mkdir("build/storage-other",0700);
    char target[PATH_MAX]; assert(realpath("build/storage-other",target));
    assert(!symlink(target,LIBREPAD_LEGACY_STATE_DIR));
    assert(librepad_storage_open()==-1 && errno==EINVAL); reset();
    mkdir(LIBREPAD_STATE_DIR,0700);
    put(LIBREPAD_STATE_DIR "/pdp-user","old preferred\n");
    put(LIBREPAD_CONTROLLER_USER_PATH,"new preferred\n");
    assert(librepad_storage_open()==-1 && errno==EEXIST);
    check(LIBREPAD_STATE_DIR "/pdp-user","old preferred\n");
    check(LIBREPAD_CONTROLLER_USER_PATH,"new preferred\n"); reset();
    put(LIBREPAD_STATE_DIR,"not a directory\n");
    assert(librepad_storage_open()==-1 && errno==ENOTDIR);
    check(LIBREPAD_STATE_DIR,"not a directory\n"); reset();
    puts("Storage: migration, all settings, firmware, logs, alias locks, conflicts and interrupted startup passed");
}
