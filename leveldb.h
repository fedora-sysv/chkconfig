#ifndef H_LEVELDB
#define H_LEVELDB

#define RUNLEVELS "/etc/rc.d"

#include <glob.h>

struct service {
    char * name;
    int levels, kPriority, sPriority;
    char * desc;
};

int parseLevels(char * str, int emptyOk);

/* returns 0 on success, 1 if the service is not chkconfig-able, -1 if an
   I/O error occurs (in which case errno can be checked) */
int readServiceInfo(char * name, struct service * service, int honorHide);
int currentRunlevel(void);
int isOn(char * name, int where);
int isConfigured(char * name, int level);
int doSetService(struct service s, int level, int on);
int findServiceEntries(char * name, int level, glob_t * globresptr);

#endif
