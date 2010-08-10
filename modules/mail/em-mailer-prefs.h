/*
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_MAILER_PREFS_H
#define EM_MAILER_PREFS_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <shell/e-shell.h>
#include <widgets/misc/e-preferences-window.h>

/* Standard GObject macros */
#define EM_TYPE_MAILER_PREFS \
	(em_mailer_prefs_get_type ())
#define EM_MAILER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_MAILER_PREFS, EMMailerPrefs))
#define EM_MAILER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_MAILER_PREFS, EMMailerPrefsClass))
#define EM_IS_MAILER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_MAILER_PREFS))
#define EM_IS_MAILER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_MAILER_PREFS))
#define EM_MAILER_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_MAILER_PREFS))

G_BEGIN_DECLS

typedef struct _EMMailerPrefs EMMailerPrefs;
typedef struct _EMMailerPrefsClass EMMailerPrefsClass;

struct _EMMailerPrefs {
	GtkVBox parent_object;

	GtkBuilder *builder;
	GConfClient *gconf;

	/* General tab */

	/* Message Display */
	GtkSpinButton *timeout;

	/* HTML Mail tab */
	GtkFontButton *font_variable;
	GtkFontButton *font_fixed;
	GtkToggleButton *font_share;

	/* Loading Images */
	GtkToggleButton *images_always;
	GtkToggleButton *images_sometimes;
	GtkToggleButton *images_never;

	GtkToggleButton *autodetect_links;

	/* Labels and Colours tab */
	GtkWidget *label_add;
	GtkWidget *label_edit;
	GtkWidget *label_remove;
	GtkWidget *label_tree;
	GtkListStore *label_list_store;
	guint labels_change_notify_id; /* mail_config's notify id */

	/* Headers tab */
	GtkButton *add_header;
	GtkButton *remove_header;
	GtkEntry *entry_header;
	GtkTreeView *header_list;
	GtkListStore *header_list_store;

	GtkToggleButton *sa_local_tests_only;
	GtkToggleButton *sa_use_daemon;
	GtkComboBox *default_junk_plugin;
	GtkLabel *plugin_status;
	GtkImage *plugin_image;

	GtkToggleButton *junk_header_check;
	GtkTreeView *junk_header_tree;
	GtkListStore *junk_header_list_store;
	GtkButton *junk_header_add;
	GtkButton *junk_header_remove;
	GtkToggleButton *junk_book_lookup;
	GtkToggleButton *junk_lookup_local_only;
};

struct _EMMailerPrefsClass {
	GtkVBoxClass parent_class;
};

GType      em_mailer_prefs_get_type (void);
GtkWidget *create_combo_text_widget (void);
GtkWidget *em_mailer_prefs_new      (EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_MAILER_PREFS_H */
