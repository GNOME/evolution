/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-ldap-server-dialog.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Toshok <toshok@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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


#include <gnome.h>
#include <glade/glade.h>
#include "e-ldap-server-dialog.h"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	ELDAPServer *server;
} ELDAPServerDialog;

static void
fill_in_server_info (ELDAPServerDialog *dialog)
{
	ELDAPServer *ldap_server = dialog->server;
	GtkEditable *editable;
	int position;

	/* the name */
	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "name-entry"));
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, ldap_server->name, strlen (ldap_server->name), &position);

	/* the server description */
	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "description-entry"));
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, ldap_server->description, strlen (ldap_server->description), &position);

	/* the server hostname */
	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "server-entry"));
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, ldap_server->host, strlen (ldap_server->host), &position);

	/* the server port */
	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "port-entry"));
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, ldap_server->port, strlen (ldap_server->port), &position);

	/* the root dn */
	position = 0;
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "root-dn-entry"));
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, ldap_server->rootdn, strlen (ldap_server->rootdn), &position);
}

static void
extract_server_info (ELDAPServerDialog *dialog)
{
	ELDAPServer *ldap_server = dialog->server;
	GtkEditable *editable;
	char *description, *server, *port, *rootdn, *name;

	/* the server name */
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "name-entry"));
	name = gtk_editable_get_chars(editable, 0, -1);
	if (name && *name) {
		if (ldap_server->name)
			g_free(ldap_server->name);
		ldap_server->name = name;
	}

	/* the server description */
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "description-entry"));
	description = gtk_editable_get_chars(editable, 0, -1);
	if (description && *description) {
		if (ldap_server->description)
			g_free(ldap_server->description);
		ldap_server->description = description;
	}

	/* the server hostname */
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "server-entry"));
	server = gtk_editable_get_chars(editable, 0, -1);
	if (server && *server) {
		if (ldap_server->host)
			g_free(ldap_server->host);
		ldap_server->host = server;
	}

	/* the server port */
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "port-entry"));
	port = gtk_editable_get_chars(editable, 0, -1);
	if (port && *port) {
		if (ldap_server->port)
			g_free (ldap_server->port);
		ldap_server->port = port;
	}

	/* the root dn */
	editable = GTK_EDITABLE(glade_xml_get_widget(dialog->gui, "root-dn-entry"));
	rootdn = gtk_editable_get_chars(editable, 0, -1);
	if (rootdn && *rootdn) {
		if (ldap_server->rootdn)
			g_free (ldap_server->rootdn);
		ldap_server->rootdn = rootdn;
	}
}

void
e_ldap_server_editor_show(ELDAPServer *server)
{
	ELDAPServerDialog *dialog = g_new0 (ELDAPServerDialog, 1);

	dialog->server = server;
	dialog->gui = glade_xml_new (EVOLUTION_GLADEDIR "/ldap-server-dialog.glade", NULL);

	dialog->dialog = glade_xml_get_widget(dialog->gui, "ldap-server-dialog");

	fill_in_server_info (dialog);

	gnome_dialog_run (GNOME_DIALOG(dialog->dialog));

	extract_server_info (dialog);

	gnome_dialog_close (GNOME_DIALOG(dialog->dialog));
}
