/*
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
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __JUNK_SETTINGS_H__
#define __JUNK_SETTINGS_H__

G_BEGIN_DECLS

#include <gtk/gtk.h>
#include <camel/camel-store.h>
#include <e-gw-connection.h>

#define _JUNK_SETTINGS_TYPE	      (junk_settings_get_type ())
#define JUNK_SETTINGS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), JUNK_SETTINGS, JunkSettings))
#define JUNK_SETTINGS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), JUNK_SETTINGS_TYPE, JunkSettings))
#define IS_JUNK_SETTINGS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), JUNK_SETTINGS_TYPE))
#define IS_JUNK_SETTINGS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), JUNK_SETTINGS_TYPE))

typedef struct _JunkSettings JunkSettings;
typedef struct _JunkSettingsClass JunkSettingsClass;

struct _JunkSettings {
	GtkVBox parent_object;

	GladeXML *xml;

	/* General tab */

	/* Default Behavior */
	GtkTreeView *entry_list;
	GtkButton *add_button;
	GtkButton *remove;
	GtkEntry *entry;
	GtkRadioButton *enable;
	GtkRadioButton *disable;
	GtkWidget *scrolled_window;
	GtkListStore *model;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkVBox  *vbox;
	GtkVBox  *table;
	GtkWidget *window;

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
JunkSettings * junk_settings_new (EGwConnection *ccnc);
void commit_changes (JunkSettings *js);

G_END_DECLS

#endif /* __JUNK_SETTINGS_H__ */
