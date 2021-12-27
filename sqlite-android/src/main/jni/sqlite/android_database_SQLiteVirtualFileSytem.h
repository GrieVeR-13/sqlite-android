#ifndef SQLITE_VFS_H
#define SQLITE_VFS_H

#ifdef __cplusplus
extern "C" {
#endif

int register_android_database_SQLiteVirtualFileSystem(JNIEnv *env);
int registerVirtualFileSystem(jobject virtualFileSystem);

#ifdef __cplusplus
}
#endif

#endif //SQLITE_VFS_H
