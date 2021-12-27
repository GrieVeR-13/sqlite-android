#include <jni.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef VFSIO_H
#define VFSIO_H

int init_vfsio(JNIEnv *env);
void clear_vfsio(JNIEnv *env);
jobject vfsio_open(JNIEnv *env, jobject vfs, const char *path);
int vfsio_delete(JNIEnv *env, jobject vfs, const char *path);
int vfsio_access(JNIEnv *env, jobject vfs, const char *path, int mode);
ssize_t vfsio_read(JNIEnv *env, jobject vfsio, uint8_t *buf, size_t count);
ssize_t vfsio_pread(JNIEnv *env, jobject vfsio, uint8_t *buf, size_t count, off64_t pos);
ssize_t vfsio_write(JNIEnv *env, jobject vfsio, const uint8_t *buf, size_t count);
ssize_t vfsio_pwrite(JNIEnv *env, jobject vfsio, const uint8_t *buf, size_t count, off64_t pos);
int vfsio_seek(JNIEnv *env, jobject vfsio, off64_t pos);
int vfsio_seek_whence(JNIEnv *env, jobject vfsio, off64_t pos, int whence);
off64_t vfsio_get_pos(JNIEnv *env, jobject vfsio);
int vfsio_close(JNIEnv *env, jobject vfsio);
int vfsio_flush(JNIEnv *env, jobject vfsio);
off64_t vfsio_get_size(JNIEnv *env, jobject vfsio);
int vfsio_fsync(JNIEnv *env, jobject vfsio);

#endif //VFSIO_H
