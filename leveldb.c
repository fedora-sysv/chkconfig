#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "leveldb.h"

int parseLevels(char * str, int emptyOk) {
    char * chptr = str;
    int rc = 0;

    if (!str || !strlen(str))
	return emptyOk ? 0 : -1;

    while (*chptr) {
	if (!isdigit(*chptr) || *chptr > '6') return -1;
	rc |= 1 << (*chptr - '0');
	chptr++;
    }

    return rc;
}

int readServiceInfo(char * name, struct service * service) {
    char * filename = alloca(strlen(name) + strlen(RUNLEVELS) + 50);
    int fd, i;
    struct stat sb;
    char * bufstart, * bufstop, * start, * end, * next;
    struct service serv = { NULL, -1, -1, -1, NULL };
    char overflow;
    char levelbuf[20];

    sprintf(filename, RUNLEVELS "/init.d/%s", name);

    if ((fd = open(filename, O_RDONLY)) < 0) return -1;
    fstat(fd, &sb);

    bufstart = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (bufstart == ((caddr_t) -1)) {
	close(fd);	
	return -1;
    }

    bufstop = bufstart + sb.st_size;
    close(fd);

    next = bufstart;
    while (next < bufstop && (serv.levels == -1 || !serv.desc)) {
	start = next;

	while (isspace(*start) && start < bufstop) start++;
	if (start == bufstop) break; 

	end = strchr(start, '\n');
	if (!end) 
	    next = end = bufstop;
	else
	    next = end + 1;

	if (*start != '#') continue;

	start++;	
	while (isspace(*start) && start < end) start++;
	if (start == end) continue;

	if (!strncmp(start, "chkconfig:", 10)) {
	    start += 10;
	    while (isspace(*start) && start < end) start++;
	    if (start == end) {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }

	    if ((sscanf(start, "%s %d %d%c", levelbuf,
			&serv.sPriority, &serv.kPriority, &overflow) != 4) ||
		 overflow != '\n') {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }

	    serv.levels = parseLevels(levelbuf, 0);
	    if (serv.levels == -1) {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }
	} else if (!strncmp(start, "description:", 12)) {
	    start += 12;
	    while (isspace(*start) && start < end) start++;
	    if (start == end) {
		munmap(bufstart, sb.st_size);
		return 1;
	    }

	    serv.desc = malloc(end - start);
	    strncpy(serv.desc, start, end - start);
	    serv.desc[end - start] = '\0';

	    start = next;
	    while (serv.desc[strlen(serv.desc) - 1] == '\\') {
		serv.desc[strlen(serv.desc) - 1] = '\0';
		start = next;
		
		while (isspace(*start) && start < bufstop) start++;
		if (start == bufstop || *start != '#') {
		    munmap(bufstart, sb.st_size);
		    return 1;
		}

		start++;

		while (isspace(*start) && start < bufstop) start++;
		if (start == bufstop) {
		    munmap(bufstart, sb.st_size);
		    return 1;
		}

		end = strchr(start, '\n');
		if (!end) 
		    next = end = bufstop;
		else
		    next = end + 1;

		i = strlen(serv.desc);
		serv.desc = realloc(serv.desc, i + end - start + 1);
		strncat(serv.desc, start, end - start);
		serv.desc[i + end - start] = '\0';

		start = next;
	    }
	}
    }

    munmap(bufstart, sb.st_size);

    if ((serv.levels == -1 ) || !serv.desc) {
	return 1;
    } 

    serv.name = strdup(name);

    *service = serv;
    return 0;
}

/* returns -1 on error */
int currentRunlevel(void) {
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

int findServiceEntries(char * name, int level, glob_t * globresptr) {
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

int isConfigured(char * name, int level) {
    glob_t globres;

    if (findServiceEntries(name, level, &globres))
	exit(1);

    if (!globres.gl_pathc)
	return 0;

    globfree(&globres);
    return 1;
}

int isOn(char * name, int level) {
    glob_t globres;

    if (!level) {
	level = currentRunlevel();
	if (level == -1) {
	    fprintf(stderr, "cannot determine current run level\n");
	    return 0;
	}
    }

    if (findServiceEntries(name, level, &globres))
	exit(1);

    if (!globres.gl_pathc || !strstr(globres.gl_pathv[0], "/S"))
	return 0;

    globfree(&globres);
    return 1;
}

int doSetService(struct service s, int level, int on) {
    int priority = on ? s.sPriority : s.kPriority;
    char linkname[200];
    char linkto[200];
    glob_t globres;
    int i;

    if (!findServiceEntries(s.name, level, &globres)) {
	for (i = 0; i < globres.gl_pathc; i++)
	    unlink(globres.gl_pathv[i]);
	if (globres.gl_pathc) globfree(&globres);
    }

    sprintf(linkname, "%s/rc%d.d/%c%d%s", RUNLEVELS, level,
			on ? 'S' : 'K', priority, s.name);
    sprintf(linkto, "../init.d/%s", s.name);

    unlink(linkname);	/* just in case */
    if (symlink(linkto, linkname)) {
	fprintf(stderr, "failed to make symlink %s: %s\n", linkname,
		strerror(errno));
	return 1;
    }

    return 0; 
}

