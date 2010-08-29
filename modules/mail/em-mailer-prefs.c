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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>

#include "em-mailer-prefs.h"
#include "em-format/em-format.h"

#include <gtkhtml/gtkhtml-properties.h>
#include <libxml/tree.h>

#include <gconf/gconf-client.h>

#include "libedataserverui/e-cell-renderer-color.h"

#include "e-util/e-util.h"
#include "e-util/e-binding.h"
#include "e-util/e-datetime-format.h"
#include "e-util/e-util-private.h"
#include "widgets/misc/e-charset-combo-box.h"
#include "shell/e-shell-utils.h"

#include "e-mail-label-manager.h"
#include "e-mail-reader-utils.h"
#include "mail-config.h"
#include "em-folder-selection-button.h"
#include "em-junk.h"
#include "em-config.h"
#include "mail-session.h"

enum {
	HEADER_LIST_NAME_COLUMN, /* displayable name of the header (may be a translation) */
	HEADER_LIST_ENABLED_COLUMN, /* is the header enabled? */
	HEADER_LIST_IS_DEFAULT_COLUMN,  /* is this header a default header, eg From: */
	HEADER_LIST_HEADER_COLUMN, /* the real name of this header */
	HEADER_LIST_N_COLUMNS
};

static GType col_types[] = {
	G_TYPE_STRING,
	G_TYPE_BOOLEAN,
	G_TYPE_BOOLEAN,
	G_TYPE_STRING
};

/* temporarily copied from em-format.c */
static const gchar *default_headers[] = {
	N_("From"),
	N_("Reply-To"),
	N_("To"),
	N_("Cc"),
	N_("Bcc"),
	N_("Subject"),
	N_("Date"),
	N_("Newsgroups"),
	N_("Face"),
	"x-evolution-mailer", /* DO NOT translate */
};

#define EM_FORMAT_HEADER_XMAILER "x-evolution-mailer"

/* for empty trash on exit frequency */
static const struct {
	const gchar *label;
	gint days;
} empty_trash_frequency[] = {
	{ N_("Every time"), 0 },
	{ N_("Once per day"), 1 },
	{ N_("Once per week"), 7 },
	{ N_("Once per month"), 30 },
};

G_DEFINE_TYPE (
	EMMailerPrefs,
	em_mailer_prefs,
	GTK_TYPE_VBOX)

static void
em_mailer_prefs_finalize (GObject *object)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) object;

	g_object_unref (prefs->builder);

	if (prefs->labels_change_notify_id) {
		gconf_client_notify_remove (prefs->gconf, prefs->labels_change_notify_id);

		prefs->labels_change_notify_id = 0;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_mailer_prefs_parent_class)->finalize (object);
}

static void
em_mailer_prefs_class_init (EMMailerPrefsClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = em_mailer_prefs_finalize;
}

static void
em_mailer_prefs_init (EMMailerPrefs *preferences)
{
	preferences->gconf = mail_config_get_gconf_client ();
}

static gboolean
mark_seen_timeout_transform (const GValue *src_value,
                             GValue *dst_value,
                             gpointer user_data)
{
	gdouble v_double;

	/* Shell Settings (gint) -> Spin Button (double) */
	v_double = (gdouble) g_value_get_int (src_value);
	g_value_set_double (dst_value, v_double / 1000.0);

	return TRUE;
}

static gboolean
mark_seen_timeout_reverse_transform (const GValue *src_value,
                                     GValue *dst_value,
                                     gpointer user_data)
{
	gdouble v_double;

	/* Spin Button (double) -> Shell Settings (gint) */
	v_double = g_value_get_double (src_value);
	g_value_set_int (dst_value, v_double * 1000);

	return TRUE;
}

enum {
	JH_LIST_COLUMN_NAME,
	JH_LIST_COLUMN_VALUE
};

static void
jh_tree_refill (EMMailerPrefs *prefs)
{
	GtkListStore *store = prefs->junk_header_list_store;
	GSList *l, *cjh = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);

	gtk_list_store_clear (store);

	for (l = cjh; l; l = l->next) {
		GtkTreeIter iter;
		gchar **tokens = g_strsplit (l->data, "=", 2);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			JH_LIST_COLUMN_NAME , tokens[0] ? tokens[0] : "",
			JH_LIST_COLUMN_VALUE, tokens[1] ? tokens[1] : "" ,
			-1);
		g_strfreev (tokens);
	}

	g_slist_foreach (cjh, (GFunc) g_free, NULL);
	g_slist_free (cjh);
}

static void
jh_add_cb (GtkWidget *widget, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	GtkWidget *dialog, *l1, *l2, *entry1, *entry2, *vbox, *hbox;
	GtkWidget *content_area;
	gint response;

	dialog = gtk_dialog_new_with_buttons (
		_("Add Custom Junk Header"),
		(GtkWindow *) gtk_widget_get_toplevel (widget),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

	vbox = gtk_vbox_new (FALSE, 6);
	hbox = gtk_hbox_new (FALSE, 0);
	l1 = gtk_label_new_with_mnemonic (_("Header Name:"));
	l2 = gtk_label_new_with_mnemonic (_("Header Value Contains:"));
	entry1 = gtk_entry_new ();
	entry2 = gtk_entry_new ();
	gtk_box_pack_start ((GtkBox *) hbox, l1, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)hbox, entry1, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)vbox, hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox, l2, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)hbox, entry2, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)vbox, hbox, FALSE, FALSE, 6);

	gtk_widget_show_all (vbox);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), vbox);
	response = gtk_dialog_run ((GtkDialog *)dialog);
	if (response == GTK_RESPONSE_ACCEPT) {
		const gchar *name = gtk_entry_get_text ((GtkEntry *)entry1);
		const gchar *value = gtk_entry_get_text ((GtkEntry *)entry2);
		gchar *tok;
		GSList *list = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);

		/* FIXME: Validate the values */

		tok = g_strdup_printf ("%s=%s", name, value);
		list = g_slist_append (list, tok);
		gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, list, NULL);
		g_slist_foreach (list, (GFunc)g_free, NULL);

		g_slist_free (list);
	}
	gtk_widget_destroy (dialog);
	jh_tree_refill (prefs);
}

static void
jh_remove_cb (GtkWidget *widget, gpointer user_data)
{
	EMMailerPrefs *prefs = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (prefs != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->junk_header_tree));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *name=NULL, *value=NULL;
		GSList *prev = NULL, *node, *list = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);
		gtk_tree_model_get (model, &iter, JH_LIST_COLUMN_NAME, &name, JH_LIST_COLUMN_VALUE, &value, -1);
		node = list;
		while (node) {
			gchar *test;
			gint len = strlen (name);
			test = strncmp (node->data, name, len) == 0 ? (gchar *) node->data+len:NULL;

			if (test) {
				test++;
				if (strcmp (test, value) == 0)
					break;
			}

			prev = node;
			node = node->next;
		}

		if (prev && !node) {
			/* Not found. So what? */
		} else if (prev && node) {
			prev->next = node->next;
			g_free (node->data);
		} else if (!prev && node) {
			list = list->next;
			g_free (node->data);
		}

		gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, list, NULL);

		g_slist_foreach (list, (GFunc)g_free, NULL);
		g_slist_free (list);
		g_free (name);
		g_free (value);

		jh_tree_refill (prefs);
	}
}

static GtkListStore *
init_junk_tree (GtkWidget *label_tree, EMMailerPrefs *prefs)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;

	g_return_val_if_fail (label_tree != NULL, NULL);
	g_return_val_if_fail (prefs != NULL, NULL);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (label_tree), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Header"), renderer, "text", JH_LIST_COLUMN_NAME, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Contains Value"), renderer, "text", JH_LIST_COLUMN_VALUE, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

	return store;
}

static void
emmp_header_remove_sensitivity (EMMailerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prefs->header_list);
	gboolean is_default;

	/* remove button should be sensitive if the currenlty selected entry in the list view
           is not a default header. if there are no entries, or none is selected, it should be
           disabled
	*/
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter,
				    HEADER_LIST_IS_DEFAULT_COLUMN, &is_default,
				    -1);
		if (is_default)
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), FALSE);
		else
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), FALSE);
	}
}

static gboolean
emmp_header_is_valid (const gchar *header)
{
	gint len = g_utf8_strlen (header, -1);

	if (header[0] == 0
	    || g_utf8_strchr (header, len, ':') != NULL
	    || g_utf8_strchr (header, len, ' ') != NULL)
		return FALSE;

	return TRUE;
}

static void
emmp_header_add_sensitivity (EMMailerPrefs *prefs)
{
	const gchar *entry_contents;
	GtkTreeIter iter;
	gboolean valid;

	/* the add header button should be sensitive if the text box contains
	   a valid header string, that is not a duplicate with something already
	   in the list view
	*/
	entry_contents = gtk_entry_get_text (GTK_ENTRY (prefs->entry_header));
	if (!emmp_header_is_valid (entry_contents)) {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), FALSE);
		return;
	}

	/* check if this is a duplicate */
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	while (valid) {
		gchar *header_name;

		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter,
				    HEADER_LIST_HEADER_COLUMN, &header_name,
				    -1);
		if (g_ascii_strcasecmp (header_name, entry_contents) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), FALSE);
			return;
		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), TRUE);
}

static void
emmp_save_headers (EMMailerPrefs *prefs)
{
	GSList *header_list;
	GtkTreeIter iter;
	gboolean valid;

	/* Headers */
	header_list = NULL;
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	while (valid) {
		struct _EMailReaderHeader h;
		gboolean enabled;
		gchar *xml;

		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter,
				    HEADER_LIST_HEADER_COLUMN, &h.name,
				    HEADER_LIST_ENABLED_COLUMN, &enabled,
				    -1);
		h.enabled = enabled;

		if ((xml = e_mail_reader_header_to_xml (&h)))
			header_list = g_slist_append (header_list, xml);

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	}

	gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, header_list, NULL);
	g_slist_foreach (header_list, (GFunc) g_free, NULL);
	g_slist_free (header_list);
}

static void
emmp_header_list_enabled_toggled (GtkCellRendererToggle *cell, const gchar *path_string, EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;
	gint enabled;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, HEADER_LIST_ENABLED_COLUMN, &enabled, -1);
	enabled = !enabled;
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, HEADER_LIST_ENABLED_COLUMN,
			    enabled, -1);
	gtk_tree_path_free (path);

	emmp_save_headers (prefs);
}

static void
emmp_header_add_header (GtkWidget *widget, EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeIter iter;
	const gchar *text = gtk_entry_get_text (prefs->entry_header);

	g_strstrip ((gchar *)text);

	if (text && (strlen (text)>0)) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				HEADER_LIST_NAME_COLUMN, text,
				HEADER_LIST_ENABLED_COLUMN, TRUE,
				HEADER_LIST_HEADER_COLUMN, text,
				HEADER_LIST_IS_DEFAULT_COLUMN, FALSE,
				-1);
		gtk_entry_set_text (prefs->entry_header, "");
		emmp_header_remove_sensitivity (prefs);
		emmp_header_add_sensitivity (prefs);

		emmp_save_headers (prefs);
	}
}

static void
emmp_header_remove_header (GtkWidget *button, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prefs->header_list);
	GtkTreeIter iter;

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	emmp_header_remove_sensitivity (prefs);

	emmp_save_headers (prefs);
}

static void
emmp_header_list_row_selected (GtkTreeSelection *selection, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;

	emmp_header_remove_sensitivity (prefs);
}

static void
emmp_header_entry_changed (GtkWidget *entry, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;

	emmp_header_add_sensitivity (prefs);
}

static void
toggle_button_toggled (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	const gchar *key;

	key = g_object_get_data ((GObject *) toggle, "key");
	gconf_client_set_bool (prefs->gconf, key, gtk_toggle_button_get_active (toggle), NULL);
}

static void
junk_book_lookup_button_toggled (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	toggle_button_toggled (toggle, prefs);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->junk_lookup_local_only), gtk_toggle_button_get_active (toggle));
}

static void
custom_junk_button_toggled (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	toggle_button_toggled (toggle, prefs);
	if (gtk_toggle_button_get_active (toggle)) {
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_remove, TRUE);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_add, TRUE);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_tree, TRUE);
	} else {
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_tree, FALSE);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_add, FALSE);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->junk_header_remove, FALSE);
	}

}

static void
toggle_button_init (EMMailerPrefs *prefs, GtkToggleButton *toggle, gint not, const gchar *key, GCallback toggled)
{
	gboolean bool;

	bool = gconf_client_get_bool (prefs->gconf, key, NULL);
	gtk_toggle_button_set_active (toggle, not ? !bool : bool);

	if (toggled) {
		g_object_set_data ((GObject *) toggle, "key", (gpointer) key);
		g_signal_connect (toggle, "toggled", toggled, prefs);
	}

	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) toggle, FALSE);
}

static void
trash_days_changed (GtkComboBox *combo_box,
                    EMMailerPrefs *prefs)
{
	gint index;

	index = gtk_combo_box_get_active (combo_box);
	g_return_if_fail (index >= 0);
	g_return_if_fail (index < G_N_ELEMENTS (empty_trash_frequency));

	gconf_client_set_int (
		prefs->gconf,
		"/apps/evolution/mail/trash/empty_on_exit_days",
		empty_trash_frequency[index].days, NULL);
}

static void
emmp_empty_trash_init (EMMailerPrefs *prefs,
                       GtkComboBox *combo_box)
{
	gint days, hist = 0, ii;
	GtkTreeModel *model;

	days = gconf_client_get_int (
		prefs->gconf,
		"/apps/evolution/mail/trash/empty_on_exit_days", NULL);

	model = gtk_combo_box_get_model (combo_box);
	gtk_list_store_clear (GTK_LIST_STORE (model));

	for (ii = 0; ii < G_N_ELEMENTS (empty_trash_frequency); ii++) {
		if (days >= empty_trash_frequency[ii].days)
			hist = ii;
		gtk_combo_box_append_text (
			combo_box, gettext (empty_trash_frequency[ii].label));
	}

	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (trash_days_changed), prefs);

	gtk_combo_box_set_active (combo_box, hist);
}

static void
junk_days_changed (GtkComboBox *combo_box,
                   EMMailerPrefs *prefs)
{
	gint index;

	index = gtk_combo_box_get_active (combo_box);
	g_return_if_fail (index >= 0);
	g_return_if_fail (index < G_N_ELEMENTS (empty_trash_frequency));

	gconf_client_set_int (
		prefs->gconf,
		"/apps/evolution/mail/junk/empty_on_exit_days",
		empty_trash_frequency[index].days, NULL);
}

static void
emmp_empty_junk_init (EMMailerPrefs *prefs,
                      GtkComboBox *combo_box)
{
	gint days, hist = 0, ii;
	GtkTreeModel *model;

	days = gconf_client_get_int (
		prefs->gconf,
		"/apps/evolution/mail/junk/empty_on_exit_days", NULL);

	model = gtk_combo_box_get_model (combo_box);
	gtk_list_store_clear (GTK_LIST_STORE (model));

	for (ii = 0; ii < G_N_ELEMENTS (empty_trash_frequency); ii++) {
		if (days >= empty_trash_frequency[ii].days)
			hist = ii;
		gtk_combo_box_append_text (
			combo_box, gettext (empty_trash_frequency[ii].label));
	}

	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (junk_days_changed), prefs);

	gtk_combo_box_set_active (combo_box, hist);
}

static void
http_images_changed (GtkWidget *widget, EMMailerPrefs *prefs)
{
	gint when;

	if (gtk_toggle_button_get_active (prefs->images_always))
		when = MAIL_CONFIG_HTTP_ALWAYS;
	else if (gtk_toggle_button_get_active (prefs->images_sometimes))
		when = MAIL_CONFIG_HTTP_SOMETIMES;
	else
		when = MAIL_CONFIG_HTTP_NEVER;

	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", when, NULL);
}

static GtkWidget *
emmp_widget_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMMailerPrefs *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

/* plugin meta-data */
static EMConfigItem emmp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "preferences_toplevel", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "vboxMailGeneral", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.fonts", (gchar *) "vboxMessageFonts", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/10.display", (gchar *) "vboxMessageDisplay", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/20.delete", (gchar *) "vboxDeletingMail", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/30.newmail", (gchar *) "vboxNewMailNotify", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "10.html", (gchar *) "vboxHtmlMail", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.html/00.general", (gchar *) "vbox173", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.html/10.images", (gchar *) "vbox190", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "20.labels", (gchar *) "frameColours", emmp_widget_glade },
	/* this is a table, so we can't use it { E_CONFIG_SECTION, "20.labels/00.labels", "tableColours", emmp_widget_glade }, */
	{ E_CONFIG_PAGE, (gchar *) "30.headers", (gchar *) "vboxHeaderTab", emmp_widget_glade },
	/* no subvbox for section { E_CONFIG_PAGE, "30.headers/00.headers", "vbox199", emmp_widget_glade }, */
	{ E_CONFIG_PAGE, (gchar *) "40.junk", (gchar *) "vbox161", emmp_widget_glade },
	/* no subvbox for section { E_CONFIG_SECTION, "40.junk/00.general", xxx, emmp_widget_glade } */
	{ E_CONFIG_SECTION, (gchar *) "40.junk/10.options", (gchar *) "vbox204", emmp_widget_glade },
};

static void
emmp_free (EConfig *ec, GSList *items, gpointer data)
{
	/* the prefs data is freed automagically */

	g_slist_free (items);
}

static void
junk_plugin_changed (GtkWidget *combo, EMMailerPrefs *prefs)
{
	gchar *def_plugin = gtk_combo_box_get_active_text (GTK_COMBO_BOX (combo));
	const GList *plugins = mail_session_get_junk_plugins ();

	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/junk/default_plugin", def_plugin, NULL);
	while (plugins) {
		EMJunkInterface *iface = plugins->data;

		if (iface->plugin_name && def_plugin && !strcmp (iface->plugin_name, def_plugin)) {
			gboolean status;

			session->junk_plugin = CAMEL_JUNK_PLUGIN (&iface->camel);
			status = e_plugin_invoke (iface->hook->plugin, iface->validate_binary, NULL) != NULL;
			if ((gboolean)status == TRUE) {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-info", GTK_ICON_SIZE_MENU);
				text = g_strdup_printf (_("%s plugin is available and the binary is installed."), iface->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			} else {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-warning", GTK_ICON_SIZE_MENU);
				text = g_strdup_printf (_("%s plugin is not available. Please check whether the package is installed."), iface->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			}
			break;
		}
		plugins = plugins->next;
	}
}

static void
junk_plugin_setup (GtkComboBox *combo_box, EMMailerPrefs *prefs)
{
	GtkListStore *store;
	GtkCellRenderer *cell;
	gint index = 0;
	gboolean def_set = FALSE;
	const GList *plugins = mail_session_get_junk_plugins ();
	gchar *pdefault = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/junk/default_plugin", NULL);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
					"text", 0,
					NULL);

	if (!plugins || !g_list_length ((GList *)plugins)) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter, 0, _("No junk plugin available"), -1);
		gtk_combo_box_set_active (combo_box, 0);
		gtk_widget_set_sensitive (GTK_WIDGET (combo_box), FALSE);
		gtk_widget_hide (GTK_WIDGET (prefs->plugin_image));
		gtk_widget_hide (GTK_WIDGET (prefs->plugin_status));
		gtk_image_set_from_stock (prefs->plugin_image, NULL, 0);
		g_free (pdefault);

		return;
	}

	while (plugins) {
		EMJunkInterface *iface = plugins->data;
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, iface->plugin_name, -1);
		if (!def_set && pdefault && iface->plugin_name && !strcmp (pdefault, iface->plugin_name)) {
			gboolean status;

			def_set = TRUE;
			gtk_combo_box_set_active (combo_box, index);
			status = e_plugin_invoke (iface->hook->plugin, iface->validate_binary, NULL) != NULL;
			if (status) {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-info", GTK_ICON_SIZE_MENU);
				/* May be a better text */
				text = g_strdup_printf (_("%s plugin is available and the binary is installed."), iface->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			} else {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-warning", GTK_ICON_SIZE_MENU);
				/* May be a better text */
				text = g_strdup_printf (_("%s plugin is not available. Please check whether the package is installed."), iface->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			}
		}
		plugins = plugins->next;
		index++;
	}

	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (junk_plugin_changed), prefs);
	g_free (pdefault);
}

static void
em_mailer_prefs_construct (EMMailerPrefs *prefs,
                           EShell *shell)
{
	GSList *header_config_list, *header_add_list, *p;
	EShellSettings *shell_settings;
	GHashTable *default_header_hash;
	GtkWidget *toplevel;
	GtkWidget *container;
	GtkWidget *table;
	GtkWidget *widget;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	gboolean locked;
	gint val, i;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;

	shell_settings = e_shell_get_shell_settings (shell);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	EM_TYPE_FOLDER_SELECTION_BUTTON;

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	/** @HookPoint-EMConfig: Mail Preferences Page
	 * @Id: org.gnome.evolution.mail.prefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The main mail preferences page.
	 */
	ec = em_config_new(E_CONFIG_BOOK, "org.gnome.evolution.mail.prefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emmp_items); i++)
		l = g_slist_prepend (l, &emmp_items[i]);
	e_config_add_items ((EConfig *)ec, l, NULL, NULL, emmp_free, prefs);

	/* General tab */

	/* Message Display */
	widget = e_builder_get_widget (prefs->builder, "chkMarkTimeout");
	e_mutual_binding_new (
		shell_settings, "mail-mark-seen",
		widget, "active");

	/* The "mark seen" timeout requires special transform functions
	 * because we display the timeout value to the user in seconds
	 * but store the settings value in milliseconds. */
	widget = e_builder_get_widget (prefs->builder, "spinMarkTimeout");
	prefs->timeout = GTK_SPIN_BUTTON (widget);
	e_mutual_binding_new (
		shell_settings, "mail-mark-seen",
		widget, "sensitive");
	e_mutual_binding_new_full (
		shell_settings, "mail-mark-seen-timeout",
		widget, "value",
		mark_seen_timeout_transform,
		mark_seen_timeout_reverse_transform,
		NULL, NULL);

	widget = e_builder_get_widget (prefs->builder, "mlimit_checkbutton");
	e_mutual_binding_new (
		shell_settings, "mail-force-message-limit",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "mlimit_spin");
	e_mutual_binding_new (
		shell_settings, "mail-force-message-limit",
		widget, "sensitive");
	e_mutual_binding_new (
		shell_settings, "mail-message-text-part-limit",
		widget, "value");

	widget = e_builder_get_widget (prefs->builder, "address_checkbox");
	e_mutual_binding_new (
		shell_settings, "mail-address-compress",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "address_spin");
	e_mutual_binding_new (
		shell_settings, "mail-address-compress",
		widget, "sensitive");
	e_mutual_binding_new (
		shell_settings, "mail-address-count",
		widget, "value");

	widget = e_builder_get_widget (prefs->builder, "magic_spacebar_checkbox");
	e_mutual_binding_new (
		shell_settings, "mail-magic-spacebar",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "view-check");
	e_mutual_binding_new (
		shell_settings, "mail-global-view-setting",
		widget, "active");

	widget = e_charset_combo_box_new ();
	container = e_builder_get_widget (prefs->builder, "hboxDefaultCharset");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	e_mutual_binding_new (
		shell_settings, "mail-charset",
		widget, "charset");

	widget = e_builder_get_widget (prefs->builder, "chkHighlightCitations");
	e_mutual_binding_new (
		shell_settings, "mail-mark-citations",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "colorButtonHighlightCitations");
	e_mutual_binding_new (
		shell_settings, "mail-mark-citations",
		widget, "sensitive");
	e_mutual_binding_new_full (
		shell_settings, "mail-citation-color",
		widget, "color",
		e_binding_transform_string_to_color,
		e_binding_transform_color_to_string,
		NULL, NULL);

	widget = e_builder_get_widget (prefs->builder, "chkEnableSearchFolders");
	e_mutual_binding_new (
		shell_settings, "mail-enable-search-folders",
		widget, "active");

	/* Deleting Mail */
	widget = e_builder_get_widget (prefs->builder, "chkEmptyTrashOnExit");
	e_mutual_binding_new (
		shell_settings, "mail-empty-trash-on-exit",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "comboboxEmptyTrashDays");
	e_mutual_binding_new (
		shell_settings, "mail-empty-trash-on-exit",
		widget, "sensitive");
	emmp_empty_trash_init (prefs, GTK_COMBO_BOX (widget));

	widget = e_builder_get_widget (prefs->builder, "chkConfirmExpunge");
	e_mutual_binding_new (
		shell_settings, "mail-confirm-expunge",
		widget, "active");

	/* Mail Fonts */
	widget = e_builder_get_widget (prefs->builder, "radFontUseSame");
	e_mutual_binding_new_with_negation (
		shell_settings, "mail-use-custom-fonts",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "FontFixed");
	e_mutual_binding_new (
		shell_settings, "mail-font-monospace",
		widget, "font-name");
	e_mutual_binding_new (
		shell_settings, "mail-use-custom-fonts",
		widget, "sensitive");

	widget = e_builder_get_widget (prefs->builder, "FontVariable");
	e_mutual_binding_new (
		shell_settings, "mail-font-variable",
		widget, "font-name");
	e_mutual_binding_new (
		shell_settings, "mail-use-custom-fonts",
		widget, "sensitive");

	/* HTML Mail tab */

	/* Loading Images */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);

	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);
	prefs->images_never = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "radImagesNever"));
	gtk_toggle_button_set_active (prefs->images_never, val == MAIL_CONFIG_HTTP_NEVER);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_never, FALSE);

	prefs->images_sometimes = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "radImagesSometimes"));
	gtk_toggle_button_set_active (prefs->images_sometimes, val == MAIL_CONFIG_HTTP_SOMETIMES);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_sometimes, FALSE);

	prefs->images_always = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "radImagesAlways"));
	gtk_toggle_button_set_active (prefs->images_always, val == MAIL_CONFIG_HTTP_ALWAYS);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_always, FALSE);

	g_signal_connect (prefs->images_never, "toggled", G_CALLBACK (http_images_changed), prefs);
	g_signal_connect (prefs->images_sometimes, "toggled", G_CALLBACK (http_images_changed), prefs);
	g_signal_connect (prefs->images_always, "toggled", G_CALLBACK (http_images_changed), prefs);

	widget = e_builder_get_widget (prefs->builder, "chkShowAnimatedImages");
	e_mutual_binding_new (
		shell_settings, "mail-show-animated-images",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptWantHTML");
	e_mutual_binding_new (
		shell_settings, "mail-confirm-unwanted-html",
		widget, "active");

	container = e_builder_get_widget (prefs->builder, "labels-alignment");
	widget = e_mail_label_manager_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	e_binding_new (
		shell_settings, "mail-label-list-store",
		widget, "list-store");

	/* headers */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/headers", NULL);

	widget = e_builder_get_widget (prefs->builder, "photo_show");
	e_mutual_binding_new (
		shell_settings, "mail-show-sender-photo",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "photo_local");
	e_mutual_binding_new (
		shell_settings, "mail-show-sender-photo",
		widget, "sensitive");
	e_mutual_binding_new (
		shell_settings, "mail-only-local-photos",
		widget, "active");

	/* always de-sensitised until the user types something in the entry */
	prefs->add_header = GTK_BUTTON (e_builder_get_widget (prefs->builder, "cmdHeadersAdd"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->add_header, FALSE);

	/* always de-sensitised until the user selects a header in the list */
	prefs->remove_header = GTK_BUTTON (e_builder_get_widget (prefs->builder, "cmdHeadersRemove"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->remove_header, FALSE);

	prefs->entry_header = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtHeaders"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->entry_header, !locked);

	prefs->header_list = GTK_TREE_VIEW (e_builder_get_widget (prefs->builder, "treeHeaders"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->header_list, !locked);

	selection = gtk_tree_view_get_selection (prefs->header_list);
	g_signal_connect (selection, "changed", G_CALLBACK (emmp_header_list_row_selected), prefs);
	g_signal_connect (prefs->entry_header, "changed", G_CALLBACK (emmp_header_entry_changed), prefs);
	g_signal_connect (prefs->entry_header, "activate", G_CALLBACK (emmp_header_add_header), prefs);
	/* initialise the tree with appropriate headings */
	prefs->header_list_store = gtk_list_store_newv (HEADER_LIST_N_COLUMNS, col_types);
	g_signal_connect (prefs->add_header, "clicked", G_CALLBACK (emmp_header_add_header), prefs);
	g_signal_connect (prefs->remove_header, "clicked", G_CALLBACK (emmp_header_remove_header), prefs);
	gtk_tree_view_set_model (prefs->header_list, GTK_TREE_MODEL (prefs->header_list_store));

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (renderer, "activatable", TRUE, NULL);
	g_signal_connect (renderer, "toggled", G_CALLBACK (emmp_header_list_enabled_toggled), prefs);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (prefs->header_list), -1,
						     "Enabled", renderer,
						     "active", HEADER_LIST_ENABLED_COLUMN,
						     NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (prefs->header_list), -1,
						     "Name", renderer,
						     "text", HEADER_LIST_NAME_COLUMN,
						     NULL);

	/* populated the listview with entries; firstly we add all the default headers, and then
	   we add read header configuration out of gconf. If a header in gconf is a default header,
	   we update the enabled flag accordingly
	*/
	header_add_list = NULL;
	default_header_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < G_N_ELEMENTS (default_headers); i++) {
		EMailReaderHeader *h;

		h = g_malloc (sizeof (EMailReaderHeader));
		h->is_default = TRUE;
		h->name = g_strdup (default_headers[i]);
		h->enabled = strcmp ((gchar *)default_headers[i], "x-evolution-mailer") != 0;
		g_hash_table_insert (default_header_hash, (gpointer) default_headers[i], h);
		header_add_list = g_slist_append (header_add_list, h);
	}

	/* read stored headers from gconf */
	header_config_list = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, NULL);
	p = header_config_list;
	while (p) {
		EMailReaderHeader *h, *def;
		gchar *xml = (gchar *) p->data;

		h = e_mail_reader_header_from_xml (xml);
		if (h) {
			def = g_hash_table_lookup (default_header_hash, h->name);
			if (def) {
				def->enabled = h->enabled;
				e_mail_reader_header_free (h);
			} else {
				h->is_default = FALSE;
				header_add_list = g_slist_append (header_add_list, h);
			}
		}

		p = p->next;
	}

	g_hash_table_destroy (default_header_hash);
	g_slist_foreach (header_config_list, (GFunc) g_free, NULL);
	g_slist_free (header_config_list);

	p = header_add_list;
	while (p) {
		struct _EMailReaderHeader *h = (struct _EMailReaderHeader *) p->data;
		const gchar *name;

		if (g_ascii_strcasecmp (h->name, EM_FORMAT_HEADER_XMAILER) == 0)
			name = _("Mailer");
		else
			name = _(h->name);

		gtk_list_store_append (prefs->header_list_store, &iter);
		gtk_list_store_set (prefs->header_list_store, &iter,
				    HEADER_LIST_NAME_COLUMN, name,
				    HEADER_LIST_ENABLED_COLUMN, h->enabled,
				    HEADER_LIST_IS_DEFAULT_COLUMN, h->is_default,
				    HEADER_LIST_HEADER_COLUMN, h->name,
				    -1);

		e_mail_reader_header_free (h);
		p = p->next;
	}

	g_slist_free (header_add_list);

	/* date/time format */
	table = e_builder_get_widget (prefs->builder, "datetime_format_table");
	/* To Translators: 'Table column' is a label for configurable date/time format for table columns showing a date in message list */
	e_datetime_format_add_setup_widget (table, 0, "mail", "table",  DTFormatKindDateTime, _("_Table column:"));
	/* To Translators: 'Date header' is a label for configurable date/time format for 'Date' header in mail message window/preview */
	e_datetime_format_add_setup_widget (table, 1, "mail", "header", DTFormatKindDateTime, _("_Date header:"));
	widget = gtk_check_button_new_with_mnemonic (_("Show _original header value"));
	gtk_widget_show (widget);
	gtk_table_attach ((GtkTable *) table, widget, 0, 3, 2, 3, GTK_EXPAND | GTK_FILL, 0, 12, 0);
	e_mutual_binding_new (
		shell_settings, "mail-show-real-date",
		widget, "active");

	/* Junk prefs */
	widget = e_builder_get_widget (prefs->builder, "chkCheckIncomingMail");
	e_mutual_binding_new (
		shell_settings, "mail-check-for-junk",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "junk_empty_check");
	e_mutual_binding_new (
		shell_settings, "mail-empty-junk-on-exit",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "junk_empty_combobox");
	e_mutual_binding_new (
		shell_settings, "mail-empty-junk-on-exit",
		widget, "sensitive");
	emmp_empty_junk_init (prefs, GTK_COMBO_BOX (widget));

	prefs->default_junk_plugin = GTK_COMBO_BOX (e_builder_get_widget (prefs->builder, "default_junk_plugin"));
	prefs->plugin_status = GTK_LABEL (e_builder_get_widget (prefs->builder, "plugin_status"));
	prefs->plugin_image = GTK_IMAGE (e_builder_get_widget (prefs->builder, "plugin_image"));
	junk_plugin_setup (prefs->default_junk_plugin, prefs);

	prefs->junk_header_check = (GtkToggleButton *)e_builder_get_widget (prefs->builder, "junk_header_check");
	prefs->junk_header_tree = (GtkTreeView *)e_builder_get_widget (prefs->builder, "junk_header_tree");
	prefs->junk_header_add = (GtkButton *)e_builder_get_widget (prefs->builder, "junk_header_add");
	prefs->junk_header_remove = (GtkButton *)e_builder_get_widget (prefs->builder, "junk_header_remove");
	prefs->junk_book_lookup = (GtkToggleButton *)e_builder_get_widget (prefs->builder, "lookup_book");
	prefs->junk_lookup_local_only = (GtkToggleButton *)e_builder_get_widget (prefs->builder, "junk_lookup_local_only");
	toggle_button_init (prefs, prefs->junk_book_lookup, FALSE,
			    "/apps/evolution/mail/junk/lookup_addressbook",
			    G_CALLBACK (junk_book_lookup_button_toggled));

	toggle_button_init (prefs, prefs->junk_lookup_local_only, FALSE,
			    "/apps/evolution/mail/junk/lookup_addressbook_local_only",
			    G_CALLBACK (toggle_button_toggled));

	junk_book_lookup_button_toggled (prefs->junk_book_lookup, prefs);

	prefs->junk_header_list_store = init_junk_tree ((GtkWidget *)prefs->junk_header_tree, prefs);
	toggle_button_init (prefs, prefs->junk_header_check, FALSE,
			    "/apps/evolution/mail/junk/check_custom_header",
			    G_CALLBACK (custom_junk_button_toggled));

	custom_junk_button_toggled (prefs->junk_header_check, prefs);
	jh_tree_refill (prefs);
	g_signal_connect (G_OBJECT (prefs->junk_header_add), "clicked", G_CALLBACK (jh_add_cb), prefs);
	g_signal_connect (G_OBJECT (prefs->junk_header_remove), "clicked", G_CALLBACK (jh_remove_cb), prefs);

	/* Sanitize the dialog for Express mode */
	e_shell_hide_widgets_for_express_mode (shell, prefs->builder,
					       "hboxReadTimeout",
					       "hboxMailSizeLimit",
					       "hboxShrinkAddresses",
					       "magic_spacebar_checkbox",
					       "hboxEnableSearchFolders",
					       NULL);

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec, prefs->gconf);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
}

GtkWidget *
em_mailer_prefs_new (EPreferencesWindow *window)
{
	EMMailerPrefs *new;
	EShell *shell = e_preferences_window_get_shell (window);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	new = g_object_new (EM_TYPE_MAILER_PREFS, NULL);

	/* FIXME Kill this function. */
	em_mailer_prefs_construct (new, shell);

	return GTK_WIDGET (new);
}
