#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <libintl.h> 
#include <locale.h> 
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Changes
   1998-09-22 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
                i18n for init.d scripts (eg.: description(pt_BR) is a brazilian
		portuguese description for the package) 
*/

#define _(String) gettext((String)) 

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

int readDescription(char *start, char *bufstop, char **english_desc, char **serv_desc) { 
	char english;
	char my_lang_loaded = 0;
	char is_my_lang = 0;
	char * lang = getenv ("LANG");
	char * final_parenthesis;
	char * end, *next;
	int i;
	
	english = *start == ':';
	end = strchr(start, '\n');
	if (!end) 
	    next = end = bufstop;
	else
	    next = end + 1;

	if (!english) {
		if (*start != '(') {
		    return 1;
		}

                ++start;
		final_parenthesis = strchr (start, ')');

		if (final_parenthesis == NULL || final_parenthesis - start > 5) {
		    return 1;
		}

		is_my_lang = lang ? strncmp (lang, start, strlen (lang)) == 0 : 0;
		start = final_parenthesis + 2;
	    } else ++start;

	    while (isspace(*start) && start < end) start++;
	    if (start == end) {
		return 1;
	    }
          {
	    char* desc = malloc(end - start + 1);
	    strncpy(desc, start, end - start);
	    desc[end - start] = '\0';

	    start = next;

	    while (desc[strlen(desc) - 1] == '\\') {
		desc[strlen(desc) - 1] = '\0';
		start = next;
		
		while (isspace(*start) && start < bufstop) start++;
		if (start == bufstop || *start != '#') {
		    return 1;
		}

		start++;

		while (isspace(*start) && start < bufstop) start++;
		if (start == bufstop) {
		    return 1;
		}

		end = strchr(start, '\n');
		if (!end) 
		    next = end = bufstop;
		else
		    next = end + 1;

		i = strlen(desc);
		desc = realloc(desc, i + end - start + 1);
		strncat(desc, start, end - start);
		desc[i + end - start] = '\0';

		start = next;
	    }

	    if (desc) {
		    if (my_lang_loaded) {
			    free(desc);
		    } else if (is_my_lang) {
			    if (*serv_desc)
			      free(*serv_desc);

			    *serv_desc = desc;
			    return 0;
		    } else if (english) {
			    if (*serv_desc)
			      free(*serv_desc);

			    if (*english_desc)
			      free (*english_desc);

			    *english_desc = desc;
		    } else free (desc);
	    }
	  }
	return 0;
}

int readXinetdServiceInfo(char *name, struct service * service, int honorHide) {
	char * filename = alloca(strlen(name) + strlen(XINETDDIR) + 50);
	int fd;
	struct service serv = { NULL, -1, -1, -1, NULL, 1, -1 };
	struct stat sb;
	char * buf, *ptr;
	char * eng_desc = NULL, *start;
	
	snprintf(filename, strlen(name)+strlen(XINETDDIR)+50, XINETDDIR "/%s", name);
	
	if ((fd = open(filename, O_RDONLY)) < 0) return -1;
	fstat(fd,&sb);
	buf = malloc(sb.st_size+1);
	if (read(fd,buf,sb.st_size)!=sb.st_size) {
		close(fd);
		free(buf);
		return -1;
	}
	close(fd);
        serv.name = strdup(name);
	buf[sb.st_size] = '\0';
	start = buf;
	while (buf) {
		ptr = strchr(buf,'\n');
		if (*buf == '#') {
			buf++;
			while (isspace(*buf) && buf < ptr) buf++;
			if (!strncmp(buf,"default:", 9)) {
				buf+=8;
				while(isspace(*buf)) buf++;
				if (!strncmp(buf+9,"on",2)) {
					serv.enabled = 1;
				} else {
					serv.enabled = 0;
				}
			} else if (!strncmp(buf,"description:",12)) {
				buf+=11;
				if (readDescription(buf,start+sb.st_size,
						    &serv.desc,&eng_desc)) {
					if (serv.desc) free(serv.desc);
				}
				if (!serv.desc) {
					if (eng_desc)
					  serv.desc = eng_desc;
				} else if (eng_desc)
					  free (eng_desc);
			}
			if (ptr) {
				*ptr = '\0';
				ptr++;
			} 
			buf = ptr;
			continue;
		}
		while (isspace(*buf) && buf < ptr) buf++;
		if (!strncmp(buf,"disable", 7)) {
			buf = strstr(buf,"=");
			if (buf) 
			  do {
				  buf++;
			  } while(isspace(*buf));

			if (buf && strncmp(buf,"yes",3)) {
				serv.levels = parseLevels("0123456",0);
				if (serv.enabled == -1)
				  serv.enabled = 1;
			} else {
				serv.levels = 0;
				if (serv.enabled == -1)
				  serv.enabled = 0;
			}
		}
		if (ptr) {
			*ptr = '\0';
			ptr++;
		} 
		buf = ptr;
	}
	*service = serv;
	return 0;
}

int readServiceInfo(char * name, struct service * service, int honorHide) {
    char * filename = alloca(strlen(name) + strlen(RUNLEVELS) + 50);
    int fd;
    struct stat sb;
    char * bufstart, * bufstop, * start, * end, * next;
    struct service serv = { NULL, -1, -1, -1, NULL, 0, 0 };
    char overflow;
    char levelbuf[20];
    char * english_desc = NULL;

    sprintf(filename, RUNLEVELS "/init.d/%s", name);

    if ((fd = open(filename, O_RDONLY)) < 0) {
	    return readXinetdServiceInfo(name,service,honorHide);
    }
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
	if (honorHide && !strncmp(start, "hide:", 5)) {
	    start += 5;
	    while (isspace(*start) && start < end) start++;
	    if (start == end || !strncmp(start, "true", 4)) {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }
	}

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
		 !isspace(overflow)) {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }

	    if (!strcmp(levelbuf, "-"))
		serv.levels = 0;
	    else
		serv.levels = parseLevels(levelbuf, 0);
	    if (serv.levels == -1) {
		if (serv.desc) free(serv.desc);
		munmap(bufstart, sb.st_size);
		return 1;
	    }
	} else if (!strncmp(start, "description", 11)) {
		if (readDescription(start+11, bufstop, &english_desc, &serv.desc)) {
			if (serv.desc) free(serv.desc);
		}
	}
    }

    munmap(bufstart, sb.st_size);

    if (!serv.desc) {
      if (english_desc)
	serv.desc = english_desc;
    } else if (english_desc)
	free (english_desc);

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

    sprintf(match, "%s/rc%d.d/[SK][0-9][0-9]%s", RUNLEVELS, level, name);

    rc = glob(match, GLOB_ERR | GLOB_NOSORT, NULL, &globres);

    if (rc && rc != GLOB_NOMATCH) {
	fprintf(stderr, _("failed to glob pattern %s: %s\n"), match,
		strerror(errno));
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

    if (level == -1) {
	level = currentRunlevel();
	if (level == -1) {
	    fprintf(stderr, _("cannot determine current run level\n"));
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

int setXinetdService(struct service s, int on) {
	int oldfd, newfd;
	char oldfname[100], newfname[100];
	char tmpstr[50];
	char *buf, *ptr, *tmp;
	struct stat sb;
	
	if (on == -1) {
		on = s.enabled ? 1 : 0;
	}
	snprintf(oldfname,100,"%s/%s",XINETDDIR,s.name);
	if ( (oldfd = open(oldfname,O_RDONLY)) == -1 ) {
		return -1;
	}
	fstat(oldfd,&sb);
	buf = malloc(sb.st_size+1);
	if (read(oldfd,buf,sb.st_size)!=sb.st_size) {
		close(oldfd);
		free(buf);
		return -1;
	}
	close(oldfd);
	buf[sb.st_size] = '\0';
	snprintf(newfname,100,"%s/%s.XXXXXX",XINETDDIR,s.name);
	newfd = mkstemp(newfname);
	if (newfd == -1) {
		free(buf);
		return -1;
	}
	while (buf) {
		tmp = buf;
		ptr = strchr(buf,'\n');
		if (ptr) {
			*ptr = '\0';
			ptr++;
		} 
		while (isspace(*buf)) buf++;
		if (strncmp(buf,"disable", 7)) {
			write(newfd,tmp,strlen(tmp));
			write(newfd,"\n",1);
			if (buf[0] == '{') {
				snprintf(tmpstr,50,"\tdisable\t= %s", on ? "no" : "yes");
				write(newfd,tmpstr,strlen(tmpstr));
				write(newfd,"\n",1);
			}
		}
		buf = ptr;
	}
	close(newfd);
	chmod(newfname,644);
	unlink(oldfname);
	return(rename(newfname,oldfname));
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

    sprintf(linkname, "%s/rc%d.d/%c%02d%s", RUNLEVELS, level,
			on ? 'S' : 'K', priority, s.name);
    sprintf(linkto, "../init.d/%s", s.name);

    unlink(linkname);	/* just in case */
    if (symlink(linkto, linkname)) {
	fprintf(stderr, _("failed to make symlink %s: %s\n"), linkname,
		strerror(errno));
	return 1;
    }

    return 0; 
}

