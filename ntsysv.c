#include <alloca.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <newt.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "leveldb.h"

/* return 1 on cancel, 2 on error, 0 on success */
static int servicesWindow(struct service * services, int numServices,
			  int levels) {
    newtComponent label, subform, ok, cancel;
    newtComponent * checkboxes, form, curr;
    char * states;
    int i, done = 0, update = 0, j;
    int height = numServices > 10 ? 10 : numServices;
    struct newtExitStruct e;

    newtPushHelpLine("Press <F1> for more information on a service.");

    newtCenteredWindow(50, height + 8, "Services");

    label = newtLabel(1, 1, "What services should be automatically "
			"started?");

    subform = newtForm(NULL, NULL, 0);
    newtFormSetBackground(subform, NEWT_COLORSET_CHECKBOX);

    checkboxes = alloca(sizeof(*checkboxes) * numServices);
    states = alloca(sizeof(*states) * numServices);
    
    for (i = 0; i < numServices; i++) {
	for (j = 0; j < 7; j++) {
	    if (levels & (1 << j)) {
		if (isOn(services[i].name, j)) break;
	    }
	}
	checkboxes[i] = newtCheckbox(15, i + 3, services[i].name, 
				     (j != 7) ? '*' : ' ', NULL, 
				     states + i);
	newtFormAddComponent(subform, checkboxes[i]);
    }

    ok = newtButton(10, height + 4, "Ok");
    cancel = newtButton(25, height + 4, "Cancel");

    form = newtForm(NULL, NULL, 0);
    newtFormAddComponents(form, label, subform, ok, cancel, NULL);

    newtFormAddHotKey(form, NEWT_KEY_F1);

    while (!done) {
	newtFormRun(form, &e);

	if (e.reason == NEWT_EXIT_HOTKEY) {
	    if (e.u.key == NEWT_KEY_F12) {
		done = 1;
		update = 1;
	    } else {
		/* must be F1 */
		curr = newtFormGetCurrent(subform);
		for (i = 0; i < numServices; i++)
		    if (curr == checkboxes[i]) break;

		if (i < numServices) 
		    newtWinMessage(services[i].name, "Ok", services[i].desc);
	    }
	} else {
	    done = 1;
	    update = (e.u.co == ok);
	}
    }
    
    newtPopWindow();
    newtFormDestroy(form);
    
    if (!update) return 1;

    for (i = 0; i < numServices; i++) {
	for (j = 0; j < 7; j++) {
	    if ((1 << j) & levels)
		doSetService(services[i], j, states[i] == '*');
	}
    }

    return 0;
}

static int getServices(struct service ** servicesPtr, int * numServicesPtr) {
    DIR * dir;
    struct dirent * ent;
    struct stat sb;
    char fn[1024];
    struct service * services;
    int numServices = 0;
    int numServicesAlloced, rc;

    numServicesAlloced = 10;
    services = malloc(sizeof(*services) * numServicesAlloced);

    if (!(dir = opendir(RUNLEVELS "/init.d"))) {
	fprintf(stderr, "failed to open " RUNLEVELS "/init.d: %s\n",
		strerror(errno));
        return 2;
    }

    errno = 0;
    while ((ent = readdir(dir))) {
	if (strchr(ent->d_name, '~') || strchr(ent->d_name, ',') ||
	    strchr(ent->d_name, '.')) continue;

	sprintf(fn, RUNLEVELS "/init.d/%s", ent->d_name);
	if (stat(fn, &sb)) continue;
	if (!S_ISREG(sb.st_mode)) continue;

	if (numServices == numServicesAlloced) {
	    numServicesAlloced += 10;
	    services = realloc(services, 
				numServicesAlloced * sizeof(*services));
	}

	rc = readServiceInfo(ent->d_name, services + numServices);
	if (rc == -1) {
	    fprintf(stderr, "error reading info for service %s: %s\n",
			ent->d_name, strerror(errno));
	    closedir(dir);
	    return 2;
	} else if (!rc)
	    numServices++;
    }

    if (errno) {
        perror("error reading from directory " RUNLEVELS "/init.d");
        return 1;
    }

    closedir(dir);

    *servicesPtr = services;
    *numServicesPtr = numServices;

    return 0;
}

int main(int argc, char ** argv) {
    struct service * services;
    int numServices;
    int levels = (1 << 3) | (1 << 4) | (1 << 5);
    char * levelsStr = NULL;
    poptContext optCon;
    int rc;
    struct poptOption optionsTable[] = {
	    { "level", '\0', POPT_ARG_STRING, &levelsStr, 0 },
	    { 0, 0, 0, 0, 0 } 
    };

    optCon = poptGetContext("ntsysv", argc, argv, optionsTable, 0);
    poptReadDefaultConfig(optCon, 1);

    if ((rc = poptGetNextOpt(optCon)) < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(rc));
	exit(1);
    }

    if (levelsStr) {
	levels = parseLevels(levelsStr, 0);
	if (levels == -1) {
	    fprintf(stderr, "bad argument to --levels\n");
	    exit(2);
	}
    }

    if (getServices(&services, &numServices)) return 1;
    if (!numServices) {
	fprintf(stderr, "No services may be managed by ntsysv!\n");
	return 2;
    }

    newtInit();
    newtCls();

    newtPushHelpLine(NULL);
    newtDrawRootText(0, 0, "ntsysv " VERSION " - (C) 1997 Red Hat "
			"Software");

    rc = servicesWindow(services, numServices, levels);

    newtFinished();

    return rc;
}
