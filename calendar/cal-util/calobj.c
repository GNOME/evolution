/*
 * Calendar objects implementations.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *   Federico Mena (federico@gimp.org)
 */
#include "calobj.h"

iCalObject *
ical_object_new (void)
{
	iCalObject *ico;

	ico = g_new0 (iCalObject, 1);
	
	ico->seq = -1;
	ico->dtstamp = time (NULL);

	return ico;
}

iCalObject *
ical_new (char *comment, char *organizer, char *summary)
{
	iCalObject *ico;

	ico = ical_object_new ();

	ico->comment   = g_strdup (comment);
	ico->organizer = g_strdup (organizer);
	ico->summary   = g_strdup (summary);

	return ico;
}

#define free_if_defined(x) if (x){ g_free (x); x = 0; }

void
ical_object_destroy (iCalObject *ico)
{
	free_if_defined (ico->comment);
	free_if_defined (ico->organizer);
	free_if_defined (ico->summary);
	
	g_free (ico);
}
