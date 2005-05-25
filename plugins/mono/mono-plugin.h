
#ifndef _ORG_GNOME_EVOLUTION_MONO_H
#define _ORG_GNOME_EVOLUTION_MONO_H

#include "e-util/e-plugin.h"

/* ********************************************************************** */
/* This is ALL private */

typedef struct _EPluginMono EPluginMono;
typedef struct _EPluginMonoClass EPluginMonoClass;

struct _EPluginMono {
	EPlugin plugin;

	struct _EPluginMonoPrivate *priv;

	char *location;		/* location */
	char *handler;		/* handler class */
};

struct _EPluginMonoClass {
	EPluginClass plugin_class;
};

void *org_gnome_evolution_mono_get_type(void *a, void *b);

#endif /* ! _ORG_GNOME_EVOLUTION_MONO_H */
