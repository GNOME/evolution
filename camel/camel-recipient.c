/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-recipient.h : handle recipients (addresses) and recipiemt lists */

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


#include "glib.h"
#include "hash-table-utils.h"


CamelRecipientTable *
camel_recipient_new ()
{
	CamelRecipientTable *recipient_table;

	recipient_table = g_new0 (CamelRecipientTable, 1);
	recipient_table->recipient_table = g_hash_table_new (g_strcase_cmp, g_strcase_hash);
	recipient_table->ref_count = 1;
	return recipient_table;
}


void 
camel_recipient_ref (CamelRecipientTable *recipient_table)
{
	g_return_if_fail (recipient_table);
	recipient_table->ref_count += 1;
}



static void 
_free_recipient_list (gpointer key, gpointer value, gpointer user_data)
{
	GList *recipient_list = (GList *)value;
	gchar *recipient_name = (gchar *key);

	while (recipient_list) {
		g_free (recipient_list->data);
		recipient_list = recipient_list->next
	}

	g_free (recipient_name);
	
}

void 
camel_recipient_free (CamelRecipientTable *recipient_table)
{
	g_return_if_fail (recipient_table);

	/* free each recipient list */
	g_hash_table_foreach (recipient_table->recipient_table, _free_recipient_list);
	g_hash_table_destroy (recipient_table->recipient_table);
}




void 
camel_recipient_unref (CamelRecipientTable *recipient_table)
{
	g_return_if_fail (recipient_table);
	recipient_table->ref_count -= 1;
	if (recipient_table->ref_count <1)		
			camel_recipient_free (recipient_table);
		
}



void 
camel_recipient_add (CamelRecipientTable *recipient_table, 
		     const gchar *recipient_type, 
		     const gchar *recipient)
{
	GList *recipients_list;
	GList *existent_list;
	
	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (recipient_table->recipient_table, recipient_type);
	
	
	/* append the new recipient to the recipient list
	   if the existent_list is NULL, then a new GList is
	   automagically created */	
	recipients_list = g_list_append (existent_list, (gpointer)recipient);
	
	if (!existent_list) /* if there was no recipient of this type create the section */
		g_hash_table_insert (mime_message->recipients, recipient_type, recipients_list);
	else 
		g_free (recipient_type);

}
