/*
 *  Routines to copy information from a Gaim buddy list into an
 *  Evolution addressbook.
 *
 *  I currently copy IM account names and buddy icons, provided you
 *  don't already have a buddy icon defined for a person.
 *
 *  This works today (25 October 2004), but is pretty sure to break
 *  later on as the Gaim buddylist file format shifts.
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
 * Authors:
 *		Nat Friedman <nat@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <e-util/e-xml-utils.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-combo-box.h>

#include <sys/time.h>
#include <sys/stat.h>

#include <e-util/e-config.h>

#include "bbdb.h"

typedef struct {
	gchar *account_name;
	gchar *proto;
	gchar *alias;
	gchar *icon;
} GaimBuddy;

/* Forward declarations for this file. */
static gboolean bbdb_merge_buddy_to_contact (EBook *book, GaimBuddy *b, EContact *c);
static GList *bbdb_get_gaim_buddy_list (void);
static gchar *get_node_text (xmlNodePtr node);
static gchar *get_buddy_icon_from_setting (xmlNodePtr setting);
static void free_buddy_list (GList *blist);
static void parse_buddy_group (xmlNodePtr group, GList **buddies, GSList *blocked);
static EContactField proto_to_contact_field (const gchar *proto);

void
bbdb_sync_buddy_list_check (void)
{
	GConfClient *gconf;
	struct stat statbuf;
	time_t last_sync;
	gchar *blist_path;
	gchar *last_sync_str;

	gconf = gconf_client_get_default ();

	blist_path = g_build_path ("/", getenv ("HOME"), ".purple/blist.xml", NULL);
	if (stat (blist_path, &statbuf) < 0) {
		g_free (blist_path);
		g_object_unref (G_OBJECT (gconf));
		return;
	}

	g_free (blist_path);

	/* Reprocess the buddy list if it's been updated. */
	last_sync_str = gconf_client_get_string (gconf, GCONF_KEY_GAIM_LAST_SYNC, NULL);
	if (last_sync_str == NULL || ! strcmp ((const gchar *)last_sync_str, ""))
		last_sync = (time_t) 0;
	else
		last_sync = (time_t) g_ascii_strtoull (last_sync_str, NULL, 10);

	g_free (last_sync_str);
	g_object_unref (G_OBJECT (gconf));

	if (statbuf.st_mtime > last_sync) {
		fprintf (stderr, "bbdb: Buddy list has changed since last sync.\n");

		bbdb_sync_buddy_list ();
	}
}

void
bbdb_sync_buddy_list (void)
{
	GList       *blist, *l;
	EBook       *book = NULL;

	/* Get the Gaim buddy list */
	blist = bbdb_get_gaim_buddy_list ();
	if (blist == NULL)
		return;

	/* Open the addressbook */
	book = bbdb_open_addressbook (GAIM_ADDRESSBOOK);
	if (book == NULL) {
		free_buddy_list (blist);
		return;
	}

	printf ("bbdb: Synchronizing buddy list to contacts...\n");
	/* Walk the buddy list */
	for (l = blist; l != NULL; l = l->next) {
		GaimBuddy *b = l->data;
		EBookQuery *query;
		GList *contacts;
		GError *error = NULL;
		EContact *c;

		if (b->alias == NULL || strlen (b->alias) == 0)
			b->alias = b->account_name;

		/* Look for an exact match full name == buddy alias */
		query = e_book_query_field_test (E_CONTACT_FULL_NAME, E_BOOK_QUERY_IS, b->alias);
		e_book_get_contacts (book, query, &contacts, NULL);
		e_book_query_unref (query);
		if (contacts != NULL) {

			/* FIXME: If there's more than one contact with this
			   name, just give up; we're not smart enough for
			   this. */
			if (contacts->next != NULL)
				continue;

			c = E_CONTACT (contacts->data);

			if (! bbdb_merge_buddy_to_contact (book, b, c))
				continue;

			/* Write it out to the addressbook */
			if (! e_book_commit_contact (book, c, &error)) {
				g_warning ("bbdb: Could not modify contact: %s\n", error->message);
				g_error_free (error);
			}
			continue;
		}

		/* Otherwise, create a new contact. */
		c = e_contact_new ();
		e_contact_set (c, E_CONTACT_FULL_NAME, (gpointer) b->alias);
		if (! bbdb_merge_buddy_to_contact (book, b, c)) {
			g_object_unref (G_OBJECT (c));
			continue;
		}

		if (! e_book_add_contact (book, c, &error)) {
			g_warning ("bbdb: Failed to add new contact: %s\n", error->message);
			g_error_free (error);
			return;
		}
		g_object_unref (G_OBJECT (c));

	}

	/* Update the last-sync'd time */
	{
		GConfClient *gconf;
		time_t  last_sync;
		gchar   *last_sync_str;

		gconf = gconf_client_get_default ();

		time (&last_sync);
		last_sync_str = g_strdup_printf ("%ld", (glong) last_sync);
		gconf_client_set_string (gconf, GCONF_KEY_GAIM_LAST_SYNC, last_sync_str, NULL);
		g_free (last_sync_str);

		g_object_unref (G_OBJECT (gconf));
	}
	printf ("bbdb: Done syncing buddy list to contacts.\n");
}

static gboolean
im_list_contains_buddy (GList *ims, GaimBuddy *b)
{
	GList *l;

	for (l = ims; l != NULL; l = l->next) {
		gchar *im = (gchar *) l->data;

		if (! strcmp (im, b->account_name))
			return TRUE;
	}

	return FALSE;
}

static gboolean
bbdb_merge_buddy_to_contact (EBook *book, GaimBuddy *b, EContact *c)
{
	EContactField field;
	GList *ims, *l;
	gboolean dirty = FALSE;

	EContactPhoto *photo = NULL;

	GError *error = NULL;

	/* Set the IM account */
	field = proto_to_contact_field (b->proto);
	ims = e_contact_get (c, field);
	if (! im_list_contains_buddy (ims, b)) {
		ims = g_list_append (ims, (gpointer) b->account_name);
		e_contact_set (c, field, (gpointer) ims);
		dirty = TRUE;
	}

        /* Set the photo if it's not set */
	if (b->icon != NULL) {
		photo = e_contact_get (c, E_CONTACT_PHOTO);
		if (photo == NULL) {
			gchar *contents = NULL;

			photo = g_new0 (EContactPhoto, 1);
			photo->type = E_CONTACT_PHOTO_TYPE_INLINED;

			if (! g_file_get_contents (b->icon, &contents, &photo->data.inlined.length, &error)) {
				g_warning ("bbdb: Could not read buddy icon: %s\n", error->message);
				g_error_free (error);
				for (l = ims; l != NULL; l = l->next)
					g_free ((gchar *) l->data);
				g_list_free (ims);
				return dirty;
			}

			photo->data.inlined.data = (guchar *)contents;
			e_contact_set (c, E_CONTACT_PHOTO, (gpointer) photo);
			dirty = TRUE;
		}
	}

	/* Clean up */
	if (photo != NULL)
		e_contact_photo_free (photo);

	for (l = ims; l != NULL; l = l->next)
		g_free ((gchar *) l->data);
	g_list_free (ims);

	return dirty;
}

static EContactField
proto_to_contact_field (const gchar *proto)
{
	if (! strcmp (proto,  "prpl-oscar"))
		return E_CONTACT_IM_AIM;
	if (! strcmp (proto, "prpl-novell"))
		return E_CONTACT_IM_GROUPWISE;
	if (! strcmp (proto, "prpl-msn"))
		return E_CONTACT_IM_MSN;
	if (! strcmp (proto, "prpl-icq"))
		return E_CONTACT_IM_ICQ;
	if (! strcmp (proto, "prpl-yahoo"))
		return E_CONTACT_IM_YAHOO;
	if (! strcmp (proto, "prpl-jabber"))
		return E_CONTACT_IM_JABBER;
	if (! strcmp (proto, "prpl-gg"))
		return E_CONTACT_IM_GADUGADU;

	return E_CONTACT_IM_AIM;
}

static void
get_all_blocked (xmlNodePtr node, GSList **blocked)
{
	xmlNodePtr child;

	if (!node || !blocked)
		return;

	for (child = node->children; child; child = child->next) {
		if (child->children)
			get_all_blocked (child, blocked);

		if (!strcmp ((const gchar *)child->name, "block")) {
			gchar *name = get_node_text (child);

			if (name)
				*blocked = g_slist_prepend (*blocked, name);
		}
	}
}

static GList *
bbdb_get_gaim_buddy_list (void)
{
	gchar *blist_path;
	xmlDocPtr buddy_xml;
	xmlNodePtr root, child, blist;
	GList *buddies = NULL;
	GSList *blocked = NULL;

	blist_path = g_build_path ("/", getenv ("HOME"), ".purple/blist.xml", NULL);

	buddy_xml = xmlParseFile (blist_path);
	g_free (blist_path);
	if (! buddy_xml) {
		fprintf (stderr, "bbdb: Could not open Pidgin buddy list.\n");
		return NULL;
	}

	root = xmlDocGetRootElement (buddy_xml);
	if (strcmp ((const gchar *)root->name, "purple")) {
		fprintf (stderr, "bbdb: Could not parse Pidgin buddy list.\n");
		xmlFreeDoc (buddy_xml);
		return NULL;
	}

	for (child = root->children; child != NULL; child = child->next) {
		if (! strcmp ((const gchar *)child->name, "privacy")) {
			get_all_blocked (child, &blocked);
			break;
		}
	}

	blist = NULL;
	for (child = root->children; child != NULL; child = child->next) {
		if (! strcmp ((const gchar *)child->name, "blist")) {
			blist = child;
			break;
		}
	}
	if (blist == NULL) {
		fprintf (stderr, "bbdb: Could not find 'blist' element in Pidgin buddy list.\n");
		xmlFreeDoc (buddy_xml);
		return NULL;
	}

	for (child = blist->children; child != NULL; child = child->next) {
		if (! strcmp ((const gchar *)child->name, "group"))
			parse_buddy_group (child, &buddies, blocked);
	}

	xmlFreeDoc (buddy_xml);

	g_slist_foreach (blocked, (GFunc)g_free, NULL);
	g_slist_free (blocked);

	return buddies;
}

static void
free_gaim_body (GaimBuddy *gb)
{
	if (!gb)
		return;

	g_free (gb->icon);
	g_free (gb->alias);
	g_free (gb->account_name);
	g_free (gb->proto);
	g_free (gb);
}

static void
free_buddy_list (GList *blist)
{
	g_list_foreach (blist, (GFunc)free_gaim_body, NULL);
	g_list_free (blist);
}

static gchar *
get_node_text (xmlNodePtr node)
{
	if (node->children == NULL || node->children->content == NULL ||
	    strcmp ((gchar *)node->children->name, "text"))
		return NULL;

	return g_strdup ((gchar *)node->children->content);
}

static gchar *
get_buddy_icon_from_setting (xmlNodePtr setting)
{
	gchar *icon = NULL;

	icon = get_node_text (setting);
	if (icon [0] != '/') {
		gchar *path;

		path = g_build_path ("/", getenv ("HOME"), ".purple/icons", icon, NULL);
		g_free (icon);
		icon = path;
	}

	return icon;
}

static void
parse_contact (xmlNodePtr contact, GList **buddies, GSList *blocked)
{
	xmlNodePtr  child;
	xmlNodePtr  buddy = NULL;
	GaimBuddy  *gb;
	gboolean    is_blocked = FALSE;

	for (child = contact->children; child != NULL; child = child->next) {
		if (! strcmp ((const gchar *)child->name, "buddy")) {
			buddy = child;
			break;
		}
	}

	if (buddy == NULL) {
		fprintf (stderr, "bbdb: Could not find buddy in contact. Malformed Pidgin buddy list file.\n");
		return;
	}

	gb = g_new0 (GaimBuddy, 1);

	gb->proto = e_xml_get_string_prop_by_name (buddy, (const guchar *)"proto");

	for (child = buddy->children; child != NULL && !is_blocked; child = child->next) {
		if (! strcmp ((const gchar *)child->name, "setting")) {
			gchar *setting_type;
			setting_type = e_xml_get_string_prop_by_name (child, (const guchar *)"name");

			if (! strcmp ((const gchar *)setting_type, "buddy_icon"))
				gb->icon = get_buddy_icon_from_setting (child);

			g_free (setting_type);
		} else if (! strcmp ((const gchar *)child->name, "name")) {
			gb->account_name = get_node_text (child);
			is_blocked = g_slist_find_custom (blocked, gb->account_name, (GCompareFunc)strcmp) != NULL;
		} else if (! strcmp ((const gchar *)child->name, "alias"))
			gb->alias = get_node_text (child);

	}

	if (is_blocked)
		free_gaim_body (gb);
	else
		*buddies = g_list_prepend (*buddies, gb);
}

static void
parse_buddy_group (xmlNodePtr group, GList **buddies, GSList *blocked)
{
	xmlNodePtr child;

	for (child = group->children; child != NULL; child = child->next) {
		if (strcmp ((const gchar *)child->name, "contact"))
			continue;

		parse_contact (child, buddies, blocked);
	}
}
