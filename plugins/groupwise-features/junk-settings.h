/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2005 Novell, Inc. (www.novell.com)
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

#ifndef __JUNK_SETTINGS_H__
#define __JUNK_SETTINGS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtk.h>
#include <camel/camel-store.h>
#include <e-gw-connection.h>

#define _JUNK_SETTINGS_TYPE    	      (junk_settings_get_type ())
#define JUNK_SETTINGS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), JUNK_SETTINGS, JunkSettings))
#define JUNK_SETTINGS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), JUNK_SETTINGS_TYPE, JunkSettings))
#define IS_JUNK_SETTINGS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), JUNK_SETTINGS_TYPE))
#define IS_JUNK_SETTINGS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), JUNK_SETTINGS_TYPE))

typedef struct _JunkSettings JunkSettings;
typedef struct _JunkSettingsClass JunkSettingsClass;

struct _GtkWidget;
struct _GladeXML;
struct _GtkButton;
struct _GtkTreeView;
struct _GtkLabel;
struct _GtkEntry;
struct _GtkWindow;
struct _GtkRadioButton;
struct _GtkListStore;
struct _GtkCellRenderer;
struct _GtkTreeViewColumn;
struct _GtkFrame;
struct _GtkVBox;

struct _JunkSettings {
	GtkVBox parent_object;
	
	struct _GladeXML *xml;
	
	/* General tab */
	
	/* Default Behavior */
	struct _GtkTreeView *entry_list;
	struct _GtkButton *add_button;
	struct _GtkButton *remove;
	struct _GtkEntry *entry;
	struct _GtkRadioButton *enable;
	struct _GtkRadioButton *disable;
	struct _GtkWidget *scrolled_window;
	struct _GtkListStore *model;
	struct _GtkCellRenderer *cell;
	struct _GtkTreeViewColumn *column;
	struct _GtkVBox  *vbox;
	struct _GtkVBox  *table;
	struct _GtkWidget *window;
	
	GList *junk_list;
	gint users;
	gint flag_for_ok;
	gboolean enabled;
	EGwConnection *cnc;
	GtkTreeIter iter;
};

struct _JunkSettingsClass {
	GtkVBoxClass parent_class;
	
};

GType junk_settings_get_type (void);
struct _JunkSettings * junk_settings_new (EGwConnection *ccnc);
void commit_changes (JunkSettings *js);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __JUNK_SETTINGS_H__ */
