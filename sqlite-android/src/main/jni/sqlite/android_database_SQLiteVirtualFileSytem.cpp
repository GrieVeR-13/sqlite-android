#include <jni.h>
#include <unistd.h>
#include <fcntl.h>
#include "sqlite3.h"
#include <cstdio>
#include <ctime>
#include <cassert>
#include <cerrno>
#include <cstring>
#include "ALog-priv.h"

#define LOG_TAG "SQLiteVirtualFileSystem"
#define SQLITE_VFS_BUFFERSZ 8192
#define MAXPATHNAME 512

JNIEnv *getEnv();

#ifdef __cplusplus
extern "C" {
#endif


static jobject vfs = nullptr;

#include "cutil/vfsio.h"

struct VfsFile {
    sqlite3_file base;              /* Base class. Must be first. */
    jobject vfsIo;

    char *aBuffer;                  /* Pointer to malloc'd buffer */
    int nBuffer;                    /* Valid bytes of data in zBuffer */
    sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */
};

typedef struct VfsFile VfsFile;

static int directWrite(
        VfsFile *p,                    /* File handle */
        const void *zBuf,               /* Buffer containing data to write */
        int iAmt,                       /* Size of data to write in bytes */
        sqlite_int64 iOfst              /* File offset to write to */
) {
    off_t ofst;                     /* Return value from lseek() */
    size_t nWrite;                  /* Return value from write() */

    auto env = getEnv();
    ofst = vfsio_seek_whence(env, p->vfsIo, iOfst, SEEK_SET);
    if (ofst != iOfst) {
        return SQLITE_IOERR_WRITE;
    }

    nWrite = vfsio_pwrite(env, p->vfsIo, (uint8_t *) zBuf, iAmt, iOfst);
    if (nWrite != iAmt) {
        return SQLITE_IOERR_WRITE;
    }

    return SQLITE_OK;
}

static int flushBuffer(VfsFile *p) {
    int rc = SQLITE_OK;
    if (p->nBuffer) {
        rc = directWrite(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
        p->nBuffer = 0;
    }
    return rc;
}

static int vfsClose(sqlite3_file *pFile) {
    int rc;
    auto *p = (VfsFile*)pFile;
    rc = flushBuffer(p);
    sqlite3_free(p->aBuffer);
    auto env = getEnv();
    vfsio_close(env, p->vfsIo);
    env->DeleteGlobalRef(p->vfsIo);
    return rc;
}

static int vfsWrite(
        sqlite3_file *pFile,
        const void *zBuf,
        int iAmt,
        sqlite_int64 iOfst
) {

    auto *p = (VfsFile *) pFile;

    if (p->aBuffer) {
        char *z = (char *) zBuf;       /* Pointer to remaining data to write */
        int n = iAmt;                 /* Number of bytes at z */
        sqlite3_int64 i = iOfst;      /* File offset to write to */

        while (n > 0) {
            int nCopy;                  /* Number of bytes to copy into buffer */

            /* If the buffer is full, or if this data is not being written directly
            ** following the data already buffered, flush the buffer. Flushing
            ** the buffer is a no-op if it is empty.
            */
            if (p->nBuffer == SQLITE_VFS_BUFFERSZ || p->iBufferOfst + p->nBuffer != i) {
                int rc = flushBuffer(p);
                if (rc != SQLITE_OK) {
                    return rc;
                }
            }
            assert(p->nBuffer == 0 || p->iBufferOfst + p->nBuffer == i);
            p->iBufferOfst = i - p->nBuffer;

            /* Copy as much data as possible into the buffer. */
            nCopy = SQLITE_VFS_BUFFERSZ - p->nBuffer;
            if (nCopy > n) {
                nCopy = n;
            }
            memcpy(&p->aBuffer[p->nBuffer], z, nCopy);
            p->nBuffer += nCopy;

            n -= nCopy;
            i += nCopy;
            z += nCopy;
        }
    } else {
        return directWrite(p, zBuf, iAmt, iOfst);
    }

    return SQLITE_OK;
}


static int vfsRead(
        sqlite3_file *pFile,
        void *zBuf,
        int iAmt,
        sqlite_int64 iOfst
) {
    auto p = (VfsFile *) pFile;
    off_t ofst;                     /* Return value from lseek() */
    int nRead;                      /* Return value from read() */
    int rc;                         /* Return code from demoFlushBuffer() */

    rc = flushBuffer(p);
    if (rc != SQLITE_OK) {
        return rc;
    }
    auto env = getEnv();
    ofst = vfsio_seek_whence(env, p->vfsIo, iOfst, SEEK_SET);
    if (ofst != iOfst) {
        return SQLITE_IOERR_READ;
    }
    nRead = vfsio_pread(env, p->vfsIo, (uint8_t *) zBuf, iAmt, ofst);

    if (nRead == iAmt) {
        return SQLITE_OK;
    } else if (nRead >= 0) {
        if (nRead < iAmt) {
            memset(&((char *) zBuf)[nRead], 0, iAmt - nRead);
        }
        return SQLITE_IOERR_SHORT_READ;
    }

    return SQLITE_IOERR_READ;
}

static int vfsTruncate(sqlite3_file *pFile, sqlite_int64 size) {
    auto *p = (VfsFile *) pFile;
    int rc = vfsio_truncate(getEnv(), p->vfsIo, size);
    if (rc == -1)
        return SQLITE_IOERR_TRUNCATE;
    return SQLITE_OK;
}

static int vfsSync(sqlite3_file *pFile, int flags) {
    auto *p = (VfsFile *) pFile;
    int rc;

    rc = flushBuffer(p);
    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = vfsio_fsync(getEnv(), p->vfsIo);
    return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC);
}

static int vfsFileSize(sqlite3_file *pFile, sqlite_int64 *pSize) {
    auto *p = (VfsFile *) pFile;
    int rc;                         /* Return code from fstat() call */

    /* Flush the contents of the buffer to disk. As with the flush in the
    ** demoRead() method, it would be possible to avoid this and save a write
    ** here and there. But in practice this comes up so infrequently it is
    ** not worth the trouble.
    */
    rc = flushBuffer(p);
    if (rc != SQLITE_OK) {
        return rc;
    }


    auto size = vfsio_get_size(getEnv(), p->vfsIo);
    if (size == -1)
        return SQLITE_IOERR_FSTAT;
    *pSize = size;

    return SQLITE_OK;
}

static int vfsLock(sqlite3_file *pFile, int eLock) {
    return SQLITE_OK;
}

static int vfsUnlock(sqlite3_file *pFile, int eLock) {
    return SQLITE_OK;
}

static int vfsCheckReservedLock(sqlite3_file *pFile, int *pResOut) {
    *pResOut = 0;
    return SQLITE_OK;
}

static int vfsFileControl(sqlite3_file *pFile, int op, void *pArg) {
    return SQLITE_NOTFOUND;
}

static int vfsSectorSize(sqlite3_file *pFile) {
    return 0;
}

static int vfsDeviceCharacteristics(sqlite3_file *pFile) {
    return 0;
}


static int vfsOpen(
        sqlite3_vfs *pVfs,              /* VFS */
        const char *zName,              /* File to open, or 0 for a temp file */
        sqlite3_file *pFile,            /* Pointer to DemoFile struct to populate */
        int flags,                      /* Input SQLITE_OPEN_XXX flags */
        int *pOutFlags                  /* Output SQLITE_OPEN_XXX flags (or NULL) */
) {

    static const sqlite3_io_methods sqlite3_io_methods = {
            1,                            /* iVersion */
            vfsClose,                    /* xClose */
            vfsRead,                     /* xRead */
            vfsWrite,                    /* xWrite */
            vfsTruncate,                 /* xTruncate */
            vfsSync,                     /* xSync */
            vfsFileSize,                 /* xFileSize */
            vfsLock,                     /* xLock */
            vfsUnlock,                   /* xUnlock */
            vfsCheckReservedLock,        /* xCheckReservedLock */
            vfsFileControl,              /* xFileControl */
            vfsSectorSize,               /* xSectorSize */
            vfsDeviceCharacteristics     /* xDeviceCharacteristics */
    };

    auto p = (VfsFile *) pFile; /* Populate this structure */
    int oflags = 0;                 /* flags to pass to open() call */
    char *aBuf = nullptr;

    if (zName == nullptr) {
        return SQLITE_IOERR;
    }

    if (flags & SQLITE_OPEN_MAIN_JOURNAL) {
        aBuf = (char *) sqlite3_malloc(SQLITE_VFS_BUFFERSZ);
        if (!aBuf) {
            return SQLITE_NOMEM;
        }
    }

    if (flags & SQLITE_OPEN_EXCLUSIVE) oflags |= O_EXCL;
    if (flags & SQLITE_OPEN_CREATE) oflags |= O_CREAT;
    if (flags & SQLITE_OPEN_READONLY) oflags |= O_RDONLY;
    if (flags & SQLITE_OPEN_READWRITE) oflags |= O_RDWR;

    memset(p, 0, sizeof(VfsFile));

    auto env = getEnv();
    auto vfsIo = vfsio_open(env, vfs, zName);
    if (vfsIo == nullptr) {
        sqlite3_free(aBuf);
        return SQLITE_CANTOPEN;
    }
    auto vfsIoGlobal = env->NewGlobalRef(vfsIo);
    p->vfsIo = vfsIoGlobal;
    p->aBuffer = aBuf;

    if (pOutFlags) {
        *pOutFlags = flags;
    }
    p->base.pMethods = &sqlite3_io_methods;
    return SQLITE_OK;
}

static int vfsDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync) {
    int rc;                         /* Return code */

    rc = vfsio_delete(getEnv(), vfs, zPath);
    if (rc != 0 && errno == ENOENT) return SQLITE_OK;

    if (rc == 0 && dirSync) {
        return SQLITE_IOERR_DELETE;
    }
    return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_DELETE);
}

static int vfsAccess(
        sqlite3_vfs *pVfs,
        const char *zPath,
        int flags,
        int *pResOut
) {
    int eAccess = F_OK;             /* Second argument to access() */

    assert(flags == SQLITE_ACCESS_EXISTS       /* access(zPath, F_OK) */
           || flags == SQLITE_ACCESS_READ         /* access(zPath, R_OK) */
           || flags == SQLITE_ACCESS_READWRITE    /* access(zPath, R_OK|W_OK) */
    );

    if (flags == SQLITE_ACCESS_READWRITE) eAccess = R_OK | W_OK;
    if (flags == SQLITE_ACCESS_READ) eAccess = R_OK;

    *pResOut = vfsio_access(getEnv(), vfs, zPath, eAccess);
    return SQLITE_OK;
}

static int vfsFullPathname(
        sqlite3_vfs *pVfs,              /* VFS */
        const char *zPath,              /* Input path (possibly a relative path) */
        int nPathOut,                   /* Size of output buffer in bytes */
        char *zPathOut                  /* Pointer to output buffer */
) {
    char zDir[MAXPATHNAME + 1];
    if (zPath[0] == '/') {
        zDir[0] = '\0';
    } else {
        if (getcwd(zDir, sizeof(zDir)) == nullptr) return SQLITE_IOERR;
    }
    zDir[MAXPATHNAME] = '\0';

    sqlite3_snprintf(nPathOut, zPathOut, "%s", zPath);
    zPathOut[nPathOut - 1] = '\0';

    return SQLITE_OK;
}

static void *vfsDlOpen(sqlite3_vfs *pVfs, const char *zPath) {
    return nullptr;
}

static void vfsDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg) {
    sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not supported");
    zErrMsg[nByte - 1] = '\0';
}

static void (*vfsDlSym(sqlite3_vfs *pVfs, void *pH, const char *z))(void){
    return nullptr;
}

static void vfsDlClose(sqlite3_vfs *pVfs, void *pHandle) {
}

static int vfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte) {
    return SQLITE_OK;
}

static int vfsSleep(sqlite3_vfs *pVfs, int nMicro) {
    sleep(nMicro / 1000000);
    usleep(nMicro % 1000000);
    return nMicro;
}

static int sarchCurrentTime(sqlite3_vfs *pVfs, double *pTime) {
    time_t t = time(nullptr);
    *pTime = t / 86400.0 + 2440587.5;
    return SQLITE_OK;
}

int registerVirtualFileSystem(jobject virtualFileSystem) {
    vfs = virtualFileSystem;

    static sqlite3_vfs sqlite3_vfs = {
            1,                            /* iVersion */
            sizeof(VfsFile),             /* szOsFile */
            MAXPATHNAME,                  /* mxPathname */
            nullptr,                            /* pNext */
            "sqlite-vfs",                       /* zName */
            nullptr,                            /* pAppData */
            vfsOpen,                     /* xOpen */
            vfsDelete,                   /* xDelete */
            vfsAccess,                   /* xAccess */
            vfsFullPathname,             /* xFullPathname */
            vfsDlOpen,                   /* xDlOpen */
            vfsDlError,                  /* xDlError */
            vfsDlSym,                    /* xDlSym */
            vfsDlClose,                  /* xDlClose */
            vfsRandomness,               /* xRandomness */
            vfsSleep,                    /* xSleep */
            sarchCurrentTime,              /* xCurrentTime */
    };

    return sqlite3_vfs_register(&sqlite3_vfs, 1);
}

int register_android_database_SQLiteVirtualFileSystem(JNIEnv *env) {
    return init_vfsio(env);
}


#ifdef __cplusplus
}
#endif