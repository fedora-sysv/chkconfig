#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "leveldb.h"

static void usage(void) {
    fprintf(stderr, "chkconfig version " VERSION 
			" - Copyright (C) 1997 Red Hat Software\n");
    fprintf(stderr, "This may be freely redistributed under the terms of "
			"the GNU Public License.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "usage:   chkconfig --list [name]\n");
    fprintf(stderr, "         chkconfig --add <name>\n");
    fprintf(stderr, "         chkconfig --del <name>\n");
    fprintf(stderr, "         chkconfig [--level <levels>] <name> <on|off|reset>\n");

    exit(1);
}

static int delService(char * name) {
    int level, i;
    glob_t globres;

    for (level = 0; level < 7; level++) {
	if (!findServiceEntries(name, level, &globres)) {
	    for (i = 0; i < globres.gl_pathc; i++)
		unlink(globres.gl_pathv[i]);
	    if (globres.gl_pathc) globfree(&globres);
	}
    }

    return 0;
}

static void readServiceError(int rc, char * name) {
    if (rc == 1) {
	fprintf(stderr, "service %s does not support chkconfig\n", name);
    } else {
	fprintf(stderr, "error reading information on service %s: %s\n",
		name, strerror(errno));
    }

    exit(1);
}

static int addService(char * name) {
    int i, rc;
    struct service s;

    if ((rc = readServiceInfo(name, &s))) {
	readServiceError(rc, name);
	return 1;
    }

    for (i = 0; i < 7; i++) {
	if (!isConfigured(name, i)) {
	    if ((1 << i) & s.levels)
		doSetService(s, i, 1);
	    else
		doSetService(s, i, 0);
	}
    }

    return 0;
}

static int showServiceInfo(char * name, int forgiving) {
    int rc;
    int i;
    struct service s;

    if ((rc = readServiceInfo(name, &s))) {
	if (!forgiving)
	    readServiceError(rc, name);
	return forgiving ? 0 : 1;
    }

    printf("%s", s.name);

    for (i = 0; i < 7; i++) {
	printf(" %d:%s", i, isOn(s.name, i) ? "on" : "off");
    }
    printf("\n");

    return 0;
}

static int listService(char * item) {
    DIR * dir;
    struct dirent * ent;
    struct stat sb;
    char fn[1024];

    if (item) return showServiceInfo(item, 0);

    if (!(dir = opendir(RUNLEVELS "/init.d"))) {
	fprintf(stderr, "failed to open " RUNLEVELS "/init.d: %s\n",
		strerror(errno));
        return 1;
    }

    errno = 0;
    while ((ent = readdir(dir))) {
	if (strchr(ent->d_name, '~') || strchr(ent->d_name, ',') ||
	    strchr(ent->d_name, '.')) continue;

	sprintf(fn, RUNLEVELS "/init.d/%s", ent->d_name);
	if (stat(fn, &sb)) continue;
	if (!S_ISREG(sb.st_mode)) continue;

	if (showServiceInfo(ent->d_name, 1)) {
	    closedir(dir);
	    return 1;
	}
    }

    if (errno) {
        perror("error reading from directory " RUNLEVELS "/init.d");
        return 1;
    }

    closedir(dir);

    return 0;
}

int setService(char * name, int where, int state) {
    int i, rc;
    int what;
    struct service s;
    
    if (!where && state != -1) {
	/* levels 3, 4, 5 */
	where = (1 << 3) | (1 << 4) | (1 << 5);
    } else if (!where) {
	where = (1 << 0) | (1 << 1) | (1 << 2) |
	        (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);
    }

    if ((rc = readServiceInfo(name, &s))) {
	readServiceError(rc, name);
	return 1;
    }

    for (i = 0; i < 7; i++) {
	if (!((1 << i) & where)) continue;

	if (state == 1 || state == 0)
	    what = state;
	else if (s.levels & (1 << i))
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

    optCon = poptGetContext("chkconfig", argc, argv, optionsTable, 0);
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

	if (!name || !*name || poptGetArg(optCon)) 
	    usage();

	return addService(name);
    } else if (delItem) {
	char * name = poptGetArg(optCon);

	if (!name || !*name || poptGetArg(optCon)) usage();

	return delService(name);
    } else if (listItem) {
	char * item = poptGetArg(optCon);

	if (item && poptGetArg(optCon)) usage();

	return listService(item);
    } else {
	char * name = poptGetArg(optCon);
	char * state = poptGetArg(optCon);
	int where = 0, level;

	if (levels) {
	    where = parseLevels(levels, 0);
	    if (where == -1) usage();
	}

	if (!state) {
	    if (where) {
		rc = 0;
		i = where;
		while (i) {
		    if (i & 1) {
			rc++;
			level = i;
		    }
		    i >>= 1;
		}

		if (rc > 1) {
		    fprintf(stderr, "only one runlevel may be specfied for "
			    "a chkconfig query\n");
		    exit(1);
		}
	    } 

	    return isOn(name, level) ? 0 : 1;
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
