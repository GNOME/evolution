/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-recipient.h : handle recipients (addresses) and recipiemt lists */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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



#ifndef CAMEL_RECIPIENT_H
#define CAMEL_RECIPIENT_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>



typedef struct {
	GHashTable *recipient_hash_table;
	gint ref_count;

} CamelRecipientTable;


typedef void (*CRLFunc) (gchar *recipient_type,
			 GList *recipient_list,
			 gpointer user_data);




CamelRecipientTable *camel_recipient_table_new ();

void camel_recipient_table_ref (CamelRecipientTable *recipient_table);

void camel_recipient_table_unref (CamelRecipientTable *recipient_table);

void camel_recipient_table_add (CamelRecipientTable *recipient_table, 
				const gchar *recipient_type, 
				const gchar *recipient);

void camel_recipient_table_add_list (CamelRecipientTable *recipient_table, 
				     const gchar *recipient_type, 
				     GList *recipient_list);

void camel_recipient_table_remove (CamelRecipientTable *recipient_table,
				   const gchar *recipient_type,
				   const gchar *recipient);
void camel_recipient_table_remove_type (CamelRecipientTable *recipient_table,
					const gchar *recipient_type);
const GList *camel_recipient_table_get (CamelRecipientTable *recipient_table,
					const gchar *recipient_type);

void camel_recipient_foreach_recipient_type (CamelRecipientTable *recipient_table,
					     CRLFunc func,
					     gpointer user_data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_RECIPIENT_H */

