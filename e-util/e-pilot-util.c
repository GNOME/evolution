/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: JP Rosevear <jpr@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <time.h>
#include <gnome.h>
#include <gnome-xml/parser.h>
#include <pi-util.h>
#include <e-pilot-util.h>

char *
e_pilot_utf8_to_pchar (const char *string)
{
	char *pstring = NULL;
	int res;
	
	res = convert_ToPilotChar ("UTF8", string, strlen (string), &pstring);

	if (res != 0)
		pstring = strdup (string);

	return pstring;
}

char *
e_pilot_utf8_from_pchar (const char *string)
{
	char *ustring = NULL;
	int res;
	
	res = convert_FromPilotChar ("UTF8", string, strlen (string), &ustring);
	
	if (res != 0)
		ustring = strdup (ustring);
	
	return ustring;
}
