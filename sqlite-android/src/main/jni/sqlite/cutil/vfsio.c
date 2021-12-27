#include "vfsio.h"
#include <stdint.h>
#include <sys/types.h>
#include <linux/fs.h>
#include <asm/errno.h>
#include "jniutil.h"
#include "check.h"

jclass vfsIoClassGlobal;

jmethodID openMethodId;
jmethodID deleteMethodId;
jmethodID accessMethodId;

jmethodID readMethodId;
jmethodID writeMethodId;
jmethodID flushMethodId;
jmethodID closeMethodId;
jmethodID seekMethodId;
jmethodID getFilePointerMethodId;
jmethodID lengthMethodId;

jmethodID preadMethodId;
jmethodID pwriteMethodId;


//#define CHECK

static jint cache_methods(JNIEnv *env) {

    jclass vfsCls;
    vfsCls = CHECK((*env)->FindClass(env, "org/sqlite/database/sqlite/SQLiteVirtualFileSystem"));
    openMethodId = CHECK((*env)->GetMethodID(env, vfsCls, "open",
                                             "(Ljava/lang/String;)Lorg/sqlite/database/sqlite/SQLiteVfsIo;"));
    deleteMethodId = CHECK((*env)->GetMethodID(env, vfsCls, "delete",
                                               "(Ljava/lang/String;)V"));
    accessMethodId = CHECK((*env)->GetMethodID(env, vfsCls, "access",
                                               "(Ljava/lang/String;I)I"));
    (*env)->DeleteLocalRef(env, vfsCls);

    jclass vfsIoCls;
    vfsIoCls = CHECK((*env)->FindClass(env, "org/sqlite/database/sqlite/SQLiteVfsIo"));

    readMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "read", "([BII)I"));
    writeMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "write", "([BII)V"));
    flushMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "flush", "()V"));
    closeMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "close", "()V"));
    seekMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "setPosition", "(J)V"));
    getFilePointerMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "getPosition", "()J"));
    lengthMethodId = CHECK((*env)->GetMethodID(env, vfsIoCls, "getLength", "()J"));


    vfsIoClassGlobal = CHECK((*env)->NewGlobalRef(env, vfsIoCls));
    preadMethodId = CHECK((*env)->GetStaticMethodID(env, vfsIoCls, "pread",
                                                    "(Lorg/sqlite/database/sqlite/SQLiteVfsIo;[BIIJ)I"));
    pwriteMethodId = CHECK((*env)->GetStaticMethodID(env, vfsIoCls, "pwrite",
                                                     "(Lorg/sqlite/database/sqlite/SQLiteVfsIo;[BIIJ)I"));
    (*env)->DeleteLocalRef(env, vfsIoCls);
    return JNI_OK;
}

int init_vfsio(JNIEnv *env) {
    if (cache_methods(env) == JNI_ERR)
        return JNI_ERR;

    return JNI_OK;
}

static void clean_classes_cache(JNIEnv *env) {
    (*env)->DeleteGlobalRef(env, vfsIoClassGlobal);
}

void clear_vfsio(JNIEnv *env) {
    clean_classes_cache(env);
}

jobject vfsio_open(JNIEnv *env, jobject vfs, const char *path) {
    jobject vfsIo = 0;
    jstring jpath = (*env)->NewStringUTF(env, path);
    if (jpath == 0)
        return 0;
    call_jni_object_func(env, vfs, openMethodId, &vfsIo, jpath);
    (*env)->DeleteLocalRef(env, jpath);
    return vfsIo;
}

int vfsio_delete(JNIEnv *env, jobject vfs, const char *path) {
    int rc = -1;
    jstring jpath = (*env)->NewStringUTF(env, path);
    if (jpath == 0)
        return rc;
    rc = call_jni_void_func(env, vfs, deleteMethodId, jpath);
    (*env)->DeleteLocalRef(env, jpath);
    return rc;
}

int vfsio_access(JNIEnv *env, jobject vfs, const char *path, int mode) {
    int res = -1;
    jstring jpath = (*env)->NewStringUTF(env, path);
    if (jpath == 0)
        return res;
    call_jni_int_func(env, vfs, accessMethodId, &res, jpath, (jint) mode);
    (*env)->DeleteLocalRef(env, jpath);
    return res;
}


ssize_t vfsio_read(JNIEnv *env, jobject vfsio, uint8_t *buf, size_t count) {
    int rc;
    ssize_t res = 0;
    jbyteArray byteArray;
    jint bytesRead;

    byteArray = (*env)->NewByteArray(env, count);
    if (byteArray == NULL) {
        res = (ssize_t) -1;
        return res;
    }

    rc = call_jni_int_func(env, vfsio, readMethodId, &bytesRead,
                           byteArray, (jint) 0, (jint) count);
    if (rc) {
        res = (ssize_t) -1;
        (*env)->DeleteLocalRef(env, byteArray);
        return res;
    }
    if (bytesRead > 0) {
        (*env)->GetByteArrayRegion(env, byteArray, 0, bytesRead, (jbyte *) buf);
    }
    res = bytesRead;
    (*env)->DeleteLocalRef(env, byteArray);
    return res;
}

ssize_t vfsio_pread(JNIEnv *env, jobject vfsio, uint8_t *buf, size_t count, off64_t pos) {
    int rc;
    ssize_t res;
    jbyteArray byteArray;
    jint bytesRead;

    byteArray = (*env)->NewByteArray(env, count);
    if (byteArray == NULL) {
        res = (ssize_t) -1;
        return res;
    }

    rc = call_jni_static_int_func(env, vfsIoClassGlobal, preadMethodId, &bytesRead,
                                  vfsio, byteArray, (jint) 0, (jint) count, (jlong) pos);
    if (rc) {
        res = (ssize_t) -1;
        (*env)->DeleteLocalRef(env, byteArray);
        return res;
    }
    if (bytesRead > 0) {
        (*env)->GetByteArrayRegion(env, byteArray, 0, bytesRead, (jbyte *) buf);
    }
    res = bytesRead;
    (*env)->DeleteLocalRef(env, byteArray);
    return res;
}

ssize_t vfsio_write(JNIEnv *env, jobject vfsio, const uint8_t *buf, size_t count) {
    int rc;
    ssize_t res;
    jbyteArray byteArray;

    byteArray = (*env)->NewByteArray(env, count);
    if (byteArray == NULL) {
        res = (ssize_t) -1;
        return res;
    }
    (*env)->SetByteArrayRegion(env, byteArray, 0, count, (const jbyte *) buf);

    rc = call_jni_void_func(env, vfsio, writeMethodId,
                            byteArray, (jint) 0, (jint) count);
    if (rc)
        res = (ssize_t) -1;
    else
        res = count;
    (*env)->DeleteLocalRef(env, byteArray);

    return res;
}

ssize_t vfsio_pwrite(JNIEnv *env, jobject vfsio, const uint8_t *buf, size_t count, off64_t pos) {
    int rc;
    ssize_t res = 0;
    jbyteArray byteArray;

    byteArray = (*env)->NewByteArray(env, count);
    if (byteArray == NULL) {
        res = (ssize_t) -1;
        return res;
    }
    (*env)->SetByteArrayRegion(env, byteArray, 0, count, (const jbyte *) buf);

    rc = call_jni_static_int_func(env, vfsIoClassGlobal, pwriteMethodId, &res,
                                  vfsio, byteArray, (jint) 0, (jint) count, (jlong) pos);
    if (rc)
        res = (ssize_t) -1;
    else
        res = count;
    (*env)->DeleteLocalRef(env, byteArray);

    return res;
}

int vfsio_seek(JNIEnv *env, jobject vfsio, off64_t pos) {
    return call_jni_void_func(env, vfsio, seekMethodId,
                              (jlong) pos);
}

int vfsio_seek_whence(JNIEnv *env, jobject vfsio, off64_t pos, int whence) {
    int rc;
    off64_t cur;
    switch (whence) {
        case SEEK_SET:
            rc = vfsio_seek(env, vfsio, pos);
            if (rc)
                return (off64_t) -1;
            break;
        case SEEK_CUR:
            cur = vfsio_get_pos(env, vfsio);
            if (cur == (off64_t) -1)
                return cur;
            rc = vfsio_seek(env, vfsio, pos + cur);
            if (rc)
                return (off64_t) -1;
            break;
        case SEEK_END:
            cur = vfsio_get_size(env, vfsio);
            if (cur == (off64_t) -1)
                return cur;
            rc = vfsio_seek(env, vfsio, pos + cur);
            if (rc)
                return (off64_t) -1;
            break;
    }
    return vfsio_get_pos(env, vfsio);
}

off64_t vfsio_get_pos(JNIEnv *env, jobject vfsio) {
    int res = 0;
    jlong pos;

    res = call_jni_long_func(env, vfsio, getFilePointerMethodId, &pos);
    if (res)
        pos = -1;
    return (off64_t) pos;
}

int vfsio_flush(JNIEnv *env, jobject vfsio) {
    return call_jni_void_func(env, vfsio, flushMethodId);
}

int vfsio_close(JNIEnv *env, jobject vfsio) {
    return call_jni_void_func(env, vfsio, closeMethodId);
}

off64_t vfsio_get_size(JNIEnv *env, jobject vfsio) {
    int res = 0;
    jlong size;

    res = call_jni_long_func(env, vfsio, lengthMethodId, &size);
    if (res)
        size = -1;
    return (off64_t) size;
}

int vfsio_fsync(JNIEnv *env, jobject vfsio) {
    int rc = 0;
    if (vfsio_flush(env, vfsio) != 0) {
        rc = -EIO;
    }
    return rc;
}