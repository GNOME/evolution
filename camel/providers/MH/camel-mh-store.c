/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mh-store.c : class for an mh store */

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

#include "camel-mh-store.h"
#include "camel-mh-folder.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelMhStore */
#define CS_CLASS(so) CAMEL_MH_STORE_CLASS (GTK_OBJECT(so)->klass)



static void
camel_mh_store_class_init (CamelMhStoreClass *camel_mh_store_class)
{
	parent_class = gtk_type_class (camel_store_get_type ());
	
	/* virtual method definition */
	/* virtual method overload */
}







GtkType
camel_mh_store_get_type (void)
{
	static GtkType camel_mh_store_type = 0;
	
	if (!camel_mh_store_type)	{
		GtkTypeInfo camel_mh_store_info =	
		{
			"CamelMhStore",
			sizeof (CamelMhStore),
			sizeof (CamelMhStoreClass),
			(GtkClassInitFunc) camel_mh_store_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mh_store_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_mh_store_info);
	}
	
	return camel_mh_store_type;
}




/** These evil public functions are here for test only **/
void 
camel_mh_store_set_toplevel_dir(CamelMhStore *store, GString *toplevel)
{
	store->toplevel_dir = toplevel;
	CAMEL_STORE(store)->separator = '/';
}


GString *
camel_mh_store_get_toplevel_dir(CamelMhStore *store)
{
	return store->toplevel_dir;
}
