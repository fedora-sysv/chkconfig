#ifndef H_LEVELDB
#define H_LEVELDB

#define LEVEL_DATABASE "./chkconfig.db"
#define NEW_LEVEL_DATABASE "./.chkconfig.new"

struct service {
    char * name;
    int levels, kPriority, sPriority;
    char * desc;
};

int parseLevels(char * str, int emptyOk);
struct service * findService(struct service * list, char * name);
/* returns filedescriptor */
int ldbLock(int rw);
int ldbUnlock(int fd);
/* last service has name NULL, list must be freed */
int ldbRead(int fd, struct service ** list, int * numServices);
int ldbWrite(int * fd, struct service * list);

#endif
