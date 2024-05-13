/* Copyright 1997-2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <unistd.h>

#define FLAGS_TEST (1 << 0)
#define FLAGS_VERBOSE (1 << 1)
#define FLAGS_KEEP_MISSING (1 << 2)
#define FLAGS_KEEP_FOREIGN (1 << 3)

#define FL_TEST(flags) ((flags)&FLAGS_TEST)
#define FL_VERBOSE(flags) ((flags)&FLAGS_VERBOSE)
#define FL_KEEP_MISSING(flags) ((flags)&FLAGS_KEEP_MISSING)
#define FL_KEEP_FOREIGN(flags) ((flags)&FLAGS_KEEP_FOREIGN)

#define _(foo) gettext(foo)

struct linkSet {
    char *title;    /* print */
    char *facility; /* /usr/bin/lpr */
    char *target;   /* /usr/bin/lpr.LPRng */
};

struct alternative {
    int priority;
    struct linkSet leader;
    struct linkSet *followers;
    char *initscript;
    int numFollowers;
    char *family;
};

struct alternativeSet {
    enum alternativeMode { AUTO, MANUAL } mode;
    struct alternative *alts;
    int numAlts;
    int best, current;
    char *currentLink;
};

enum programModes {
    MODE_UNKNOWN,
    MODE_INSTALL,
    MODE_REMOVE,
    MODE_REMOVE_ALL,
    MODE_AUTO,
    MODE_DISPLAY,
    MODE_CONFIG,
    MODE_SET,
    MODE_FOLLOWER,
    MODE_VERSION,
    MODE_USAGE,
    MODE_LIST,
    MODE_ADD_FOLLOWER,
    MODE_REMOVE_FOLLOWER
};

static int usage(int rc) {
    printf(_("alternatives version %s - Copyright (C) 2001 Red Hat, Inc.\n"),
           VERSION);
    printf(_("This may be freely redistributed under the terms of the GNU "
             "Public License.\n\n"));
    printf(
        _("usage: alternatives --install <link> <name> <path> <priority>\n"));
    printf(_("                    [--initscript <service>]\n"));
    printf(_("                    [--family <family>]\n"));
    printf(_("                    [--follower <follower_link> <follower_name> <follower_path>]*\n"));
    printf(_("       alternatives --remove <name> <path>\n"));
    printf(_("       alternatives --auto <name>\n"));
    printf(_("       alternatives --config <name>\n"));
    printf(_("       alternatives --display <name>\n"));
    printf(_("       alternatives --set <name> <path/family>\n"));
    printf(_("       alternatives --list\n"));
    printf(_("       alternatives --remove-all <name>\n"));
    printf(_("       alternatives --add-follower <name> <path> <follower_link> <follower_name> <follower_path>\n"));
    printf(_("       alternatives --remove-follower <name> <path> <follower_name>\n"));
    printf(_("\n"));
    printf(_("common options: --verbose --test --help --usage --version "
             "--keep-missing --keep-foreign\n"));
    printf(_("                --altdir <directory> --admindir <directory>\n"));

    exit(rc);
}

/*
 * Function to clean path form unnecessary backslashes
 * It will make from //abcd///efgh/ -> /abcd/efgh/
 */
char *normalize_path(char *s) {
    if (s) {
        const char *src = s;
        char *dst = (char *)s;
        while ((*dst = *src) != '\0') {
            do {
                src++;
            } while (*dst == '/' && *src == '/');
            dst++;
        }
    }
    return s;
}

char *normalize_path_alloc(const char *s) {
    char *ret = strdup(s);
    return normalize_path(ret);
}

int streq(const char *a, const char *b) {
    if (a && b)
        return strcmp(a, b) ? 0 : 1;

    if (!a && !b)
        return 1;

    return 0;
}

char * strsteal(char **ptr) {
    char *ret = *ptr;
    *ptr = NULL;
    return ret;
}

int altBetter(struct alternative new, struct alternative old, char *family) {
    if (!family || (streq(old.family, family) == streq(new.family, family)))
        return new.priority > old.priority;

    if (streq(new.family, family))
        return 1;

    return 0;
}

static int isSystemd(char *initscript) {
    char tmppath[500];
    struct stat sbuf;

    snprintf(tmppath, 500, "/lib/systemd/system/%s.service", initscript);
    if (!stat(tmppath, &sbuf))
        return 1;

    snprintf(tmppath, 500, "/etc/systemd/system/%s.service", initscript);
    if (!stat(tmppath, &sbuf))
        return 1;

    return 0;
}

static void setupSingleArg(enum programModes *mode, const char ***nextArgPtr,
                           enum programModes newMode, char **title) {
    const char **nextArg = *nextArgPtr;

    if (*mode != MODE_UNKNOWN)
        usage(2);
    *mode = newMode;
    nextArg++;

    if (!*nextArg || **nextArg == '/')
        usage(2);
    *title = strdup(*nextArg);
    *nextArgPtr = nextArg + 1;
}

static void setupDoubleArg(enum programModes *mode, const char ***nextArgPtr,
                           enum programModes newMode, char **title,
                           char **target) {
    const char **nextArg = *nextArgPtr;

    if (*mode != MODE_UNKNOWN)
        usage(2);
    *mode = newMode;
    nextArg++;

    if (!*nextArg || **nextArg == '/')
        usage(2);
    *title = strdup(*nextArg);
    nextArg++;

    if (!*nextArg)
        usage(2);
    *target = normalize_path_alloc(*nextArg);
    *nextArgPtr = nextArg + 1;
}

static void setupTripleArg(enum programModes *mode, const char ***nextArgPtr,
                           enum programModes newMode, char **title,
                           char **target, char **followerTitle) {
    const char **nextArg = *nextArgPtr;

    if (*mode != MODE_UNKNOWN)
        usage(2);
    *mode = newMode;
    nextArg++;

    if (!*nextArg || **nextArg == '/')
        usage(2);
    *title = strdup(*nextArg);
    nextArg++;

    if (!*nextArg)
        usage(2);
    *target = normalize_path_alloc(*nextArg);
    nextArg++;

    if (!*nextArg)
        usage(2);
    *followerTitle = strdup(*nextArg);
    *nextArgPtr = nextArg + 1;
}

static void setupLinkSet(struct linkSet *set, const char ***nextArgPtr) {
    const char **nextArg = *nextArgPtr;

    if (!*nextArg || **nextArg != '/')
        usage(2);
    set->facility = normalize_path_alloc(*nextArg);
    nextArg++;

    if (!*nextArg || **nextArg == '/')
        usage(2);
    set->title = strdup(*nextArg);
    nextArg++;

    if (!*nextArg || **nextArg != '/')
        usage(2);
    set->target = normalize_path_alloc(*nextArg);
    *nextArgPtr = nextArg + 1;
}

char *parseLine(char **buf) {
    char *start = *buf;
    char *end;

    if (!*buf || !**buf)
        return NULL;

    end = strchr(start, '\n');
    if (!end) {
        *buf = start + strlen(start);
    } else {
        *buf = end + 1;
        *end = '\0';
    }

    while (isspace(*start) && *start)
        start++;

    return strdup(start);
}

void nextLine(char **buf, char **line) {
    free(*line);
    *line = parseLine(buf);
}

static int readConfig(struct alternativeSet *set, const char *title,
                      const char *altDir, const char *stateDir, int flags) {
    char *path;
    char *leader_path;
    int fd;
    int i;
    struct stat sb;
    char *buf;
    char *end;
    char *line = NULL;
    char *ptr;
    struct {
        char *facility;
        char *title;
    } *groups = NULL;
    int numGroups = 0;
    char linkBuf[PATH_MAX];
    int r = 0;

    set->alts = NULL;
    set->numAlts = 0;
    set->mode = AUTO;
    set->best = 0;
    set->current = -1;

    path = alloca(strlen(stateDir) + strlen(title) + 2);
    sprintf(path, "%s/%s", stateDir, title);

    path = normalize_path(path);

    if (FL_VERBOSE(flags))
        printf(_("reading %s\n"), path);

    if ((fd = open(path, O_RDONLY)) < 0) {
        if (errno == ENOENT)
            return 3;
        fprintf(stderr, _("failed to open %s: %s\n"), path, strerror(errno));
        return 1;
    }

    fstat(fd, &sb);
    buf = alloca(sb.st_size + 1);
    if (read(fd, buf, sb.st_size) != sb.st_size) {
        close(fd);
        fprintf(stderr, _("failed to read %s: %s\n"), path, strerror(errno));
        return 1;
    }
    close(fd);
    buf[sb.st_size] = '\0';

    nextLine(&buf, &line);
    if (!line) {
        fprintf(stderr, _("%s empty!\n"), path);
        r = 1;
        goto finish;
    }

    if (!strcmp(line, "auto")) {
        set->mode = AUTO;
    } else if (!strcmp(line, "manual")) {
        set->mode = MANUAL;
    } else {
        fprintf(stderr, _("bad mode on line 1 of %s\n"), path);
        r = 1;
        goto finish;
    }

    nextLine(&buf, &line);
    if (!line || *line != '/') {
        fprintf(stderr, _("bad primary link in %s\n"), path);
        r = 1;
        goto finish;
    }

    groups = realloc(groups, sizeof(*groups));
    groups[0].title = strdup(title);
    groups[0].facility = strsteal(&line);
    numGroups = 1;

    nextLine(&buf, &line);
    while (line && *line) {
        if (*line == '/') {
            fprintf(stderr, _("path %s unexpected in %s\n"), line, path);
            r = 1;
            goto finish;
        }

        groups = realloc(groups, sizeof(*groups) * (numGroups + 1));
        groups[numGroups].title = strsteal(&line);

        nextLine(&buf, &line);
        if (!line || !*line) {
            fprintf(stderr, _("missing path for follower %s in %s\n"), line, path);
            r = 1;
            goto finish;
        }

        groups[numGroups++].facility = strsteal(&line);

        nextLine(&buf, &line);
    }

    if (!line) {
        fprintf(stderr, _("unexpected end of file in %s\n"), path);
        r = 1;
        goto finish;
    }

    nextLine(&buf, &line);
    while (line && *line) {
        set->alts = realloc(set->alts, (set->numAlts + 1) * sizeof(*set->alts));

        if (*line != '/') {
            fprintf(stderr, _("path to alternate expected in %s\n"), path);
            fprintf(stderr, _("unexpected line in %s: %s\n"), path, line);
            r = 1;
            goto finish;
        }

        set->alts[set->numAlts].leader.facility = normalize_path_alloc(groups[0].facility);
        set->alts[set->numAlts].leader.title = strdup(groups[0].title);
        set->alts[set->numAlts].leader.target = strsteal(&line);
        set->alts[set->numAlts].numFollowers = numGroups - 1;
        if (numGroups > 1)
            set->alts[set->numAlts].followers = malloc(
                (numGroups - 1) * sizeof(*set->alts[set->numAlts].followers));
        else
            set->alts[set->numAlts].followers = NULL;

        set->alts[set->numAlts].priority = -1;
        set->alts[set->numAlts].initscript = NULL;
        set->alts[set->numAlts].family = NULL;

        nextLine(&buf, &line);
        ptr = line;

        if (ptr && ptr[0] == '@') {
            ptr++;
            end = strchr(ptr, '@');
            if (!end || (end == ptr)) {
                fprintf(stderr,
                        _("closing '@' missing or the family is empty in %s\n"),
                        path);
                fprintf(stderr, _("unexpected line in %s: %s\n"), path, line);
                r = 1;
                goto finish;
            }
            *end = '\0';
            set->alts[set->numAlts].family = strdup(ptr);
            ptr = end + 1;
        }

        set->alts[set->numAlts].priority = strtol(ptr, &end, 0);

        if (!end || (end == ptr)) {
            fprintf(stderr, _("numeric priority expected in %s\n"), path);
            fprintf(stderr, _("unexpected line in %s: %s\n"), path, line);
            r = 1;
            goto finish;
        }
        if (end) {
            while (*end && isspace(*end))
                end++;
            if (strlen(end)) {
                set->alts[set->numAlts].initscript = strdup(end);
            }
        }

        if (set->alts[set->numAlts].priority > set->alts[set->best].priority)
            set->best = set->numAlts;

        for (i = 1; i < numGroups; i++) {
            nextLine(&buf, &line);
            if (line && strlen(line) && *line != '/') {
                fprintf(stderr, _("follower path expected in %s\n"), path);
                fprintf(stderr, _("unexpected line in %s: %s\n"), path, line);
                r = 1;
                goto finish;
            }

            set->alts[set->numAlts].followers[i - 1].title =
                strdup(groups[i].title);
            set->alts[set->numAlts].followers[i - 1].facility =
                normalize_path_alloc(groups[i].facility);
            set->alts[set->numAlts].followers[i - 1].target =
                (line && strlen(line)) ? strsteal(&line) : NULL;
        }

        set->numAlts++;

        nextLine(&buf, &line);
    }

    while (line) {
        nextLine(&buf, &line);
        if (line && *line) {
            fprintf(stderr, _("unexpected line in %s: %s\n"), path, line);
            r = 1;
            goto finish;
        }
    }

    leader_path = alloca(strlen(altDir) + strlen(set->alts[0].leader.title) + 2);
    sprintf(leader_path, "%s/%s", altDir, set->alts[0].leader.title);

    if (((i = readlink(leader_path, linkBuf, sizeof(linkBuf) - 1)) < 0)) {
        fprintf(stderr, _("failed to read link %s: %s\n"),
                set->alts[0].leader.facility, strerror(errno));
        r = 2;
        goto finish;
    }

    linkBuf[i] = '\0';

    for (i = 0; i < set->numAlts; i++)
        if (!strcmp(linkBuf, set->alts[i].leader.target))
            break;

    if (i == set->numAlts) {
        set->mode = MANUAL;
        set->current = -1;
        if (FL_VERBOSE(flags))
            printf(
                _("link points to no alternative -- setting mode to manual\n"));
    } else {
        if (i != set->best && set->mode == AUTO) {
            set->mode = MANUAL;
            if (FL_VERBOSE(flags))
                printf(_("link changed -- setting mode to manual\n"));
        }
        set->current = i;
    }

    set->currentLink = normalize_path_alloc(linkBuf);
finish:
    for (i = 1; i < numGroups; i++) {
        free(groups[i].title);
        free(groups[i].facility);
    }
    free(groups);
    free(line);
    return r;
}

static int isLink(char *path) {
    struct stat sbuf;

    if (lstat(path, &sbuf))
        return 0;
    return !!S_ISLNK(sbuf.st_mode);
}

static int fileExists(char *path) {
    struct stat sbuf;

    return !stat(path, &sbuf);
}

static int facilityBelongsToUs(char *facility, const char *altDir) {
    char buf[PATH_MAX];
    if (readlink(facility, buf, sizeof(buf)) <= 0)
        return 0;
    return strncmp(buf, altDir, strlen(altDir)) ? 0 : 1;
}

static int removeLinks(struct linkSet *l, const char *altDir, int flags) {
    char *sl;

    if (FL_KEEP_MISSING(flags))
        return 0;

    sl = alloca(strlen(altDir) + strlen(l->title) + 2);
    sprintf(sl, "%s/%s", altDir, l->title);
    if (FL_TEST(flags)) {
        printf(_("would remove %s\n"), sl);
    } else if (isLink(sl) && unlink(sl) && errno != ENOENT) {
        fprintf(stderr, _("failed to remove link %s: %s\n"), sl,
                strerror(errno));
        return 1;
    }
    if (FL_TEST(flags)) {
        printf(_("would remove %s\n"), l->facility);
    } else {
        if (isLink(l->facility) && (!FL_KEEP_FOREIGN(flags) || facilityBelongsToUs(l->facility, altDir)))
            if (unlink(l->facility) && errno != ENOENT) {
                fprintf(stderr, _("failed to remove link %s: %s\n"), l->facility,
                strerror(errno));
                return 1;
            }
    }

    return 0;
}

static int makeLinks(struct linkSet *l, const char *altDir, int flags) {
    char *sl;
    char buf[PATH_MAX];

    sl = alloca(strlen(altDir) + strlen(l->title) + 2);
    sprintf(sl, "%s/%s", altDir, l->title);

    if (fileExists(l->facility) && !isLink(l->facility)) {
        fprintf(
            stderr,
            _("failed to link %s -> %s: %s exists and it is not a symlink\n"),
            l->facility, sl, l->facility);
    } else if (FL_KEEP_FOREIGN(flags) && isLink(l->facility) && !facilityBelongsToUs(l->facility, altDir)) {
        fprintf(
            stderr,
            _("failed to link %s -> %s: --keep-foreign was set and link %s points outside  %s\n"),
            l->facility, sl, l->facility, altDir);
    } else {
        if (FL_TEST(flags)) {
            printf(_("would link %s -> %s\n"), l->facility, sl);
        } else {
            memset(buf, 0, sizeof(buf));
            readlink(l->facility, buf, sizeof(buf)-1);

            if(!streq(sl, buf)) {
                unlink(l->facility);

                if (symlink(sl, l->facility)) {
                    fprintf(stderr, _("failed to link %s -> %s: %s\n"), l->facility,
                            sl, strerror(errno));
                    return 1;
                }
            }
        }
    }

    if (FL_TEST(flags)) {
        printf(_("would link %s -> %s\n"), sl, l->target);
    } else {
        memset(buf, 0, sizeof(buf));
        readlink(sl, buf, sizeof(buf)-1);

        if(!streq(l->target, buf)) {
            if (unlink(sl) && errno != ENOENT) {
                fprintf(stderr, _("failed to remove link %s: %s\n"), sl,
                        strerror(errno));
                return 1;
            }

            if (symlink(l->target, sl)) {
                fprintf(stderr, _("failed to link %s -> %s: %s\n"), sl, l->target,
                        strerror(errno));
                return 1;
            }
        }
    }

    return 0;
}

static int writeState(struct alternativeSet *set, const char *altDir,
                      const char *stateDir, int forceLinks, int flags) {
    char *path;
    char *path2;
    int fd;
    FILE *f;
    int i, j;
    int rc = 0;
    struct alternative *alt;

    path = alloca(strlen(stateDir) + strlen(set->alts[0].leader.title) + 6);
    sprintf(path, "%s/%s.new", stateDir, set->alts[0].leader.title);

    path2 = alloca(strlen(stateDir) + strlen(set->alts[0].leader.title) + 2);
    sprintf(path2, "%s/%s", stateDir, set->alts[0].leader.title);

    if (FL_TEST(flags))
        fd = dup(1);
    else
        fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);

    if (fd < 0) {
        if (errno == EEXIST)
            fprintf(stderr, _("%s already exists\n"), path);
        else
            fprintf(stderr, _("failed to create %s: %s\n"), path,
                    strerror(errno));
        return 1;
    }

    f = fdopen(fd, "w");
    fprintf(f, "%s\n", set->mode == AUTO ? "auto" : "manual");
    fprintf(f, "%s\n", set->alts[0].leader.facility);
    for (i = 0; i < set->alts[0].numFollowers; i++) {
        fprintf(f, "%s\n", set->alts[0].followers[i].title);
        fprintf(f, "%s\n", set->alts[0].followers[i].facility);
    }
    fprintf(f, "\n");

    for (i = 0; i < set->numAlts; i++) {
        fprintf(f, "%s\n", set->alts[i].leader.target);
        if (set->alts[i].family)
            fprintf(f, "@%s@", set->alts[i].family);
        fprintf(f, "%d", set->alts[i].priority);
        if (set->alts[i].initscript)
            fprintf(f, " %s", set->alts[i].initscript);
        fprintf(f, "\n");

        for (j = 0; j < set->alts[i].numFollowers; j++) {
            if (set->alts[i].followers[j].target)
                fprintf(f, "%s", set->alts[i].followers[j].target);
            fprintf(f, "\n");
        }
    }

    fclose(f);

    if (!FL_TEST(flags) && rename(path, path2)) {
        fprintf(stderr, _("failed to replace %s with %s: %s\n"), path2, path,
                strerror(errno));
        unlink(path);
        return 1;
    }

    if (set->mode == AUTO)
        set->current = set->best;

    alt = set->alts + (set->current > 0 ? set->current : 0);

    if (forceLinks || set->mode == AUTO) {
        rc |= makeLinks(&alt->leader, altDir, flags);
        for (i = 0; i < alt->numFollowers; i++) {
            if (alt->followers[i].target)
                rc |= makeLinks(alt->followers + i, altDir, flags);
            else
                rc |= removeLinks(alt->followers + i, altDir, flags);
        }
    }

    if (!FL_TEST(flags)) {
        if (alt->initscript) {
            if (isSystemd(alt->initscript)) {
                asprintf(&path, "/bin/systemctl -q is-enabled %s.service || "
                                "/bin/systemctl -q preset %s.service",
                         alt->initscript, alt->initscript);
                if (FL_VERBOSE(flags))
                    printf(_("running %s\n"), path);
                system(path);
                free(path);
            } else {
                asprintf(&path, "/sbin/chkconfig --add %s", alt->initscript);
                if (FL_VERBOSE(flags))
                    printf(_("running %s\n"), path);
                system(path);
                free(path);
            }
        }
        for (i = 0; i < set->numAlts; i++) {
            struct alternative *tmpalt = set->alts + i;
            if (tmpalt != alt && tmpalt->initscript) {
                if (isSystemd(tmpalt->initscript)) {
                    asprintf(&path, "/bin/systemctl -q disable %s.service",
                             tmpalt->initscript);
                    if (FL_VERBOSE(flags))
                        printf(_("running %s\n"), path);
                    system(path);
                    free(path);
                } else {
                    asprintf(&path, "/sbin/chkconfig --del %s",
                             tmpalt->initscript);
                    if (FL_VERBOSE(flags))
                        printf(_("running %s\n"), path);
                    system(path);
                    free(path);
                }
            }
        }
    }

    return rc;
}

static int linkCmp(const void *a, const void *b) {
    struct linkSet *one = (struct linkSet *)a, *two = (struct linkSet *)b;

    return strcmp(one->facility, two->facility);
}

static void fillTemplateFrom(struct alternative source,
                             struct alternative *template) {
    template->numFollowers = source.numFollowers;
    template->followers = malloc(source.numFollowers * sizeof(struct linkSet));
    memcpy(template->followers, source.followers,
           source.numFollowers * sizeof(struct linkSet));
}

static void addFollowerToAlternative(struct alternative *template,
                                  struct linkSet follower) {
    int i;
    for (i = 0; i < template->numFollowers; i++) {
        if (streq(follower.facility, template->followers[i].facility))
            break;
    }
    if (i == template->numFollowers) {
        template->followers =
            realloc(template->followers,
                    (template->numFollowers + 1) * sizeof(struct linkSet));

        memcpy(&template->followers[i], &follower, sizeof(struct linkSet));

        template->numFollowers++;
    }
}

static int matchFollowers(struct alternativeSet *set,
                       struct alternative template) {
    int i, j, k;
    struct linkSet *newLinks;

    /* Sort the list for file legibility */
    qsort(template.followers, template.numFollowers, sizeof(struct linkSet), linkCmp);

    /* need to match the followers up; newLinks will parallel the original
       ordering */
    for (k = 0; k < set->numAlts; k++) {
        newLinks = malloc(sizeof(struct linkSet) * template.numFollowers);
        if (!newLinks)
            return 3;

        newLinks =
            memset(newLinks, 0, sizeof(struct linkSet) * template.numFollowers);

        for (j = 0; j < template.numFollowers; j++) {
            for (i = 0; i < set->alts[k].numFollowers; i++) {
                if (!strcmp(set->alts[k].followers[i].title,
                            template.followers[j].title))
                    break;
            }
            /* check if the follower in alternatives exist they have same name
             * and link*/
            if (i < set->alts[k].numFollowers) {
                if (strcmp(set->alts[k].followers[i].facility,
                           template.followers[j].facility)) {
                    fprintf(
                        stderr, _("link %s incorrect for follower %s (%s %s)\n"),
                        set->alts[k].followers[i].facility,
                        set->alts[k].followers[i].title,
                        template.followers[j].facility, template.followers[j].title);
                    free(newLinks);
                    return 2;
                }
                newLinks[j] = set->alts[k].followers[i];
            } else {
                /* alternative did not have a record about a follower, let's add it
                 * with empty target */
                newLinks[j].title = template.followers[j].title;
                newLinks[j].facility = template.followers[j].facility;
                newLinks[j].target = NULL;
            }
        }
        /* memory link */
        free(set->alts[k].followers);
        set->alts[k].followers = newLinks;
        set->alts[k].numFollowers = template.numFollowers;
    }
    return 0;
}

static struct alternative *findAlternativeInSet(struct alternativeSet set,
                                                char *target) {
    int i;

    for (i = 0; i < set.numAlts; i++)
        if (streq(set.alts[i].leader.target, target))
            return set.alts + i;
    return NULL;
}

static void removeUnusedFollowersFromTemplate(struct alternativeSet set,
                                           struct alternative *template,
                                           const char *altDir, int flags) {
    int i, j, k = 0;
    int found;

    if (set.numAlts == 0)
        return;

    for (i = 0; i < template->numFollowers; i++) {
        found = 0;
        for (j = 0; j < set.numAlts && !found; j++)
            for (k = 0; k < set.alts[j].numFollowers && !found; k++)
                if (streq(template->followers[i].title,
                          set.alts[j].followers[k].title) &&
                    set.alts[j].followers[k].target)
                    found = 1;

        if (!found) {
            removeLinks(template->followers + i, altDir, flags);
            template->numFollowers--;
            if (i != template->numFollowers) {
                template->followers[i] = template->followers[template->numFollowers];
                i--;
            }
        }
    }
}

static int removeFollower(char *title, char *target, char *followerTitle,
                       const char *altDir, const char *stateDir, int flags) {
    struct alternative template, *a = NULL;
    struct alternativeSet set;
    int i;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    a = findAlternativeInSet(set, target);
    if (!a) {
        fprintf(stderr,
                _("%s has not been configured as an alternative for %s\n"),
                target, title);
        return 2;
    }

    for (i = 0; i < a->numFollowers; i++) {
        if (streq(a->followers[i].title, followerTitle)) {
            a->followers[i].target = NULL;
            fillTemplateFrom(*a, &template);
            removeUnusedFollowersFromTemplate(set, &template, altDir, flags);
            matchFollowers(&set, template);
            if (writeState(&set, altDir, stateDir, 1, flags))
                return 2;
            return 0;
        }
    }
    fprintf(
        stderr,
        _("%s has not been configured as an follower alternative for %s (%s)\n"),
        followerTitle, title, target);
    return 2;
}

static int addFollower(char *title, char *target, struct linkSet newFollower,
                    const char *altDir, const char *stateDir, int flags) {
    struct alternativeSet set;
    int i;
    struct alternative *a = NULL;
    struct alternative template;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    a = findAlternativeInSet(set, target);

    if (!a) {
        fprintf(stderr,
                _("%s has not been configured as an alternative for %s\n"),
                target, title);
        return 2;
    }

    fillTemplateFrom(*a, &template);
    addFollowerToAlternative(&template, newFollower);
    matchFollowers(&set, template);

    /* let's check if such follower already exists, in this case we will just
     * update the link */
    for (i = 0; i < a->numFollowers; i++) {
        if (streq(a->followers[i].title, newFollower.title)) {
            a->followers[i].target = newFollower.target;
            break;
        }
    }

    if (writeState(&set, altDir, stateDir, 1, flags))
        return 2;

    return 0;
}

static int addService(struct alternative newAlt, const char *altDir,
                      const char *stateDir, int flags) {
    struct alternativeSet set;
    struct alternative template;
    struct alternative *alt = NULL;

    int i, rc;
    int forceLinks = 0;

    if ((rc = readConfig(&set, newAlt.leader.title, altDir, stateDir, flags)) &&
        rc != 3 && rc != 2)
        return 2;

    if (set.numAlts) {
        if (strcmp(newAlt.leader.facility, set.alts[0].leader.facility)) {
            fprintf(stderr, _("the primary link for %s must be %s\n"),
                    set.alts[0].leader.title, set.alts[0].leader.facility);
            return 2;
        }

        /* Determine the maximal set of follower links. */
        fillTemplateFrom(set.alts[0], &template);
        for (i = 0; i < newAlt.numFollowers; i++)
            addFollowerToAlternative(&template, newAlt.followers[i]);

        alt = findAlternativeInSet(set, newAlt.leader.target);

        if (alt) {
            *alt = newAlt;
            forceLinks = 1;
            /* Check for followers no alternative provides */
            removeUnusedFollowersFromTemplate(set, &template, altDir, flags);
        } else {
            set.alts = realloc(set.alts, sizeof(*set.alts) * (set.numAlts + 1));
            set.alts[set.numAlts] = newAlt;
            if (set.alts[set.best].priority < newAlt.priority)
                set.best = set.numAlts;
            set.numAlts++;
        }

        if (matchFollowers(&set, template))
            return 2;
    } else {
        set.alts = realloc(set.alts, sizeof(*set.alts) * (set.numAlts + 1));
        set.alts[set.numAlts] = newAlt;
        if (set.alts[set.best].priority < newAlt.priority)
            set.best = set.numAlts;
        set.numAlts++;
    }

    if (writeState(&set, altDir, stateDir, forceLinks, flags))
        return 2;

    return 0;
}

static int displayService(char *title, const char *altDir, const char *stateDir,
                          int flags) {
    struct alternativeSet set;

    int alt;
    int follower;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    if (set.mode == AUTO)
        printf(_("%s - status is auto.\n"), title);
    else
        printf(_("%s - status is manual.\n"), title);

    printf(_(" link currently points to %s\n"), set.currentLink);

    for (alt = 0; alt < set.numAlts; alt++) {
        printf("%s - ", set.alts[alt].leader.target);
        if (set.alts[alt].family)
            printf(_("family %s "), set.alts[alt].family);
        printf(_("priority %d\n"), set.alts[alt].priority);
        for (follower = 0; follower < set.alts[alt].numFollowers; follower++) {
            printf(_(" follower %s: %s\n"), set.alts[alt].followers[follower].title,
                   set.alts[alt].followers[follower].target);
        }
    }

    printf(_("Current `best' version is %s.\n"),
           set.alts[set.best].leader.target);

    return 0;
}

static int autoService(char *title, const char *altDir, const char *stateDir,
                       int flags) {
    struct alternativeSet set;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;
    set.current = set.best;
    set.mode = AUTO;

    if (writeState(&set, altDir, stateDir, 0, flags))
        return 2;

    return 0;
}

static int configService(char *title, const char *altDir, const char *stateDir,
                         int flags) {
    struct alternativeSet set;
    int i;
    char choice[200];
    char *end = NULL;
    char *nicer = NULL;
    ;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    do {
        printf("\n");
        printf(ngettext(_("There is %d program that provides '%s'.\n"),
                        _("There are %d programs which provide '%s'.\n"),
                        set.numAlts),
               set.numAlts, set.alts[0].leader.title);
        printf("\n");
        printf(_("  Selection    Command\n"));
        printf("-----------------------------------------------\n");

        for (i = 0; i < set.numAlts; i++) {
            if (set.alts[i].family)
                asprintf(&nicer, "%s (%s)", set.alts[i].family,
                         set.alts[i].leader.target);
            printf("%c%c %-4d        %s\n", i == set.best ? '*' : ' ',
                   i == set.current ? '+' : ' ', i + 1,
                   nicer ?: set.alts[i].leader.target);
            free(nicer);
            nicer = NULL;
            ;
        }
        printf("\n");
        printf(_("Enter to keep the current selection[+], or type selection "
                 "number: "));

        if (!fgets(choice, sizeof(choice), stdin)) {
            fprintf(stderr, _("\nerror reading choice\n"));
            return 2;
        }
        i = strtol(choice, &end, 0);
        if ((*end == '\n') && (end != choice)) {
            set.current = i - 1;
        }
    } while (!end || *end != '\n' || (set.current < 0) ||
             (set.current >= set.numAlts));

    set.mode = MANUAL;
    if (writeState(&set, altDir, stateDir, 1, flags))
        return 2;

    return 0;
}

static int setService(const char *title, const char *target, const char *altDir,
                      const char *stateDir, int flags) {
    struct alternativeSet set;
    int found = -1;
    int i, r;

    r = readConfig(&set, title, altDir, stateDir, flags);
    if (r) {
        if (r == 3) {
            fprintf(stderr,
                _("cannot access %s/%s: No such file or directory\n"), stateDir, title);
        }
        return 2;
    }

    for (i = 0; i < set.numAlts; i++)
        if (!strcmp(set.alts[i].leader.target, target)) {
            found = i;
            break;
        }

    if (found == -1)
        for (i = 0; i < set.numAlts; i++)
            if (set.alts[i].family && !strcmp(set.alts[i].family, target))
                if (found == -1 ||
                    (set.alts[i].priority > set.alts[found].priority))
                    found = i;

    if (found == -1) {
        fprintf(stderr,
                _("%s has not been configured as an alternative for %s\n"),
                target, title);
        return 2;
    }

    set.current = found;
    set.mode = MANUAL;

    if (writeState(&set, altDir, stateDir, 1, flags))
        return 2;

    return 0;
}

static int removeService(const char *title, const char *target,
                         const char *altDir, const char *stateDir, int flags) {
    int rc;
    struct alternativeSet set;
    int i;
    char *family = NULL;
    int forceLinks = 0;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    for (i = 0; i < set.numAlts; i++)
        if (!strcmp(set.alts[i].leader.target, target))
            break;

    if (i == set.numAlts) {
        fprintf(stderr,
                _("%s has not been configured as an alternative for %s\n"),
                target, title);
        return 2;
    }

    if (set.numAlts == 1) {
        char *path;

        rc = removeLinks(&set.alts[0].leader, altDir, flags);

        for (i = 0; i < set.alts[0].numFollowers; i++)
            rc |= removeLinks(set.alts[0].followers + i, altDir, flags);

        path = alloca(strlen(stateDir) + strlen(title) + 2);
        sprintf(path, "%s/%s", stateDir, title);
        if (FL_TEST(flags)) {
            printf(_("(would remove %s\n"), path);
        } else if (unlink(path)) {
            fprintf(stderr, _("failed to remove %s: %s\n"), path,
                    strerror(errno));
            rc |= 1;
        }

        if (rc)
            return 2;
        else
            return 0;
    }

    if (set.current != -1)
        family = set.alts[set.current].family;

    /* If the current link is what we're removing, reset it. */
    if (set.current == i)
        set.current = -1;

    /* Replace removed link set with last one */
    set.alts[i] = set.alts[set.numAlts - 1];
    if (set.current == (set.numAlts - 1))
        set.current = i;
    set.numAlts--;

    set.best = 0;
    for (i = 0; i < set.numAlts; i++)
        if (altBetter(set.alts[i], set.alts[set.best], family))
            set.best = i;

    if (set.current == -1) {
        if (!family || !streq(family, set.alts[set.best].family))
            set.mode = AUTO;
        else
            forceLinks = 1;
        set.current = set.best;
    }

    if (writeState(&set, altDir, stateDir, forceLinks, flags))
        return 2;

    return 0;
}

static int removeAll(const char *title, const char *altDir,
                     const char *stateDir, int flags) {
    struct alternativeSet set;

    int alt;
    int ret_val = 0;

    if (readConfig(&set, title, altDir, stateDir, flags))
        return 2;

    for (alt = 0; alt < set.numAlts; alt++) {
        ret_val += removeService(title, set.alts[alt].leader.target, altDir,
                                 stateDir, flags);
    }

    return (ret_val > 1) ? 2 : 0;
}

static int listServices(const char *altDir, const char *stateDir, int flags) {
    DIR *dir;
    struct dirent *ent;
    struct alternativeSet set;
    int max_name = 0;
    int l;

    dir = opendir(stateDir);
    if (dir == NULL)
        return 2;

    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        l = strlen(ent->d_name);
        max_name = max_name > l ? max_name : l;
    }

    rewinddir(dir);

    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        if (readConfig(&set, ent->d_name, altDir, stateDir, flags)) {
            closedir(dir);
            return 2;
        }

        printf("%-*s\t%s\t%s\n", max_name, ent->d_name,
               set.mode == AUTO ? "auto  " : "manual", set.currentLink);
    }

    closedir(dir);

    return 0;
}

int main(int argc, const char **argv) {
    const char **nextArg;
    char *end;
    char *title = NULL;
    char *target = NULL;
    char *followerTitle = NULL;
    enum programModes mode = MODE_UNKNOWN;
    struct alternative newAlt = {-1, {NULL, NULL, NULL}, NULL, NULL, 0, NULL};
    int flags = 0;
    char *altDir = "/etc/alternatives";
    char *stateDir = "/var/lib/alternatives";
    struct stat sb;
    struct linkSet newSet = {NULL, NULL, NULL};

    setlocale(LC_ALL, "");
    bindtextdomain("chkconfig", "/usr/share/locale");
    textdomain("chkconfig");

    if (!argv[1])
        return usage(2);

    nextArg = argv + 1;
    while (*nextArg) {
        if (!strcmp(*nextArg, "--install")) {
            if (mode != MODE_UNKNOWN && mode != MODE_FOLLOWER)
                usage(2);
            mode = MODE_INSTALL;
            nextArg++;

            setupLinkSet(&newAlt.leader, &nextArg);

            if (!*nextArg)
                usage(2);
            newAlt.priority = strtol(*nextArg, &end, 0);
            if (!end || *end)
                usage(2);
            nextArg++;
        } else if (!strcmp(*nextArg, "--add-follower") || !strcmp(*nextArg, "--add-slave")) {
            setupDoubleArg(&mode, &nextArg, MODE_ADD_FOLLOWER, &title, &target);
            setupLinkSet(&newSet, &nextArg);
        } else if (!strcmp(*nextArg, "--remove-follower") || !strcmp(*nextArg, "--remove-slave")) {
            setupTripleArg(&mode, &nextArg, MODE_REMOVE_FOLLOWER, &title, &target, &followerTitle);
        } else if (!strcmp(*nextArg, "--follower") || !strcmp(*nextArg, "--slave")) {
            if (mode != MODE_UNKNOWN && mode != MODE_INSTALL)
                usage(2);
            if (mode == MODE_UNKNOWN)
                mode = MODE_FOLLOWER;
            nextArg++;

            newAlt.followers = realloc(newAlt.followers, sizeof(*newAlt.followers) *
                                                       (newAlt.numFollowers + 1));
            setupLinkSet(newAlt.followers + newAlt.numFollowers, &nextArg);
            newAlt.numFollowers++;
        } else if (!strcmp(*nextArg, "--initscript")) {
            if (mode != MODE_UNKNOWN && mode != MODE_INSTALL)
                usage(2);
            nextArg++;

            if (!*nextArg)
                usage(2);
            newAlt.initscript = strdup(*nextArg);
            nextArg++;
        } else if (!strcmp(*nextArg, "--family")) {
            if (mode != MODE_UNKNOWN && mode != MODE_INSTALL)
                usage(2);
            nextArg++;

            if (!*nextArg)
                usage(2);
            newAlt.family = strdup(*nextArg);

            if (strchr(newAlt.family, '@')) {
                printf(_("--family can't contain the symbol '@'\n"));
                usage(2);
            }
            nextArg++;
        } else if (!strcmp(*nextArg, "--remove")) {
            setupDoubleArg(&mode, &nextArg, MODE_REMOVE, &title, &target);
        } else if (!strcmp(*nextArg, "--remove-all")) {
            setupSingleArg(&mode, &nextArg, MODE_REMOVE_ALL, &title);
        } else if (!strcmp(*nextArg, "--set")) {
            setupDoubleArg(&mode, &nextArg, MODE_SET, &title, &target);
        } else if (!strcmp(*nextArg, "--auto")) {
            setupSingleArg(&mode, &nextArg, MODE_AUTO, &title);
        } else if (!strcmp(*nextArg, "--display")) {
            setupSingleArg(&mode, &nextArg, MODE_DISPLAY, &title);
        } else if (!strcmp(*nextArg, "--config")) {
            setupSingleArg(&mode, &nextArg, MODE_CONFIG, &title);
        } else if (!strcmp(*nextArg, "--help") ||
                   !strcmp(*nextArg, "--usage")) {
            if (mode != MODE_UNKNOWN)
                usage(2);
            mode = MODE_USAGE;
            nextArg++;
        } else if (!strcmp(*nextArg, "--test")) {
            flags |= FLAGS_TEST;
            nextArg++;
        } else if (!strcmp(*nextArg, "--verbose")) {
            flags |= FLAGS_VERBOSE;
            nextArg++;
        } else if (!strcmp(*nextArg, "--keep-missing")) {
            flags |= FLAGS_KEEP_MISSING;
            nextArg++;
        } else if (!strcmp(*nextArg, "--keep-foreign")) {
            flags |= FLAGS_KEEP_FOREIGN;
            nextArg++;
        } else if (!strcmp(*nextArg, "--version")) {
            if (mode != MODE_UNKNOWN)
                usage(2);
            mode = MODE_VERSION;
            nextArg++;
        } else if (!strcmp(*nextArg, "--altdir")) {
            nextArg++;
            if (!*nextArg)
                usage(2);
            altDir = normalize_path_alloc(*nextArg);
            nextArg++;
        } else if (!strcmp(*nextArg, "--admindir")) {
            nextArg++;
            if (!*nextArg)
                usage(2);
            stateDir = normalize_path_alloc(*nextArg);
            nextArg++;
        } else if (!strcmp(*nextArg, "--list")) {
            if (mode != MODE_UNKNOWN)
                usage(2);
            mode = MODE_LIST;
            nextArg++;
        } else {
            usage(2);
        }
    }

    if (stat(altDir, &sb) || !S_ISDIR(sb.st_mode) || access(altDir, F_OK)) {
        fprintf(stderr, _("altdir %s invalid\n"), altDir);
        exit(2);
    }

    if (stat(stateDir, &sb) || !S_ISDIR(sb.st_mode) || access(stateDir, F_OK)) {
        fprintf(stderr, _("admindir %s invalid\n"), stateDir);
        exit(2);
    }

    switch (mode) {
    case MODE_UNKNOWN:
        usage(2);
    case MODE_USAGE:
        usage(0);
    case MODE_VERSION:
        printf(_("alternatives version %s\n"), VERSION);
        exit(0);
    case MODE_INSTALL:
        return addService(newAlt, altDir, stateDir, flags);
    case MODE_ADD_FOLLOWER:
        return addFollower(title, target, newSet, altDir, stateDir, flags);
    case MODE_REMOVE_FOLLOWER:
        return removeFollower(title, target, followerTitle, altDir, stateDir, flags);
    case MODE_DISPLAY:
        return displayService(title, altDir, stateDir, flags);
    case MODE_AUTO:
        return autoService(title, altDir, stateDir, flags);
    case MODE_CONFIG:
        return configService(title, altDir, stateDir, flags);
    case MODE_SET:
        return setService(title, target, altDir, stateDir, flags);
    case MODE_REMOVE:
        return removeService(title, target, altDir, stateDir, flags);
    case MODE_REMOVE_ALL:
        return removeAll(title, altDir, stateDir, flags);
    case MODE_FOLLOWER:
        usage(2);
    case MODE_LIST:
        return listServices(altDir, stateDir, flags);
    }

    abort();
}
