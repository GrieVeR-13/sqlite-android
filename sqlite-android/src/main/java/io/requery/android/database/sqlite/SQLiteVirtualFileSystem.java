package io.requery.android.database.sqlite;

public interface SQLiteVirtualFileSystem {

    SQLiteVfsIo open(String path);

    void delete(String path);

    /**
     * @return if mode == 0 && file exists and file size > 0 return 1
     * if mode != 0 && file exists return 1
     * otherwise return 0
     */
    int access(String path, int mode);

}
