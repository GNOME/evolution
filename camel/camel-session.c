/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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

#include "camel-session.h"
#include "gstring-util.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelSession */
#define CSS_CLASS(so) CAMEL_SESSION_CLASS (GTK_OBJECT(so)->klass)


static void
camel_session_class_init (CamelSessionClass *camel_session_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	/* virtual method overload */
}







GtkType
camel_session_get_type (void)
{
	static GtkType camel_session_type = 0;
	
	if (!camel_session_type)	{
		GtkTypeInfo camel_session_info =	
		{
			"CamelSession",
			sizeof (CamelSession),
			sizeof (CamelSessionClass),
			(GtkClassInitFunc) camel_session_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_session_type = gtk_type_unique (gtk_object_get_type (), &camel_session_info);
	}
	
	return camel_session_type;
}

