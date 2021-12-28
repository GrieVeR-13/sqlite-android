cmake_minimum_required(VERSION 3.6)
project(sqlite3x VERSION 1.0.0 LANGUAGES C CXX)

add_library(sqlite3x SHARED
    android_database_SQLiteCommon.cpp
    android_database_SQLiteConnection.cpp
    android_database_SQLiteFunction.cpp
    android_database_SQLiteGlobal.cpp
    android_database_SQLiteDebug.cpp
    android_database_CursorWindow.cpp
    android_database_SQLiteVirtualFileSytem.cpp
    cutil/jniutil.c
    cutil/vfsio.c
    CursorWindow.cpp
    JNIHelp.cpp
    JNIString.cpp
    sqlite3.c
)

target_include_directories(sqlite3x PUBLIC include)


target_compile_definitions(sqlite3x
        PRIVATE
        SQLITE_TEMP_STORE=3
        HAVE_CONFIG_H
        KHTML_NO_EXCEPTIONS
        GKWQ_NO_JAVA
        NO_SUPPORT_JS_BINDING
        QT_NO_WHEELEVENT
        KHTML_NO_XBL
        HAVE_STRCHRNUL=0
        SQLITE_USE_URI=1
        HAVE_USLEEP=1
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_RTREE
        SQLITE_ENABLE_JSON1
        SQLITE_ENABLE_FTS3
        DSQLITE_ENABLE_FTS3_PARENTHESIS
        SQLITE_ENABLE_BATCH_ATOMIC_WRITE
        )

target_compile_options(sqlite3x
        PRIVATE
        -Wno-unused-parameter
        -Wno-int-to-pointer-cast
        -Wno-uninitialized
        -Wno-parentheses
        -Wno-conversion-null
        )

target_include_directories(sqlite3x
        PRIVATE
        ../sqlite
        )

target_link_libraries(sqlite3x
        dl
        log
        )