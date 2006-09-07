/* Copyright 1997-2006 Red Hat, Inc.
 *
 * This software may be freely redistributed under the terms of the GNU
 * public license.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef H_LEVELDB
#define H_LEVELDB

#define RUNLEVELS "/etc"
#define XINETDDIR "/etc/xinetd.d"

#include <glob.h>

#define TYPE_INIT_D	0
#define TYPE_XINETD	1

struct service {
    char * name;
    int levels, kPriority, sPriority;
    char * desc;
    char **startDeps;
    char **stopDeps;
    char **provides;
    int type;
    int isLSB;
    int enabled;
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
int readXinetdServiceInfo(char *name, struct service *service, int honorHide);
int setXinetdService(struct service s, int on);

#endif
