#include <alloca.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libintl.h> 
#include <locale.h> 
#include <newt.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define _(String) gettext((String)) 

#include "leveldb.h"

/* return 1 on cancel, 2 on error, 0 on success */
static int servicesWindow(struct service * services, int numServices,
			  int levels, int backButton) {
    newtComponent label, subform, ok, cancel;
    newtComponent * checkboxes, form, curr, blank;
    newtComponent sb = NULL;
    newtGrid grid, subgrid, buttons;
    char * states;
    int i, done = 0, update = 0, j;
    struct newtExitStruct e;

    newtPushHelpLine(_("Press <F1> for more information on a service."));

    sb = newtVerticalScrollbar(-1, -1, 8, NEWT_COLORSET_CHECKBOX,
				    NEWT_COLORSET_ACTCHECKBOX);

    subform = newtForm(sb, NULL, 0);
    newtFormSetBackground(subform, NEWT_COLORSET_CHECKBOX);
    newtFormSetHeight(subform, 8);

    checkboxes = alloca(sizeof(*checkboxes) * numServices);
    states = alloca(sizeof(*states) * numServices);
    
    for (i = 0; i < numServices; i++) {
	for (j = 0; j < 7; j++) {
	    if (levels & (1 << j)) {
		if (isOn(services[i].name, j)) break;
	    }
	}
	checkboxes[i] = newtCheckbox(-1, i, services[i].name, 
				     (j != 7) ? '*' : ' ', NULL, 
				     states + i);
	newtFormAddComponent(subform, checkboxes[i]);
    }

    newtFormSetWidth(subform, 20);

    buttons = newtButtonBar(_("Ok"), &ok, backButton ? _("Back") : _("Cancel"),
			    &cancel, NULL);

    blank = newtForm(NULL, NULL, 0);
    newtFormSetWidth(blank, 2);
    newtFormSetHeight(blank, 8);
    newtFormSetBackground(blank, NEWT_COLORSET_CHECKBOX);

    subgrid = newtGridHCloseStacked(NEWT_GRID_COMPONENT, subform,
			            NEWT_GRID_COMPONENT, blank,
			            NEWT_GRID_COMPONENT, sb, NULL);

    label = newtTextboxReflowed(-1, -1,  _("What services should be "
				"automatically started?"), 30, 0, 20, 0);
    grid = newtGridBasicWindow(label, subgrid, buttons);

    form = newtForm(NULL, NULL, 0);
    newtGridAddComponentsToForm(grid, form, 1);
    newtGridWrappedWindow(grid, _("Services"));
    newtGridFree(grid, 1);

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
		    newtWinMessage(services[i].name, _("Ok"), services[i].desc);
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
	if (services[i].levels & (1 << j))
	  doSetService(services[i], j, states[i] == '*');
      }
    }

    return 0;
}

static int serviceNameCmp(const void * a, const void * b) {
    const struct service * first = a;
    const struct service * second = b;

    return strcmp(first->name, second->name);
}

static int getServices(struct service ** servicesPtr, int * numServicesPtr,	
		       int backButton) {
    DIR * dir;
    struct dirent * ent;
    struct stat sb;
    char fn[1024];
    struct service * services;
    int numServices = 0;
    int numServicesAlloced, rc;
    int err = 0;
    int j;

    numServicesAlloced = 10;
    services = malloc(sizeof(*services) * numServicesAlloced);

    if (!(dir = opendir(RUNLEVELS "/init.d"))) {
	fprintf(stderr, "failed to open " RUNLEVELS "/init.d: %s\n",
		strerror(errno));
        return 2;
    }

    while ((ent = readdir(dir))) {
	if (strchr(ent->d_name, '~') || strchr(ent->d_name, ',') ||
	    strchr(ent->d_name, '.')) continue;

	sprintf(fn, RUNLEVELS "/init.d/%s", ent->d_name);
	if (stat(fn, &sb))
	{
		err = errno;
		continue;
	}
	if (!S_ISREG(sb.st_mode)) continue;

	if (numServices == numServicesAlloced) {
	    numServicesAlloced += 10;
	    services = realloc(services, 
				numServicesAlloced * sizeof(*services));
	}

	rc = readServiceInfo(ent->d_name, services + numServices);
	fprintf(stderr, "got service %s, runlevels ",
		services[numServices].name);
	for (j = 0; j < 7; j++) {
	    if (services[numServices].levels & (1 << j)) {
	      fprintf(stderr,"%d ",j);
	    }
	}
	fprintf(stderr,"\n");

	if (rc == -1) {
	    fprintf(stderr, _("error reading info for service %s: %s\n"),
			ent->d_name, strerror(errno));
	    closedir(dir);
	    return 2;
	} else if (!rc)
	    numServices++;
    }

    if (err) {
	fprintf(stderr, _("error reading from directory %s/init.d: %s\n"),
		RUNLEVELS, strerror(err));
        return 1;
    }

    closedir(dir);

    qsort(services, numServices, sizeof(*services), serviceNameCmp);

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
    int rc, backButton = 0;
    struct poptOption optionsTable[] = {
	    { "back", '\0', 0, &backButton, 0 },
	    { "level", '\0', POPT_ARG_STRING, &levelsStr, 0 },
	    { 0, 0, 0, 0, 0 } 
    };

    setlocale(LC_ALL, ""); 
    bindtextdomain("chkconfig", "/usr/share/locale"); 
    textdomain("chkconfig"); 

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
	    fprintf(stderr, _("bad argument to --levels\n"));
	    exit(2);
	}
    }

    if (getServices(&services, &numServices, backButton)) return 1;
    if (!numServices) {
	fprintf(stderr, _("No services may be managed by ntsysv!\n"));
	return 2;
    }

    newtInit();
    newtCls();

    newtPushHelpLine(NULL);
    newtDrawRootText(0, 0, "ntsysv " VERSION " - (C) 1998 Red Hat "
			"Software");

    rc = servicesWindow(services, numServices, levels, backButton);

    newtFinished();

    return rc;
}
