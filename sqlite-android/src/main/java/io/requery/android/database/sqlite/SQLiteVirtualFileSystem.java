package io.requery.android.database.sqlite;

public interface SQLiteVirtualFileSystem {

    SQLiteVfsIo open(String path);

    void delete(String path);

    int access(String path, int mode);

}
