#if !defined(_APUE_DB_H)
#define _APUE_DB_H

typedef void *DBHANDLE;

DBHANDLE db_open(const char *pathname, int oflag, ...);
void db_close(DBHANDLE h);
char *db_fetch(DBHANDLE h, const char *key);
int db_store(DBHANDLE h, const char *key, const char *data, int flag);
int db_delete(DBHANDLE h, const char *key);
void db_rewind(DBHANDLE h);
char *db_nextrec(DBHANDLE h, char *key);

#define DB_INSERT 1
#define DB_REPLACE 2
#define DB_STORE 3

#define IDXLEN_MIN 6 // key, sep, start, sep, length, \n
#define IDXLEN_MAX 1024

#define DATLEN_MIN 2 // data byte, newline
#define DATLEN_MAX 1024

#endif