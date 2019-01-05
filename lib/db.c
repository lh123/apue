#include "apue_db.h"
#include <apue.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/stat.h>

#define IDXLEN_SZ 4
#define SEP ':'
#define SPACE ' '
#define NEWLINE '\n'

#define PTR_SZ 7        // size of ptr field int hash chain
#define PTR_MAX 999999  // max file offset = 10**PTR_SZ - 1
#define NHASH_DEF 137   //default hash table size
#define FREE_OFF 0      // free list offset in index file
#define HASH_OFF PTR_SZ // hash table offset in index file

typedef unsigned long DBHASH;
typedef unsigned long COUNT;

typedef struct {
    int idxfd;
    int datfd;
    char *idxbuf;
    char *datbuf;
    char *name;
    off_t idxoff;   // offset in index file of index record
                    // key is at (idxoff + PTR_SZ + IDXLEN_SZ)
    size_t idxlen;  // length of index record
    off_t datoff;   // offset in data file of data record
    size_t datlen;  // lenght of data record
                    // include newline at end
    off_t ptrval;   // contents of chain ptr in index record
    off_t ptroff;   // chain ptr offset pointing to this idx record
    off_t chainoff; // offset of hash chain for this index record
    off_t hashoff;  // offset in index file of hash table
    DBHASH nhash;   // current hash table size
    COUNT cnt_delok;
    COUNT cnt_delerr;
    COUNT cnt_fetchok;
    COUNT cnt_fetcherr;
    COUNT cnt_nextrec;
    COUNT cnt_stor1;
    COUNT cnt_stor2;
    COUNT cnt_stor3;
    COUNT cnt_stor4;
    COUNT cnt_storerr;
} DB;

static DB *_db_alloc(int);
static void _db_dodelte(DB *);
static int _db_find_and_lock(DB *, const char *, int);
static int _db_findfree(DB *, int, int);
static void _db_free(DB *);
static DBHASH _db_hash(DB *, const char *);
static char *_db_readdat(DB *);
static off_t _db_readidx(DB *, off_t);
static off_t _db_readptr(DB *, off_t);
static void _db_writedat(DB *, const char *, off_t, int);
static void _db_writeidx(DB *, const char *, off_t, int, off_t);
static void _db_writeptr(DB *, off_t, off_t);

DBHANDLE db_open(const char *pathname, int oflag, ...) {
    int len = strlen(pathname);
    DB *db = _db_alloc(len);
    if (db == NULL) {
        err_dump("db_openL _db_alloc error for DB");
    }
    db->nhash = NHASH_DEF;
    db->hashoff = HASH_OFF;
    strcpy(db->name, pathname);
    strcat(db->name, ".idx");

    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        int mode = va_arg(ap, int);
        va_end(ap);

        db->idxfd = open(db->name, oflag, mode);
        strcpy(db->name + len, ".dat");
        db->datfd = open(db->name, oflag, mode);
    } else {
        db->idxfd = open(db->name, oflag);
        strcpy(db->name + len, ".dat");
        db->datfd = open(db->name, oflag);
    }

    if (db->idxfd < 0 || db->datfd < 0) {
        _db_free(db);
        return NULL;
    }
    if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) {
        if (writew_lock(db->idxfd, 0, SEEK_SET, 0) < 0) {
            err_dump("db_open: writew_lock error");
        }
        struct stat statbuf;
        if (fstat(db->idxfd, &statbuf) < 0) {
            err_sys("db_open: fstat error");
        }
        if (statbuf.st_size == 0) {
            char asciiptr[PTR_SZ + 1];
            sprintf(asciiptr, "%*d", PTR_SZ, 0);
            char hash[(NHASH_DEF + 1) * PTR_SZ + 2];
            hash[0] = 0;
            int i;
            for (i = 0; i < NHASH_DEF + 1; i++) {
                strcat(hash, asciiptr);
            }
            strcat(hash, "\n");
            i = strlen(hash);
            if (write(db->idxfd, hash, i) != i) {
                err_dump("db_open: index file init write error");
            }
        }
        if (un_lock(db->idxfd, 0, SEEK_SET, 0) < 0) {
            err_dump("db_open: un_lock error");
        }
    }
    db_rewind(db);
    return db;
}

static DB *_db_alloc(int namelen) {
    DB *db = calloc(1, sizeof(DB));
    if (db == NULL) {
        err_dump("_db_alloc: calloc error for DB");
    }
    db->idxfd = db->datfd = -1;
    
    // +5 for ".idx" or ".dat" plus null at end
    if ((db->name = malloc(namelen + 5)) == NULL) {
        err_dump("_db_alloc: malloc error for name");
    }

    // +2 for newline and null at end
    if ((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL) {
        err_dump("_db_alloc: malloc error for index buffer");
    }
    if ((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL) {
        err_dump("_db_alloc: malloc error for data buffer");
    }
    return db;
}

void db_close(DBHANDLE h) {
    _db_free((DB *)h);
}

static void _db_free(DB *db) {
    if (db->idxfd >= 0) {
        close(db->idxfd);
    }
    if (db->datfd >= 0) {
        close(db->datfd);
    }
    if (db->idxbuf != NULL) {
        free(db->idxbuf);
    }
    if (db->datbuf != NULL) {
        free(db->datbuf);
    }
    if (db->name != NULL) {
        free(db->name);
    }
    free(db);
}

char *db_fetch(DBHANDLE h, const char *key) {
    DB *db = h;
    char *ptr;
    if (_db_find_and_lock(db, key, 0) < 0) {
        ptr = NULL;
        db->cnt_fetcherr++;
    } else {
        ptr = _db_readdat(db);
        db->cnt_fetchok++;
    }

    if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0) {
        err_dump("db_fetch: un_lock error");
    }
    return ptr;
}

static int _db_find_and_lock(DB *db, const char *key, int writelock) {
    // + db->hashoff 跳过空闲链表指针
    db->chainoff = (_db_hash(db, key) * PTR_SZ) + db->hashoff;
    db->ptroff = db->chainoff;

    if (writelock) {
        if (writew_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0) {
            err_dump("_db_find_and_lock: writew_lock error");
        }
    } else {
        if (readw_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0) {
            err_dump("_db_find_and_lock: readw_lock error");
        }
    }
    off_t offset = _db_readptr(db, db->ptroff);
    while (offset != 0) {
        off_t nextoffset = _db_readidx(db, offset);
        if (strcmp(db->idxbuf, key) == 0) {
            break;
        }
        db->ptroff = offset;
        offset = nextoffset;
    }

    return offset == 0 ? -1 : 0;
}

static DBHASH _db_hash(DB *db, const char *key) {
    DBHASH hval = 0;
    int i;
    while (*key != 0) {
        char c = *key;
        hval += c * i;
        key++;
        i++;
    }
    return hval % db->nhash;
}

static off_t _db_readptr(DB *db, off_t offset) {
    if (lseek(db->idxfd, offset, SEEK_SET) == -1) {
        err_dump("_db_readptr: lseek error to ptr field");
    }
    char asciiptr[PTR_SZ + 1];
    if (read(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ) {
        err_dump("_db_readptr: read error of ptr field");
    }
    asciiptr[PTR_SZ] = 0;
    return atol(asciiptr);
}

static off_t _db_readidx(DB *db, off_t offset) {
    if ((db->idxoff = lseek(db->idxfd, offset, offset == 0 ? SEEK_CUR : SEEK_SET)) == -1) {
        err_dump("_db_readidx: lseek error");
    }

    char asciiptr[PTR_SZ + 1], asciilen[IDXLEN_SZ + 1];
    struct iovec iov[2];
    iov[0].iov_base = asciiptr;
    iov[0].iov_len = PTR_SZ;
    iov[1].iov_base = asciilen;
    iov[1].iov_len = IDXLEN_SZ;

    int i = readv(db->idxfd, &iov[0], 2);
    if (i != PTR_SZ + IDXLEN_SZ) {
        if (i == 0 && offset == 0) {
            return -1;
        }
        err_dump("_db_readidx: readv error of index record");
    }

    asciiptr[PTR_SZ] = 0;
    db->ptrval = atol(asciiptr);
    asciilen[IDXLEN_SZ] = 0;
    if ((db->idxlen = atoi(asciilen)) < IDXLEN_MIN || db->idxlen > IDXLEN_MAX) {
        err_dump("_db_readidx: invalid length");
    }

    i = read(db->idxfd, db->idxbuf, db->idxlen);
    if (i != db->idxlen) {
        err_dump("_db_readidx: read error of index record");
    }
    if (db->idxbuf[db->idxlen - 1] != NEWLINE) {
        err_dump("_db_readidx: missing newline");
    }
    db->idxbuf[db->idxlen - 1] = 0;

    char *ptr1 = strchr(db->idxbuf, SEP);
    if (ptr1 == NULL) {
        err_dump("_db_readidx: missing  first separator");
    }
    *ptr1++ = 0;
    char *ptr2 = strchr(ptr1, SEP);
    if (ptr2 == NULL) {
        err_dump("_db_readidx: missing second separator");
    }
    *ptr2++ = 0;
    if (strchr(ptr2, SEP) != NULL) {
        err_dump("_db_readidx: too many separator");
    }

    if ((db->datoff = atol(ptr1)) < 0) {
        err_dump("_db_readidx: starting offset < 0");
    }
    if ((db->datlen = atol(ptr2)) <= 0 || db->datlen > DATLEN_MAX) {
        err_dump("_db_readidx: invalid length");
    }
    return db->ptrval;
}

static char *_db_readdat(DB *db) {
    if (lseek(db->datfd, db->datoff, SEEK_SET) == -1) {
        err_dump("_db_readdat: lseek error");
    }
    if (read(db->datfd, db->datbuf, db->datlen) != db->datlen) {
        err_dump("_db_readdat: read error");
    }
    if (db->datbuf[db->datlen - 1] != NEWLINE) {
        err_dump("_db_readdat: missing newline");
    }
    db->datbuf[db->datlen - 1] = 0;
    return db->datbuf;
}

int db_delete(DBHANDLE h, const char *key) {
    DB *db = h;
    int rc = 0;
    if (_db_find_and_lock(db, key, 1) == 0) {
        _db_dodelte(db);
    } else {
        rc = -1;
        db->cnt_delerr++;
    }
    if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0) {
        err_dump("db_delete: un_lock error");
    }
    return rc;
}

static void _db_dodelte(DB *db) {
    char *ptr;
    int i;
    for (ptr = db->datbuf, i = 0; i < db->datlen - 1; i++) {
        *ptr++ = SPACE;
    }
    *ptr = 0;
    ptr = db->idxbuf;
    while (*ptr) {
        *ptr++ = SPACE;
    }

    if (writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("_db_dodelte: writew_lock error");
    }

    _db_writedat(db, db->datbuf, db->datoff, SEEK_SET);
    off_t freeptr = _db_readptr(db, FREE_OFF);
    off_t saveptr = db->ptrval;

    _db_writeidx(db, db->idxbuf, db->idxoff, SEEK_SET, freeptr);
    _db_writeptr(db, FREE_OFF, db->idxoff);
    _db_writeptr(db, db->ptroff, saveptr);
    if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("_db_dodelte: un_lock error");
    }
}

static void _db_writedat(DB *db, const char *data, off_t offset, int whence) {
    static char newline = NEWLINE;
    if (whence == SEEK_SET) {
        if (writew_lock(db->datfd, 0, SEEK_SET, 0) < 0) {
            err_dump("_db_writedat: writew_lock error");
        }
    }

    if ((db->datoff = lseek(db->datfd, offset, whence)) == -1) {
        err_dump("_db_writedat: lseek error");
    }
    db->datlen = strlen(data) + 1;

    struct iovec iov[2];
    iov[0].iov_base = (char *)data;
    iov[0].iov_len = db->datlen - 1;
    iov[1].iov_base = &newline;
    iov[1].iov_len = 1;
    if (writev(db->datfd, &iov[0], 2) != db->datlen) {
        err_dump("_db_writedat: writev error of data record");
    }

    if (whence == SEEK_SET) {
        if (un_lock(db->datfd, 0, SEEK_SET, 0) < 0) {
            err_dump("_db_writedat: un_lock error");
        }
    }
}

static void _db_writeidx(DB *db, const char *key, off_t offset, int whence, off_t ptrval) {
    if ((db->ptrval = ptrval) < 0 || ptrval > PTR_MAX) {
        err_quit("_db_writeidx: invalid ptr: %ld", ptrval);
    }
    char asciiptrlen[PTR_SZ + IDXLEN_SZ + 1];
    sprintf(db->idxbuf, "%s%c%lld%c%ld\n", key, SEP, (long long)db->datoff, SEP, (long)db->datlen);
    int len = strlen(db->idxbuf);
    if (len < IDXLEN_MIN || len > IDXLEN_MAX) {
        err_dump("_db_writeidx: invalid length");
    }
    sprintf(asciiptrlen, "%*lld%*d", PTR_SZ, (long long)ptrval, IDXLEN_SZ, len);
    if (whence == SEEK_END) {
        if (writew_lock(db->idxfd, ((db->nhash + 1) * PTR_SZ) + 1, SEEK_SET, 0) < 0) {
            err_dump("_db_writeidx: writew_lock error");
        }
    }

    if ((db->idxoff = lseek(db->idxfd, offset, whence)) == -1) {
        err_dump("_db_writeidx: lseek error");
    }

    struct iovec iov[2];
    iov[0].iov_base = asciiptrlen;
    iov[0].iov_len = PTR_SZ + IDXLEN_SZ;
    iov[1].iov_base = db->idxbuf;
    iov[1].iov_len = len;
    if (writev(db->idxfd, &iov[0], 2) != PTR_SZ + IDXLEN_SZ + len) {
        err_dump("_db_writeidx: writev error of index record");
    }
    if (whence == SEEK_END) {
        if (un_lock(db->idxfd, ((db->nhash + 1) * PTR_SZ) + 1, SEEK_SET, 0) < 0) {
            err_dump("_db_writeidx: un_lock error");
        }
    }
}

static void _db_writeptr(DB *db, off_t offset, off_t ptrval) {
    if (ptrval < 0 || ptrval > PTR_MAX) {
        err_quit("_db_writeptr: invalid ptr: %d", (int)ptrval);
    }
    char asciiptr[PTR_SZ + 1];
    sprintf(asciiptr, "%*lld", PTR_SZ, (long long)ptrval);

    if (lseek(db->idxfd, offset, SEEK_SET) == -1) {
        err_dump("_db_writeptr: lseek error to ptr field");
    }
    if (write(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ) {
        err_dump("_db_writeptr: write error of ptr field");
    }
}

int db_store(DBHANDLE h, const char *key, const char *data, int flag) {
    DB *db = h;

    if (flag != DB_INSERT && flag != DB_REPLACE && flag != DB_STORE) {
        errno = EINVAL;
        return -1;
    }

    int keylen = strlen(key);
    int datlen = strlen(data) + 1;
    if (datlen < DATLEN_MIN || datlen > DATLEN_MAX) {
        err_dump("db_store: invalid data length");
    }
    int rc;
    if (_db_find_and_lock(db, key, 1) < 0) {
        if (flag == DB_REPLACE) {
            rc = -1;
            db->cnt_storerr++;
            errno = ENOENT;
            goto doreturn;
        }
        off_t ptrval = _db_readptr(db, db->chainoff);
        if (_db_findfree(db, keylen, datlen) < 0) {
            _db_writedat(db, data, 0, SEEK_END);
            _db_writeidx(db, key, 0, SEEK_END, ptrval);

            _db_writeptr(db, db->chainoff, db->idxoff);
            db->cnt_stor1++;
        } else {
            _db_writedat(db, data, db->datoff, SEEK_SET);
            _db_writeidx(db, key, db->idxoff, SEEK_SET, ptrval);
            _db_writeptr(db, db->chainoff, db->idxoff);
            db->cnt_stor2++;
        }
    } else {
        if (flag == DB_INSERT) {
            rc = 1;
            db->cnt_storerr++;
            goto doreturn;
        }

        if (datlen != db->datlen) {
            _db_dodelte(db);
            off_t ptrval = _db_readptr(db, db->chainoff);
            _db_writedat(db, data, 0, SEEK_END);
            _db_writeidx(db, key, 0, SEEK_END, ptrval);
            _db_writeptr(db, db->chainoff, db->idxoff);
            db->cnt_stor3++;
        } else {
            _db_writedat(db, data, db->datoff, SEEK_SET);
            db->cnt_stor4++;
        }
    }
    rc = 0;
doreturn:
    if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0) {
        err_dump("db_store: un_lock error");
    }

    return rc;
}

static int _db_findfree(DB *db, int keylen, int datlen) {
    if (writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("_db_findfree: writew_lock error");
    }

    off_t saveoffset = FREE_OFF;
    off_t offset = _db_readptr(db, saveoffset);
    while (offset != 0) {
        off_t nextoffset = _db_readidx(db, offset);
        if (strlen(db->idxbuf) == keylen && db->datlen == datlen) {
            break;
        }
        saveoffset = offset;
        offset = nextoffset;
    }

    int rc;
    if (offset == 0) {
        rc = -1;
    } else {
        _db_writeptr(db, saveoffset, db->ptrval);
        rc = 0;
    }

    if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("_db_findfree: unlock error");
    }
    return rc;
}

void db_rewind(DBHANDLE h) {
    DB *db = h;

    off_t offset = (db->nhash + 1) * PTR_SZ;
    if ((db->idxoff = lseek(db->idxfd, offset + 1, SEEK_SET)) == -1) {
        err_dump("db_rewind: lseek error");
    }
}

char *db_nextrec(DBHANDLE h, char *key) {
    DB *db = h;

    if (readw_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("db_nextrec: readw_lock error");
    }

    char *ptr;
    char c;
    do {
        if (_db_readidx(db, 0) < 0) {
            ptr = NULL;
            goto doreturn;
        }

        ptr = db->idxbuf;
        
        while ((c = *ptr++) != 0 && c == SPACE);
    } while (c == 0);

    if (key != NULL) {
        strcpy(key, db->idxbuf);
    }
    ptr = _db_readdat(db);
    db->cnt_nextrec++;

doreturn:
    if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0) {
        err_dump("db_nextrec: un_lock error");
    }
    return ptr;
}
