#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

int ldbLock(int rw) {
    int fd, i;
    struct flock lock = { 0, SEEK_SET, 0, 0, 0 };

    /* we don't actually write to this file; we open in O_RDWR though just
       to ensure we have enough permissions */

    fd = open(LEVEL_DATABASE, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
	return -1;

    lock.l_type = rw ? F_WRLCK : F_RDLCK;
	
    if (!fcntl(fd, F_SETLK, &lock)) return fd; 

    if (errno == EAGAIN) {
	sleep(1);
	if (!fcntl(fd, F_SETLK, &lock)) return fd; 
    }

    i = errno;
    close(fd);
    errno = i;

    return -1;
}

/* this eats memory if it fails (shucks) */
int ldbRead(int fd, struct service ** listptr, int * numServices) {
    char * map, * end, * start, * chptr, * memend, * numEnd;
    struct stat sb;
    struct service * list;
    int i;

    if (fstat(fd, &sb)) return -1;

    map = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (map == ((char *) -1)) return -1;

    memend = map + sb.st_size;

    i = 0, start = map;
    while (start < memend) {
	if (*start == '\n') i++;
	start++;
    }

    list = malloc(sizeof(*list) * (i + 1));

    i = 0, start = map;
    while (start < memend) {
	end = start;
	while (end < memend && *end != '\n') end++;
	if (*end != '\n') return 1;
	*end = '\0';

	/* name */
	chptr = start;
	while (*chptr && *chptr != ':') chptr++;
	if (!*chptr) return 1;
	*chptr = '\0';
	list[i].name = strdup(start);

	/* levels */
	chptr = start = chptr + 1;
	while (*chptr && *chptr != ':') chptr++;
	if (!*chptr) return 1;
	*chptr = '\0';
	list[i].levels = parseLevels(start, 0);
	if (list[i].levels == -1) return 1;

	/* start priority */
	start = chptr + 1;
	list[i].sPriority = strtol(start, &numEnd, 0);
	if (*numEnd != ':') return 1;
	start = numEnd + 1;

	/* kill priority */
	list[i].kPriority = strtol(start, &numEnd, 0);
	if (*numEnd != ':') return 1;
	start = numEnd + 1;

	/* description */
	if (start >= end) return 1;

	list[i++].desc = strdup(start);

	start = end + 1;
    }
    
    memset(list + i, 0, sizeof(*list));

    munmap(map, sb.st_size);

    *numServices = i;
    *listptr = list;

    return 0;
}

struct service * findService(struct service * list, char * name) {
    struct service * res;

    res = list;
    while (res->name && strcmp(res->name, name)) res++;

    return res->name ? res : NULL;
}

int ldbUnlock(int fd) {
    close(fd);
    return 0;
}

/* last service has name NULL, list must be freed */
int ldbWrite(int * fd, struct service * list) {
    int newfd;
    struct flock lock = { F_WRLCK, SEEK_SET, 0, 0, 0 };
    char buf[4096];
    struct service * s;
    char levels[10];
    int i, j;

    newfd = open(NEW_LEVEL_DATABASE, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (newfd < 0) return -1;
	
    if (fcntl(newfd, F_SETLK, &lock)) return -1;

    for (s = list; s->name; s++) {
	*levels = 0, j = 0;
	for (i = 0; i < 7; i++) {
	    if (s->levels & (1 << i)) {
		levels[j++] = i + '0';
		levels[j] = '\0';
	    }
	}

	sprintf(buf, "%s:%s:%d:%d:%s\n", s->name, levels, s->sPriority,
		s->kPriority, s->desc);

	write(newfd, buf, strlen(buf));
    }

    if (rename(NEW_LEVEL_DATABASE, LEVEL_DATABASE)) {
	i = errno;
	close(newfd);
	unlink(NEW_LEVEL_DATABASE);
	errno = i;
    }

    close(*fd);
    *fd = newfd;
   
    return 0;
}
