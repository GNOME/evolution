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
	
	new = MAIL_FONT_PREFS (gtk_type_new (mail_font_prefs_get_type ()));
	
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
mail_font_prefs_finalize (GtkObject *object)
{
	MailFontPrefs *prefs = (MailFontPrefs *) object;

	gtk_object_unref (GTK_OBJECT (prefs->pman));
	gtk_object_unref (GTK_OBJECT (prefs->gui));

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
mail_font_prefs_init (MailFontPrefs *prefs)
{
	GtkWidget *toplevel;
	GladeXML *gui;

	prefs->gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "font_tab");


	prefs->pman = GTK_HTML_PROPMANAGER (gtk_html_propmanager_new (NULL));
	gtk_html_propmanager_set_gui (prefs->pman, gui, NULL);
	gtk_object_ref (GTK_OBJECT (prefs->pman));
	gtk_object_sink (GTK_OBJECT (prefs->pman));

	gtk_signal_connect (GTK_OBJECT (prefs->pman), "changed", font_prefs_changed, prefs);

	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);	
}

static void
mail_font_prefs_class_init (MailFontPrefsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());

	object_class->finalize = mail_font_prefs_finalize;
}

GtkType
mail_font_prefs_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailFontPrefs",
			sizeof (MailFontPrefs),
			sizeof (MailFontPrefsClass),
			(GtkClassInitFunc) mail_font_prefs_class_init,
			(GtkObjectInitFunc) mail_font_prefs_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info); 
	}

	return type;
}


