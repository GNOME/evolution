/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-recipient.h : handle recipients (addresses) and recipiemt lists */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-recipient.h"


/**
 * camel_recipient_table_new: Create a new recipient table object
 * 
 * 
 * creates a new recipient table object. A recipient table 
 * objects merely associates a recipient list (GList) to
 * recipient types (as for example "To", "Cc" for mime
 * maile messages 
 *
 * Return value: the newly created recipient table object
 **/
CamelRecipientTable *
camel_recipient_table_new ()
{
	CamelRecipientTable *recipient_table;

	recipient_table = g_new0 (CamelRecipientTable, 1);
	recipient_table->recipient_hash_table = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	recipient_table->ref_count = 1;
	return recipient_table;
}


/**
 * camel_recipient_table_ref: add a reference to a recipient table object
 * @recipient_table: the recipient table object
 * 
 * Add a reference to a recipient table object. 
 **/
void 
camel_recipient_table_ref (CamelRecipientTable *recipient_table)
{
	g_return_if_fail (recipient_table);
	recipient_table->ref_count += 1;
}



static void 
_free_recipient_list (gpointer key, gpointer value, gpointer user_data)
{
	GList *recipient_list = (GList *)value;
	gchar *recipient_name = (gchar *)key;

	while (recipient_list) {
		g_free (recipient_list->data);
		recipient_list = recipient_list->next;
	}

	g_list_free ((GList *)value);
	g_free (recipient_name);
	
}

/**
 * camel_recipient_table_free: Free a recipient table object
 * @recipient_table: the recipient table object to free
 * 
 * Free a recipient table object. All recipients and recipient
 * are freed. 
 **/
void 
camel_recipient_table_free (CamelRecipientTable *recipient_table)
{
	if (!recipient_table) return;

	/* free each recipient list */
	g_hash_table_foreach (recipient_table->recipient_hash_table, _free_recipient_list, NULL);
	g_hash_table_destroy (recipient_table->recipient_hash_table);
}




/**
 * camel_recipient_table_unref: Removes a reference to a recipient table object
 * @recipient_table: the recipient table object
 * 
 * Removes a reference to the reference count of a recipient
 * table object. If the reference count falls to zero, the 
 * recipient table object is freed.
 *
 **/
void 
camel_recipient_table_unref (CamelRecipientTable *recipient_table)
{
	if (!recipient_table) return;

	recipient_table->ref_count -= 1;
	if (recipient_table->ref_count <1)		
			camel_recipient_table_free (recipient_table);
		
}




/**
 * camel_recipient_table_add: Add a recipient to a recipient table object.
 * @recipient_table: The recipient table object
 * @recipient_type: Recipient type string
 * @recipient:  The recipient to add
 * 
 * Add a recipient to a recipient table object. 
 * The recipient is appended to the list of recipients
 * of type @recipient_type. @recipient and @recipient_type
 * are duplicated if necessary and freed when 
 * camel_recipient_table_free is called.
 **/
void 
camel_recipient_table_add (CamelRecipientTable *recipient_table, 
			   const gchar *recipient_type, 
			   const gchar *recipient)
{
	GList *recipients_list;
	GList *existent_list;
	
	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (recipient_table->recipient_hash_table, recipient_type);
	
	/* append the new recipient to the recipient list
	   if the existent_list is NULL, then a new GList is
	   automagically created */	
	recipients_list = g_list_append (existent_list, (gpointer)g_strdup (recipient));
	
	if (!existent_list) /* if there was no recipient of this type create the section */
		g_hash_table_insert (recipient_table->recipient_hash_table, g_strdup (recipient_type), recipients_list);
	
 
}


/**
 * camel_recipient_table_add_list: Add a full list of recipients to a recipient table.
 * @recipient_table: The recipient table object
 * @recipient_type:  Recipient type string.
 * @recipient_list: Recipient list to add.
 * 
 * Add a full list of recipients to a recipient table.
 * The new recipients are appended at the end of the
 * existing recipient list corresponding to @recipient_type.
 * Be careful, the list is used as is, and its element
 * will be freed by camel_recipient_table_unref
 **/
void 
camel_recipient_table_add_list (CamelRecipientTable *recipient_table, 
				const gchar *recipient_type, 
				GList *recipient_list)
{
	GList *existent_list;

	/* see if there is already a list for this recipient type */
	existent_list = (GList *)g_hash_table_lookup (recipient_table->recipient_hash_table, recipient_type);

	if (existent_list) 
		g_list_concat (existent_list, recipient_list);
	else 
		g_hash_table_insert (recipient_table->recipient_hash_table, g_strdup (recipient_type), recipient_list);		

}




/**
 * camel_recipient_table_remove: Remove a recipient from a recipient table.
 * @recipient_table: The recipient table object
 * @recipient_type: Recipient type string.
 * @recipient: Recipient to remove from the table
 * 
 * Remove a recipient from a recipient table. The recipient is
 * only removed from the recipient list corresponding to 
 * @recipient_type. The removed recipient is freed.
 *
 **/
void
camel_recipient_table_remove (CamelRecipientTable *recipient_table,
			      const gchar *recipient_type,
			      const gchar *recipient) 
{
	GList *recipients_list;
	GList *new_recipients_list;
	GList *old_element;
	gchar *old_recipient_type;
	
	/* if the recipient type section does not exist, do nothing */
	if (! g_hash_table_lookup_extended (recipient_table->recipient_hash_table, 
					    recipient_type, 
					    (gpointer)&(old_recipient_type),
					    (gpointer)&(recipients_list)) 
	    ) return;
	
	/* look for the recipient to remove */
	/* g_list_find_custom , use gpointer instead of gconstpointer */
	old_element = g_list_find_custom (recipients_list, (gpointer)recipient, g_strcase_equal);
	if (old_element) {
		/* if recipient exists, remove it */
		new_recipients_list =  g_list_remove_link (recipients_list, old_element);
		
		/* if glist head has changed, fix up hash table */
		if (new_recipients_list != recipients_list)
			g_hash_table_insert (recipient_table->recipient_hash_table, old_recipient_type, new_recipients_list);
		
		g_free( (gchar *)(old_element->data));
		g_list_free_1 (old_element);
	}
}



/**
 * camel_recipient_table_get: Get the recipients corresponding to a recipient type.
 * @recipient_table: The recipient table object
 * @recipient_type: Recipient type string.
 * 
 * Return the list of recipients corresponding to 
 * @recipient_type. The returned list is not a copy 
 * of the internal list used by the recipient table object
 * but the list itself. It thus must not be freed.
 * The recipients it contains can be modified.
 * 
 * Return value: The list of recipients.
 **/
const GList *
camel_recipient_table_get (CamelRecipientTable *recipient_table,
			   const gchar *recipient_type)
{
	return (const GList *)g_hash_table_lookup (recipient_table->recipient_hash_table, recipient_type);
}




/**
 * camel_recipient_foreach_recipient_type: Runs a function over over all recipients type lists.
 * @recipient_table: The recipient table object.
 * @func: The function to run.
 * @user_data: User data to pass to the function.
 * 
 * Runs a function over over all recipients type lists.
 **/
void
camel_recipient_foreach_recipient_type (CamelRecipientTable *recipient_table,
					CRLFunc func,
					gpointer user_data)
{
	g_hash_table_foreach (recipient_table->recipient_hash_table, (GHFunc)func, user_data);
}
