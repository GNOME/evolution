/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Gary Ekker <gekker@novell.com>
 *
 * Copyright 2004, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

/*
 * UrlEditorDialog - a GtkObject which handles a libglade-loaded dialog
 * to edit the calendar preference settings.
 */

#ifndef _URL_EDITOR_DIALOG_H_
#define _URL_EDITOR_DIALOG_H_

G_BEGIN_DECLS

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "cal-prefs-dialog.h"
#include "widgets/misc/e-source-selector.h"

struct _UrlDialogData {
	/* Glade XML data */
	GladeXML *xml;
	GtkWidget *url_editor;
	GtkWidget *url_dialog;

	GtkEntry *url_entry;
	GtkWidget *daily;
	GtkWidget *weekly;
	GtkWidget *user_publish;
	
	GtkWidget *calendar_list_label;
	GtkWidget *scrolled_window;
	
	GtkEntry *username_entry;
	GtkEntry *password_entry;
	GtkWidget *remember_pw;
	
	GtkWidget *cancel;
	GtkWidget *ok;
	EPublishUri *url_data;
};
typedef struct _UrlDialogData UrlDialogData;

gboolean			
url_editor_dialog_new (DialogData *dialog_data, EPublishUri *pub_uri);

G_END_DECLS

#endif /* _URL_EDITOR_DIALOG_H_ */
