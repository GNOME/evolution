/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "mh-utils.h"

#include <sys/stat.h> 

gboolean
mh_is_a_message_file (const gchar *file_name, const gchar *file_path)
{
	struct stat stat_buf;
	gint stat_error = 0;
	gboolean ok;
	gchar *full_file_name;
	int i;
	
	/* test if the name is a number */
	i=0;
	while ((file_name[i] != '\0') && (file_name[i] >= '0') && (file_name[i] <= '9'))
		i++;
	if ((i==0) || (file_name[i] != '\0')) return FALSE;
	
	/* is it a regular file ? */
	full_file_name = g_strdup_printf ("%s/%s", file_path, file_name);
	stat_error = stat (full_file_name, &stat_buf);
	g_free (full_file_name);

	return  ((stat_error != -1) && S_ISREG (stat_buf.st_mode));
}

