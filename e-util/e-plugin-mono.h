
#ifndef _E_PLUGIN_MONO_H
#define _E_PLUGIN_MONO_H

#include "e-plugin.h"

/* ********************************************************************** */

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

GType e_plugin_mono_get_type(void);

#endif /* ! _E_PLUGIN_MONO_H */
