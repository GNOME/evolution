/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include <shell/e-shell.h>

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
	GtkBox parent_object;

	GtkBuilder *builder;
	GSettings *settings;

	/* General tab */

	/* Message Display */
	GtkSpinButton *timeout;

	/* HTML Mail tab */
	GtkFontButton *font_variable;
	GtkFontButton *font_fixed;
	GtkToggleButton *font_share;

	GtkToggleButton *autodetect_links;

	/* Labels and Colours tab */
	GtkWidget *label_add;
	GtkWidget *label_edit;
	GtkWidget *label_remove;
	GtkWidget *label_tree;
	GtkListStore *label_list_store;

	/* Headers tab */
	GtkButton *add_header;
	GtkButton *remove_header;
	GtkEntry *entry_header;
	GtkTreeView *header_list;
	GtkListStore *header_list_store;

	GtkToggleButton *junk_header_check;
	GtkTreeView *junk_header_tree;
	GtkListStore *junk_header_list_store;
	GtkButton *junk_header_add;
	GtkButton *junk_header_remove;
	GtkToggleButton *junk_book_lookup;
	GtkToggleButton *junk_lookup_local_only;
};

struct _EMMailerPrefsClass {
	GtkBoxClass parent_class;
};

GType		em_mailer_prefs_get_type	(void);
GtkWidget *	em_mailer_prefs_new		(EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_MAILER_PREFS_H */
