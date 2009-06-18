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

#ifndef __EM_MAILER_PREFS_H__
#define __EM_MAILER_PREFS_H__

#include <gtk/gtk.h>
#include <shell/Evolution.h>
#include <gconf/gconf-client.h>
#include <e-util/e-signature.h>

#define EM_MAILER_PREFS_TYPE        (em_mailer_prefs_get_type ())
#define EM_MAILER_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_MAILER_PREFS_TYPE, EMMailerPrefs))
#define EM_MAILER_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_MAILER_PREFS_TYPE, EMMailerPrefsClass))
#define EM_IS_MAILER_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_MAILER_PREFS_TYPE))
#define EM_IS_MAILER_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_MAILER_PREFS_TYPE))

G_BEGIN_DECLS

typedef struct _EMMailerPrefs EMMailerPrefs;
typedef struct _EMMailerPrefsClass EMMailerPrefsClass;
typedef struct _EMMailerPrefsHeader EMMailerPrefsHeader;

struct _EMMailerPrefsHeader {
	gchar *name;
	guint enabled:1;
	guint is_default:1;
};

struct _EMMailerPrefs {
	GtkVBox parent_object;

	GNOME_Evolution_Shell shell;

	struct _GladeXML *gui;
	struct _GConfClient *gconf;

	/* General tab */

	/* Message Display */
	GtkToggleButton *timeout_toggle;
	GtkSpinButton *timeout;
	GtkToggleButton *address_toggle;
	GtkSpinButton *address_count;
	GtkToggleButton *mlimit_toggle;
	GtkSpinButton *mlimit_count;
	GtkOptionMenu *charset;
	GtkToggleButton *citation_highlight;
	GtkColorButton *citation_color;
	GtkToggleButton *enable_search_folders;
	GtkToggleButton *magic_spacebar;

	/* Deleting Mail */
	GtkToggleButton *empty_trash;
	GtkComboBox *empty_trash_days;
	GtkToggleButton *confirm_expunge;

	/* HTML Mail tab */
	GtkFontButton *font_variable;
	GtkFontButton *font_fixed;
	GtkToggleButton *font_share;

	/* Loading Images */
	GtkToggleButton *images_always;
	GtkToggleButton *images_sometimes;
	GtkToggleButton *images_never;

	GtkToggleButton *show_animated;
	GtkToggleButton *autodetect_links;
	GtkToggleButton *prompt_unwanted_html;

	/* Labels and Colours tab */
	GtkWidget *label_add;
	GtkWidget *label_edit;
	GtkWidget *label_remove;
	GtkWidget *label_tree;
	guint labels_change_notify_id; /* mail_config's notify id */

	/* Headers tab */
	GtkButton *add_header;
	GtkButton *remove_header;
	GtkEntry *entry_header;
	GtkTreeView *header_list;
	GtkListStore *header_list_store;
	GtkToggleButton *photo_show;
	GtkToggleButton *photo_local;

	/* Junk prefs */
	GtkToggleButton *check_incoming;
	GtkToggleButton *empty_junk;
	GtkComboBox *empty_junk_days;

	GtkToggleButton *sa_local_tests_only;
	GtkToggleButton *sa_use_daemon;
	GtkComboBox *default_junk_plugin;
	GtkLabel *plugin_status;
	GtkImage *plugin_image;

	GtkToggleButton *junk_header_check;
	GtkTreeView *junk_header_tree;
	GtkButton *junk_header_add;
	GtkButton *junk_header_remove;
	GtkToggleButton *junk_book_lookup;
	GtkToggleButton *junk_lookup_local_only;
};

struct _EMMailerPrefsClass {
	GtkVBoxClass parent_class;

	/* signals */

};

GType em_mailer_prefs_get_type (void);
GtkWidget * create_combo_text_widget (void);

GtkWidget *em_mailer_prefs_new (void);

EMMailerPrefsHeader *em_mailer_prefs_header_from_xml(const gchar *xml);
gchar *em_mailer_prefs_header_to_xml(EMMailerPrefsHeader *header);
void em_mailer_prefs_header_free(EMMailerPrefsHeader *header);

/* needed by global config */
#define EM_MAILER_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_MailerPrefs_ConfigControl:" BASE_VERSION

G_END_DECLS

#endif /* __EM_MAILER_PREFS_H__ */
