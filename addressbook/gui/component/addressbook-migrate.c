/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2004, Novell, Inc.
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
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#include <glib.h>
#include <string.h>
#include "addressbook-migrate.h"
#include <libebook/e-book-async.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>

static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
setup_progress_dialog (void)
{
	GtkWidget *vbox, *hbox, *w;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *) window, _("Migrating..."));
	gtk_window_set_modal ((GtkWindow *) window, TRUE);
	gtk_container_set_border_width ((GtkContainer *) window, 6);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add ((GtkContainer *) window, vbox);
	
	w = gtk_label_new (_("The location and hierarchy of the Evolution contact "
			     "folders has changed since Evolution 1.x.\n\nPlease be "
			     "patient while Evolution migrates your folders..."));
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_widget_show (w);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, w);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, hbox);
	
	label = (GtkLabel *) gtk_label_new ("");
	gtk_widget_show ((GtkWidget *) label);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) label);
	
	progress = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_widget_show ((GtkWidget *) progress);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) progress);
	
	gtk_widget_show (window);
}

static void
dialog_close (void)
{
	gtk_widget_destroy ((GtkWidget *) window);
}

static void
dialog_set_folder_name (const char *folder_name)
{
	char *text;
	
	text = g_strdup_printf (_("Migrating `%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);
	
	gtk_progress_bar_set_fraction (progress, 0.0);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
dialog_set_progress (double percent)
{
	char text[5];
	
	snprintf (text, sizeof (text), "%d%%", (int) (percent * 100.0f));
	
	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static GList*
find_addressbook_dirs (char *path, gboolean toplevel)
{
	char *db_path;
	char *subfolder_path;
	GList *list = NULL;

	printf ("find_addressbook_dirs (%s)\n", path);

	/* check if the present path is a db */
	db_path = g_build_filename (path, "addressbook.db", NULL);
	if (g_file_test (db_path, G_FILE_TEST_EXISTS))
		list = g_list_append (list, g_strdup (path));
	g_free (db_path);

	/* recurse to subfolders */
	if (toplevel)
		subfolder_path = g_strdup (path);
	else
		subfolder_path = g_build_filename (path, "subfolders", NULL);
	if (g_file_test (subfolder_path, G_FILE_TEST_IS_DIR)) {
		GDir *dir = g_dir_open (subfolder_path, 0, NULL);

		if (dir) {
			const char *name;
			while ((name = g_dir_read_name (dir))) {
				if (strcmp (name, ".") && strcmp (name, "..")) {
					char *p;
					GList *sublist;

					if (toplevel)
						p = g_build_filename (path, name, NULL);
					else
						p = g_build_filename (path, "subfolders", name, NULL);

					sublist = find_addressbook_dirs (p, FALSE);
					g_free (p);
					list = g_list_concat (list, sublist);
				}
			}
			g_dir_close (dir);
		}
	}

	g_free (subfolder_path);

	return list;
}

static gboolean
check_for_conflict (ESourceGroup *group, char *name)
{
	GSList *sources;
	GSList *s;

	sources = e_source_group_peek_sources (group);

	for (s = sources; s; s = s->next) {
		ESource *source = E_SOURCE (s->data);

		if (!strcmp (e_source_peek_name (source), name))
			return TRUE;
	}

	return FALSE;
}

static char *
get_source_name (ESourceGroup *group, const char *path)
{
	char **p = g_strsplit (path, "/", 0);
	int i, j, starting_index;
	int num_elements;
	gboolean conflict;
	GString *s = g_string_new ("");

	for (i = 0; p[i]; i ++) ;

	num_elements = i;
	i--;

	/* p[i] is now the last path element */

	/* check if it conflicts */
	starting_index = i;
	do {
		g_string_assign (s, "");
		for (j = starting_index; j < num_elements; j += 2) {
			if (j != starting_index)
				g_string_append_c (s, '_');
			g_string_append (s, p[j]);
		}

		conflict = check_for_conflict (group, s->str);


		/* if there was a conflict back up 2 levels (skipping the /subfolder/ element) */
		if (conflict)
			starting_index -= 2;

		/* we always break out if we can't go any further,
		   regardless of whether or not we conflict. */
		if (starting_index < 0)
			break;

	} while (conflict);

	return g_string_free (s, FALSE);
}

static void
migrate_contacts (EBook *old_book, EBook *new_book)
{
	EBookQuery *query = e_book_query_any_field_contains ("");
	GList *l, *contacts;
	int num_added = 0;
	int num_contacts;

	/* both books are loaded, start the actual migration */
	e_book_get_contacts (old_book, query, &contacts, NULL);

	num_contacts = g_list_length (contacts);
	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;
		GError *e = NULL;
		GList *attrs, *attr;

		/* do some last minute massaging of the contact's attributes */

		attrs = e_vcard_get_attributes (E_VCARD (contact));
		for (attr = attrs; attr;) {
			EVCardAttribute *a = attr->data;

			/* evo 1.4 used the non-standard X-EVOLUTION-OFFICE attribute,
			   evo 1.5 uses the third element in the ORG list attribute. */
			if (!strcmp ("X-EVOLUTION-OFFICE", e_vcard_attribute_get_name (a))) {
				GList *v = e_vcard_attribute_get_values (a);
				GList *next_attr;

				if (v && v->data)
					e_contact_set (contact, E_CONTACT_OFFICE, v->data);

				next_attr = attr->next;
				e_vcard_remove_attribute (E_VCARD (contact), a);
				attr = next_attr;
			}
			/* evo 1.4 didn't put TYPE=VOICE in for phone numbers.
			   evo 1.5 does.

			   so we search through the attribute params for
			   either TYPE=VOICE or TYPE=FAX.  If we find
			   either we do nothing.  If we find neither, we
			   add TYPE=VOICE.
			*/
			else if (!strcmp ("TEL", e_vcard_attribute_get_name (a))) {
				GList *params, *param;
				gboolean found = FALSE;

				params = e_vcard_attribute_get_params (a);
				for (param = params; param; param = param->next) {
					EVCardAttributeParam *p = param->data;
					if (!strcmp (EVC_TYPE, e_vcard_attribute_param_get_name (p))) {
						GList *v = e_vcard_attribute_param_get_values (p);
						if (v && v->data)
							if (!strcmp ("VOICE", v->data)
							    || !strcmp ("FAX", v->data))
								found = TRUE;
					}
				}

				if (!found)
					e_vcard_attribute_add_param_with_value (a,
										e_vcard_attribute_param_new (EVC_TYPE),
										"VOICE");
				attr = attr->next;
			}
			else {
				attr = attr->next;
			}
		}

		if (!e_book_add_contact (new_book,
					 contact,
					 &e))
			g_warning ("contact add failed: `%s'", e->message);

		num_added ++;

		dialog_set_progress ((double)num_added / num_contacts);
	}
}

static void
migrate_contact_folder (char *old_path, ESourceGroup *dest_group, char *source_name)
{
	char *old_uri = g_strdup_printf ("file://%s", old_path);
	GError *e = NULL;

	EBook *old_book = NULL, *new_book = NULL;
	ESource *old_source;
	ESource *new_source;
	ESourceGroup *group;

	group = e_source_group_new ("", old_uri);
	old_source = e_source_new ("", "");
	e_source_set_group (old_source, group);
	g_object_unref (group);

	new_source = e_source_new (source_name, source_name);
	e_source_set_group (new_source, dest_group);

	dialog_set_folder_name (source_name);

	old_book = e_book_new ();
	if (!e_book_load_source (old_book, old_source, TRUE, &e)) {
		g_warning ("failed to load source book for migration: `%s'", e->message);
		goto finish;
	}

	new_book = e_book_new ();
	if (!e_book_load_source (new_book, new_source, FALSE, &e)) {
		g_warning ("failed to load destination book for migration: `%s'", e->message);
		goto finish;
	}

	migrate_contacts (old_book, new_book);

 finish:
	g_object_unref (old_book);
	g_object_unref (new_book);
	g_free (old_uri);
}

#define LDAP_BASE_URI "ldap://"
#define PERSONAL_RELATIVE_URI "Personal"

static void
create_groups (AddressbookComponent *component,
	       ESourceList   *source_list,
	       ESourceGroup **on_this_computer,
	       ESourceGroup **on_ldap_servers)
{
	GSList *groups;
	ESourceGroup *group;
	ESource *personal_source = NULL;
	char *base_uri, *base_uri_proto, *new_dir;

	*on_this_computer = NULL;
	*on_ldap_servers = NULL;

	base_uri = g_build_filename (addressbook_component_peek_base_directory (component),
				     "/addressbook/local/OnThisComputer/",
				     NULL);

	base_uri_proto = g_strconcat ("file://", base_uri, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!strcmp (base_uri_proto, e_source_group_peek_base_uri (group)))
				*on_this_computer = group;
			else if (!strcmp (LDAP_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_ldap_servers = group;
		}
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (*on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			if (!strcmp (PERSONAL_RELATIVE_URI, e_source_peek_relative_uri (source))) {
				personal_source = source;
				break;
			}
		}
	}
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!personal_source) {
		/* Create the default Person addressbook */
		new_dir = g_build_filename (base_uri, "Personal/", NULL);
		if (!e_mkdir_hier (new_dir, 0700)) {
			personal_source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
			e_source_group_add_source (*on_this_computer, personal_source, -1);
		}
		g_free (new_dir);
	}

	if (!*on_ldap_servers) {
		/* Create the LDAP source group */
		group = e_source_group_new (_("On LDAP Servers"), LDAP_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_ldap_servers = group;
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

static gboolean
migrate_local_folders (AddressbookComponent *component, ESourceGroup *on_this_computer)
{
	char *old_path = NULL;
	char *local_contact_folder = NULL;
	char *source_name = NULL;
	GList *dirs, *l;

	old_path = g_strdup_printf ("%s/evolution/local", g_get_home_dir ());

	dirs = find_addressbook_dirs (old_path, TRUE);

	/* migrate the local addressbook first, to OnThisComputer/Personal */
	local_contact_folder = g_build_filename (g_get_home_dir (), "/evolution/local/Contacts",
						 NULL);
	source_name = "Personal";
	migrate_contact_folder (local_contact_folder, on_this_computer, source_name);

	for (l = dirs; l; l = l->next) {
		ESource *source;

		/* skip the local contact folder, since we handle that
		   specifically, mapping it to Personal */
		if (!strcmp ((char*)l->data, local_contact_folder))
			continue;

		source_name = get_source_name (on_this_computer, (char*)l->data + strlen (old_path) + 1);

		source = e_source_new (source_name, source_name);
		e_source_group_add_source (on_this_computer, source, -1);

		migrate_contact_folder (l->data, on_this_computer, source_name);
		g_free (source_name);
	}

	g_list_foreach (dirs, (GFunc)g_free, NULL);
	g_list_free (dirs);
	g_free (local_contact_folder);
	g_free (old_path);

	return TRUE;
}

static char *
get_string_child (xmlNode *node,
		  const char *name)
{
	xmlNode *p;
	xmlChar *xml_string;
	char *retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return NULL;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the empty string */
		return g_strdup("");

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = g_strdup ((char *) xml_string);
	xmlFree (xml_string);

	return retval;
}

static int
get_integer_child (xmlNode *node,
		   const char *name,
		   int defval)
{
	xmlNode *p;
	xmlChar *xml_string;
	int retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return defval;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the default */
		return defval;

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = atoi (xml_string);
	xmlFree (xml_string);

	return retval;
}

static gboolean
migrate_ldap_servers (AddressbookComponent *component, ESourceGroup *on_ldap_servers)
{
	char *sources_xml = g_strdup_printf ("%s/evolution/addressbook-sources.xml",
					     g_get_home_dir ());

	printf ("trying to migrate from %s\n", sources_xml);

	if (g_file_test (sources_xml, G_FILE_TEST_EXISTS)) {
		xmlDoc  *doc = xmlParseFile (sources_xml);
		xmlNode *root;
		xmlNode *child;
		int num_contactservers;
		int servernum;

		if (!doc)
			return FALSE;

		root = xmlDocGetRootElement (doc);
		if (root == NULL || strcmp (root->name, "addressbooks") != 0) {
			xmlFreeDoc (doc);
			return FALSE;
		}

		/* count the number of servers, so we can give progress */
		num_contactservers = 0;
		for (child = root->children; child; child = child->next) {
			if (!strcmp (child->name, "contactserver")) {
				num_contactservers++;
			}
		}
		printf ("found %d contact servers to migrate\n", num_contactservers);

		dialog_set_folder_name (_("LDAP Servers"));

		servernum = 0;
		for (child = root->children; child; child = child->next) {
			if (!strcmp (child->name, "contactserver")) {
				char *port, *host, *rootdn, *scope, *authmethod, *ssl;
				char *emailaddr, *binddn, *limitstr;
				int limit;
				char *name, *description;
				GString *uri = g_string_new ("");
				ESource *source;

				name        = get_string_child (child, "name");
				description = get_string_child (child, "description");
				port        = get_string_child (child, "port");
				host        = get_string_child (child, "host");
				rootdn      = get_string_child (child, "rootdn");
			        scope       = get_string_child (child, "scope");
				authmethod  = get_string_child (child, "authmethod");
				ssl         = get_string_child (child, "ssl");
				emailaddr   = get_string_child (child, "emailaddr");
				binddn      = get_string_child (child, "binddn");
				limit       = get_integer_child (child, "limit", 100);
				limitstr    = g_strdup_printf ("%d", limit);

				g_string_append_printf (uri,
							"%s:%s/%s?"/*trigraph prevention*/"?%s",
							host, port, rootdn, scope);

				source = e_source_new (name, uri->str);
				e_source_set_property (source, "description", description);
				e_source_set_property (source, "limit", limitstr);
				e_source_set_property (source, "ssl", ssl);
				e_source_set_property (source, "auth", authmethod);
				if (emailaddr)
					e_source_set_property (source, "email_addr", emailaddr);
				if (binddn)
					e_source_set_property (source, "binddn", binddn);

				e_source_group_add_source (on_ldap_servers, source, -1);

				g_string_free (uri, TRUE);
				g_free (port);
				g_free (host);
				g_free (rootdn);
				g_free (scope);
				g_free (authmethod);
				g_free (ssl);
				g_free (emailaddr);
				g_free (binddn);
				g_free (limitstr);
				g_free (name);
				g_free (description);

				servernum++;
				dialog_set_progress ((double)servernum/num_contactservers);
			}
		}

		xmlFreeDoc (doc);
	}

	g_free (sources_xml);

	return TRUE;
}

static ESource*
get_source_by_uri (ESourceList *source_list, const char *uri)
{
	GSList *groups;
	GSList *g;

	groups = e_source_list_peek_groups (source_list);
	if (!groups)
		return NULL;

	for (g = groups; g; g = g->next) {
		GSList *sources;
		GSList *s;
		ESourceGroup *group = E_SOURCE_GROUP (g->data);

		sources = e_source_group_peek_sources (group);
		if (!sources)
			continue;

		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			char *source_uri = e_source_get_uri (source);
			gboolean found = FALSE;

			if (!strcmp (uri, source_uri))
				found = TRUE;

			g_free (source_uri);
			if (found)
				return source;
		}
	}

	return NULL;
}

static gboolean
migrate_completion_folders (AddressbookComponent *component, ESourceList *source_list)
{
	char *uris_xml = gconf_client_get_string (addressbook_component_peek_gconf_client (component),
						  "/apps/evolution/addressbook/completion/uris",
						  NULL);

	printf ("trying to migrate completion folders\n");

	if (uris_xml) {
		xmlDoc  *doc = xmlParseMemory (uris_xml, strlen (uris_xml));
		xmlNode *root;
		xmlNode *child;

		if (!doc)
			return FALSE;

		dialog_set_folder_name (_("Autocompletion Settings"));

		root = xmlDocGetRootElement (doc);
		if (root == NULL || strcmp (root->name, "EvolutionFolderList") != 0) {
			xmlFreeDoc (doc);
			return FALSE;
		}

		for (child = root->children; child; child = child->next) {
			if (!strcmp (child->name, "folder")) {
				char *physical_uri = e_xml_get_string_prop_by_name (child, "physical-uri");
				char *uri;
				ESource *source;

				/* if the physical uri is
				   file://... we need to convert the
				   path to the new directory
				   structure.

				   if the physical_uri is anything
				   else, we strip off the args
				   (anything after ;) before searching
				   for the uri. */

				if (!strncmp (physical_uri, "file://", 7)) {
					char *local_path = g_build_filename (g_get_home_dir (),
									     "/evolution/local/",
									     NULL);

					if (!strncmp (physical_uri + 7, local_path, strlen (local_path))) {
						char *path_extra;
						char *path;

						if (!strcmp (physical_uri + 7 + strlen (local_path), "Contacts"))
							/* special case the ~/evolution/local/Contacts folder */
							path_extra = "Personal";
						else
							path_extra = physical_uri + 7 + strlen (local_path);

						path = g_build_filename (g_get_home_dir (),
								 "/.evolution/addressbook/local/OnThisComputer",
								 path_extra,
								 NULL);
						uri = g_strdup_printf ("file://%s", path);
						g_free (path);
					}
					else {
						/* if they somehow created a folder that lies
						   outside the evolution folder tree, just pass
						   the uri straight on */
						uri = g_strdup (physical_uri);
					}

					g_free (local_path);
				}
				else {
					char *semi = strchr (physical_uri, ';');
					if (semi)
						uri = g_strndup (physical_uri, semi - physical_uri);
					else
						uri = g_strdup (physical_uri);
				}

				source = get_source_by_uri (source_list, uri);
				if (source) {
					e_source_set_property (source, "completion", "true");
				}
				else {
					g_warning ("found completion folder with uri `%s' that "
						   "doesn't correspond to anything we migrated.", physical_uri);
				}

				g_free (physical_uri);
				g_free (uri);
			}
		}

		g_free (uris_xml);
	}
	else {
		g_message ("no completion folder settings to migrate");
	}

	return TRUE;
}

int
addressbook_migrate (AddressbookComponent *component, int major, int minor, int revision)
{
	ESourceList *source_list = addressbook_component_peek_source_list (component);
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;

	printf ("addressbook_migrate (%d.%d.%d)\n", major, minor, revision);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_groups (component, source_list, &on_this_computer, &on_ldap_servers);

	if (major <= 1) {
		
		if (/* we're <= 1.5.2 */
		    (major == 1
		     && ((minor == 5 && revision <= 2) 
			 || (minor < 5)))
		    ||
		    /* we're 0.x */
		    (major == 0)) {

			setup_progress_dialog ();
			if (on_this_computer)
				migrate_local_folders (component, on_this_computer);
			if (on_ldap_servers)
				migrate_ldap_servers (component, on_ldap_servers);

			migrate_completion_folders (component, source_list);

			dialog_close ();
		}
	}

	e_source_list_sync (source_list, NULL);

	return TRUE;
}
