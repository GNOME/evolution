/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-html-utils.c
* 
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Ettore Perazzoli <ettore@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-corba-utils.h"


const CORBA_char *
e_safe_corba_string (const char *s)
{
	if (s == NULL)
		return (CORBA_char *) "";

	return s;
}

CORBA_char *
e_safe_corba_string_dup (const char *s)
{
	return CORBA_string_dup (e_safe_corba_string (s));
}
