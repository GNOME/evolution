/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libxml/parser.h>
#include <pi-util.h>

#include "e-pilot-util.h"

char *
e_pilot_utf8_to_pchar (const char *string)
{
	char *pstring = NULL;
	int res;

	if (!string)
		return NULL;
	
	res = convert_ToPilotChar ("UTF-8", string, strlen (string), &pstring);

	if (res != 0)
		pstring = strdup (string);

	return pstring;
}

char *
e_pilot_utf8_from_pchar (const char *string)
{
	char *ustring = NULL;
	int res;

	if (!string)
		return NULL;
	
	res = convert_FromPilotChar ("UTF-8", string, strlen (string), &ustring);
	
	if (res != 0)
		ustring = strdup (string);
	
	return ustring;
}
