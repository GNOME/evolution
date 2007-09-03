#include <libedataserver/e-categories.h>
#include <libecal/e-cal.h>
#include <pi-appinfo.h>

#define PILOT_MAX_CATEGORIES 16


int e_pilot_add_category_if_possible(char *cat_to_add, struct CategoryAppInfo *category);
void e_pilot_local_category_to_remote(int * pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
void e_pilot_remote_category_to_local(int   pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
#include <libedataserver/e-categories.h>
#include <libecal/e-cal.h>
#include <pi-appinfo.h>

#define PILOT_MAX_CATEGORIES 16


int e_pilot_add_category_if_possible(char *cat_to_add, struct CategoryAppInfo *category);
void e_pilot_local_category_to_remote(int * pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
void e_pilot_remote_category_to_local(int   pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
