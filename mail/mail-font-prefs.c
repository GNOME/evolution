/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Larry Ewing <lewing@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml-propmanager.h>

#include "mail-font-prefs.h"

static GtkVBoxClass *parent_class = NULL;

GtkWidget *
mail_font_prefs_new (void)
{
	MailFontPrefs *new;
	
	new = MAIL_FONT_PREFS (g_object_new (mail_font_prefs_get_type ()), NULL);
	
	return GTK_WIDGET (new);
}

void
mail_font_prefs_apply (MailFontPrefs *prefs)
{
	gtk_html_propmanager_apply (prefs->pman);
}

static void
font_prefs_changed (GtkHTMLPropmanager *pman, MailFontPrefs *prefs)
{
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
mail_font_prefs_destroy (GtkObject *object)
{
	MailFontPrefs *prefs = (MailFontPrefs *) object;

	if (prefs->pman) {
		g_object_unref(prefs->pman);
		g_object_unref(prefs->gui);
		prefs->pman = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
mail_font_prefs_init (MailFontPrefs *prefs)
{
	GtkWidget *toplevel;
	GladeXML *gui;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "font_tab", NULL);
	prefs->gui = gui;

	prefs->pman = GTK_HTML_PROPMANAGER (gtk_html_propmanager_new (NULL));
	gtk_html_propmanager_set_gui (prefs->pman, gui, NULL);
	g_object_ref(prefs->pman);
	gtk_object_sink (GTK_OBJECT (prefs->pman));

	g_signal_connect(prefs->pman, "changed", font_prefs_changed, prefs);

	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	g_object_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	g_object_unref (toplevel);
}

static void
mail_font_prefs_class_init (MailFontPrefsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;
	parent_class = g_type_class_ref(gtk_vbox_get_type ());

	object_class->destroy = mail_font_prefs_destroy;
}

GtkType
mail_font_prefs_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo type_info = {
			sizeof (MailFontPrefsClass),
			NULL, NULL,
			(GClassInitFunc) mail_font_prefs_class_init,
			NULL, NULL,
			sizeof (MailFontPrefs),
			0,
			(GInstanceInitFunc) mail_font_prefs_init,
		};
		
		type = g_type_register_static (gtk_vbox_get_type (), "MailFontPrefs", &type_info, 0);
	}

	return type;
}


