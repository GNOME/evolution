/* Calendar properties dialog box
 *
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@kernel.org>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */
#include <config.h>
#ifdef HAVE_LANGINGO_H
#include <langinfo.h>
#else
#include <locale.h>
#endif
#include <gnome.h>
#include "calendar-commands.h"


char *
build_color_spec (int r, int g, int b)
{
	static char spec[100];

	sprintf (spec, "#%04x%04x%04x", r, g, b);
	return spec;
}

void
parse_color_spec (char *spec, int *r, int *g, int *b)
{
	g_return_if_fail (spec != NULL);
	g_return_if_fail (r != NULL);
	g_return_if_fail (r != NULL);
	g_return_if_fail (r != NULL);

	if (sscanf (spec, "#%04x%04x%04x", r, g, b) != 3) {
		g_warning ("Invalid color specification %s, returning black", spec);

		*r = *g = *b = 0;
	}
}

char *
color_spec_from_prop (ColorProp propnum)
{
	return build_color_spec (color_props[propnum].r, color_props[propnum].g, color_props[propnum].b);
}
