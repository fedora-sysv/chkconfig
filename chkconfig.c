#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "leveldb.h"

#define RUNLEVELS "/etc/rc.d"

static void usage(void) {
    fprintf(stderr, "chkconfig version " VERSION 
			" - Copyright (C) 1997 Red Hat Software\n");
    fprintf(stderr, "This may be freely redistributed under the terms of "
			"the GNU Public License.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "usage:   chkconfig --list [name]\n");
    fprintf(stderr, "         chkconfig --add <name> <levels> <startpri> <killpri> <desc>\n");
    fprintf(stderr, "         chkconfig --del <name>\n");
    fprintf(stderr, "         chkconfig [--level <levels>] <name> <on|off|reset>\n");

    exit(1);
}

/* returns -1 on error */
static int currentRunlevel(void) {
    FILE * p;
    char response[50];

    p = popen("/sbin/runlevel", "r");
    if (!p) return -1;

    if (!fgets(response, sizeof(response), p)) {
	pclose(p);
	return -1;
    }

    pclose(p);

    if (response[1] != ' ' || !isdigit(response[2]) || response[3] != '\n') 
	return -1;

    return response[2] - '0';
}

int findMatches(char * name, int level, glob_t * globresptr) {
    char match[200];
    glob_t globres;
    int rc;

    sprintf(match, "%s/rc%d.d/*%s*", RUNLEVELS, level, name);

    rc = glob(match, GLOB_ERR | GLOB_NOSORT, NULL, &globres);

    if (rc && rc != GLOB_NOMATCH) {
	fprintf(stderr, "failed to glob pattern %s\n", match);
	return 1;
    } else if (rc == GLOB_NOMATCH) {
	globresptr->gl_pathc = 0;
	return 0;
    }

    *globresptr = globres;
    return 0;
}

static int isConfigured(char * name, int level) {
    glob_t globres;

    if (findMatches(name, level, &globres))
	exit(1);

    if (!globres.gl_pathc)
	return 0;

    globfree(&globres);
    return 1;
}

static int isOn(char * name, int where) {
    glob_t globres;
    int level;

    if (where) {
	for (level = 0; level < 7; level++)
	    if (where & (1 << level)) break;
    } else {
	level = currentRunlevel();
	if (level == -1) {
	    fprintf(stderr, "cannot determine current run level\n");
	    return 0;
	}
    }

    if (findMatches(name, level, &globres))
	exit(1);

    if (!globres.gl_pathc || !strstr(globres.gl_pathv[0], "/S"))
	return 0;

    globfree(&globres);
    return 1;
}

static int doSetService(struct service * s, int level, int on) {
    int priority = on ? s->sPriority : s->kPriority;
    char linkname[200];
    char linkto[200];
    glob_t globres;
    int i;

    if (!findMatches(s->name, level, &globres)) {
	for (i = 0; i < globres.gl_pathc; i++)
	    unlink(globres.gl_pathv[i]);
	if (globres.gl_pathc) globfree(&globres);
    }

    sprintf(linkname, "%s/rc%d.d/%c%d%s", RUNLEVELS, level,
			on ? 'S' : 'K', priority, s->name);
    sprintf(linkto, "../init.d/%s", s->name);

    unlink(linkname);	/* just in case */
    if (symlink(linkto, linkname)) {
	fprintf(stderr, "failed to make symlink %s: %s\n", linkname,
		strerror(errno));
	return 1;
    }

    return 0; 
}

static int delService(char * name) {
    int db;
    struct service * serviceList, * s, * newlist;
    int numServices, level, i;
    glob_t globres;

    if ((db = ldbLock(1)) < 0) {
	fprintf(stderr, "failed to open chkconfig database: %s\n",
		strerror(errno));
	return 1;
    }

    if (ldbRead(db, &serviceList, &numServices)) {
	fprintf(stderr, "failed to read chkconfig database\n");
	return 1;
    }

    s = findService(serviceList, name);
    if (!s) {
	fprintf(stderr, "service %s is not configured in chkconfig\n", name);
	return 1;
    }

    newlist = malloc(sizeof(*newlist) * numServices);
    memcpy(newlist, serviceList, sizeof(*newlist) * (s - serviceList));
    memcpy(newlist + (s - serviceList), serviceList + (s - serviceList + 1), 
		sizeof(*newlist) * (numServices - (s - serviceList) - 1));

    if (ldbWrite(&db, newlist)) {
	fprintf(stderr, "failed to write database: %s\n", strerror(errno));
    }
    ldbUnlock(db);


    for (level = 0; level < 7; level++) {
	if (!findMatches(s->name, level, &globres)) {
	    for (i = 0; i < globres.gl_pathc; i++)
		unlink(globres.gl_pathv[i]);
	    if (globres.gl_pathc) globfree(&globres);
	}
    }

    return 0;
}

static int addService(char * name, int levels, int sPriority, int kPriority,
		      char * desc) {
    int db, i;
    struct service * serviceList, * s;
    int numServices;

    if ((db = ldbLock(1)) < 0) {
	fprintf(stderr, "failed to open chkconfig database: %s\n",
		strerror(errno));
	return 1;
    }

    if (ldbRead(db, &serviceList, &numServices)) {
	fprintf(stderr, "failed to read chkconfig database\n");
	return 1;
    }

    s = findService(serviceList, name);
    if (!s) {
	serviceList = realloc(serviceList, sizeof(*s) * (numServices + 2));
	serviceList[numServices + 1] = serviceList[numServices];
	s = serviceList + numServices;
    }
	
    s->name = name;
    s->levels = levels;
    s->sPriority = sPriority;
    s->kPriority = kPriority;
    s->desc = desc;

    if (ldbWrite(&db, serviceList)) {
	fprintf(stderr, "failed to write database: %s\n", strerror(errno));
    }
    ldbUnlock(db);

    for (i = 0; i < 7; i++) {
	if (!isConfigured(name, i)) {
	    if ((1 << i) & levels)
		doSetService(s, i, 1);
	    else
		doSetService(s, i, 0);
	}
    }

    return 0;
}

static int listService(char * item) {
    int db, where;
    struct service * serviceList, * s;
    int numServices;
    int i;

    if ((db = ldbLock(0)) < 0) {
	fprintf(stderr, "failed to open chkconfig database: %s\n",
		strerror(errno));
	return 1;
    }

    if (ldbRead(db, &serviceList, &numServices)) {
	fprintf(stderr, "failed to read chkconfig database\n");
	return 1;
    }

    if (!item) {
	s = serviceList;
	while (s->name) {
	    printf("%s", s->name);

	    where = 1;
	    for (i = 0; i < 7; i++) {
		printf(" %d:%s", i, isOn(s->name, where) ? "on" : "off");
		where <<= 1;
	    }
	    printf("\n");

	    s++;
	}
    } else {
	s = findService(serviceList, item);
	if (!s) {
	    fprintf(stderr, "unknown service %s\n", item);
	    return 1;
	}

	printf("%s", item);

	where = 1;
	for (i = 0; i < 7; i++) {
	    printf(" %d:%s", i, isOn(s->name, where) ? "on" : "off");
	    where <<= 1;
	}
	printf("\n");
    }

    ldbUnlock(db);

    return 0;
}

int setService(char * name, int where, int state) {
    int i;
    int what;
    int db;
    struct service * serviceList, * s;
    int numServices;
    
    if (!where && state != -1) {
	/* levels 3, 4, 5 */
	where = (1 << 3) | (1 << 4) | (1 << 5);
    } else if (!where) {
	where = (1 << 0) | (1 << 1) | (1 << 2) |
	        (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);
    }

    if ((db = ldbLock(0)) < 0) {
	fprintf(stderr, "failed to open chkconfig database: %s\n",
		strerror(errno));
	return 1;
    }

    if (ldbRead(db, &serviceList, &numServices)) {
	fprintf(stderr, "failed to read chkconfig database\n");
	return 1;
    }

    s = findService(serviceList, name);
    if (!s) {
	fprintf(stderr, "unknown service %s\n", name);
	return 1;
    }

    for (i = 0; i < 7; i++) {
	if (!((1 << i) & where)) continue;

	if (state == 1 || state == 0)
	    what = state;
	else if (s->levels & (1 << i))
	    what = 1;
	else
	    what = 0;

	doSetService(s, i, what);
    }

    return 0;
}

int main(int argc, char ** argv) {
    int listItem = 0, addItem = 0, delItem = 0;
    int rc, i;
    char * levels = NULL;
    poptContext optCon;
    struct poptOption optionsTable[] = {
	    { "add", '\0', 0, &addItem, 0 },
	    { "del", '\0', 0, &delItem, 0 },
	    { "list", '\0', 0, &listItem, 0 },
	    { "level", '\0', POPT_ARG_STRING, &levels, 0 },
	    { 0, 0, 0, 0, 0 } 
    };

    if (argc == 1) usage();

    optCon = poptGetContext("whiptail", argc, argv, optionsTable, 0);
    poptReadDefaultConfig(optCon, 1);

    if ((rc = poptGetNextOpt(optCon)) < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(rc));
	exit(1);
    }

    if ((listItem + addItem + delItem) > 1) {
	fprintf(stderr, "only one of --list, --add, or --del may be "
		"specified\n");
	exit(1);
    }

    if (addItem) {
	char * name = poptGetArg(optCon);
	int levels = parseLevels(poptGetArg(optCon), 0);
	char * startpriStr = poptGetArg(optCon);
	char * killpriStr = poptGetArg(optCon);
	char * desc = poptGetArg(optCon);
	char * end;
	int startPri, killPri;

	if (!desc || levels == -1 || !*name || !*desc || poptGetArg(optCon)) 
	    usage();

	startPri = strtoul(startpriStr, &end, 10);
	if (*end) 
	    usage();
	 
	killPri = strtoul(killpriStr, &end, 10);
	if (*end) 
	    usage();
	 
	
	return addService(name, levels, startPri, killPri, desc);
    } else if (delItem) {
	char * name = poptGetArg(optCon);

	if (!name || poptGetArg(optCon)) usage();

	return delService(name);
    } else if (listItem) {
	char * item = poptGetArg(optCon);

	if (item && poptGetArg(optCon)) usage();

	return listService(item);
    } else {
	char * name = poptGetArg(optCon);
	char * state = poptGetArg(optCon);
	int where = 0;

	if (levels) {
	    where = parseLevels(levels, 0);
	    if (where == -1) usage();
	}

	if (!state) {
	    if (where) {
		rc = 0;
		i = where;
		while (i) {
		    if (i & 1) rc++;
		    i >>= 1;
		}

		if (rc > 1) {
		    fprintf(stderr, "only one runlevel may be specfied for "
			    "a chkconfig query\n");
		    exit(1);
		}
	    }

	    return isOn(name, where) ? 0 : 1;
	} else if (!strcmp(state, "on"))
	    return setService(name, where, 1);
	else if (!strcmp(state, "off"))
	    return setService(name, where, 0);
	else if (!strcmp(state, "reset"))
	    return setService(name, where, -1);
	else
	    usage();
    }

    usage();

    return 1;
}
