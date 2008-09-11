/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gnome.h>
#include "e-charset-picker.h"

int
main (int argc, char **argv)
{
	char *charset;

	gnome_init ("test-charset-picker", "1.0", argc, argv);

	charset = e_charset_picker_dialog ("test-charset-picker",
					   "Pick a charset, any charset",
					   NULL, NULL);
	if (charset)
		printf ("You picked: %s\n", charset);

	return 0;
}
