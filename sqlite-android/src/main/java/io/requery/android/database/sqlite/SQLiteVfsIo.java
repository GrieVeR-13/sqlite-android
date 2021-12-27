package io.requery.android.database.sqlite;

public interface SQLiteVfsIo {

    int read(byte[] buffer, int bufferOffset, int count);

    void write(byte[] buffer, int bufferOffset, int count);

    void flush();

    void close();

    long getPosition();

    void setPosition(long position);

    long getLength();

    void truncate();

    static int pread(SQLiteVfsIo io, byte[] buf, int bufOffset, int count, long position) {
        long cur = io.getPosition();
        io.setPosition(position);
        try {
            int res = 0;
            int tmp;
            while (res < count) {
                tmp = io.read(buf, bufOffset + res, count - res);
                if (tmp > 0) res += tmp;
                else break;
            }
            return res;
        } finally {
            io.setPosition(cur);
        }
    }

    static int pwrite(SQLiteVfsIo io, byte[] buf, int bufOffset, int count, long position) {
        long cur = io.getPosition();
        io.setPosition(position);
        try {
            io.write(buf, bufOffset, count);
            return count;
        } finally {
            io.setPosition(cur);
        }
    }
}