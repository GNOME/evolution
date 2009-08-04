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

#include "em-mailer-prefs.h"
#include "em-format.h"

#include <camel/camel-iconv.h>
#include <gtkhtml/gtkhtml-properties.h>
#include <libxml/tree.h>
#include "misc/e-charset-picker.h"
#include <bonobo/bonobo-generic-factory.h>

#include <glade/glade.h>

#include <gconf/gconf-client.h>

#include "libedataserverui/e-cell-renderer-color.h"

#include "e-util/e-datetime-format.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util-labels.h"

#include "mail-config.h"
#include "em-junk-hook.h"
#include "em-config.h"
#include "mail-session.h"

static void em_mailer_prefs_class_init (EMMailerPrefsClass *class);
static void em_mailer_prefs_init       (EMMailerPrefs *dialog);
static void em_mailer_prefs_dispose    (GObject *obj);

static GtkVBoxClass *parent_class = NULL;

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

GType
em_mailer_prefs_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo type_info = {
			sizeof (EMMailerPrefsClass),
			NULL, NULL,
			(GClassInitFunc) em_mailer_prefs_class_init,
			NULL, NULL,
			sizeof (EMMailerPrefs),
			0,
			(GInstanceInitFunc) em_mailer_prefs_init,
		};

		type = g_type_register_static (gtk_vbox_get_type (), "EMMailerPrefs", &type_info, 0);
	}

	return type;
}

static void
em_mailer_prefs_class_init (EMMailerPrefsClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (gtk_vbox_get_type ());

	object_class->dispose = em_mailer_prefs_dispose;
}

static void
em_mailer_prefs_init (EMMailerPrefs *preferences)
{
	preferences->gconf = mail_config_get_gconf_client ();
}

static void
em_mailer_prefs_dispose (GObject *obj)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) obj;

	if (prefs->header_list_store) {
		g_object_unref (prefs->header_list_store);
		prefs->header_list_store = NULL;
	}

	g_object_unref (prefs->gui);

	if (prefs->labels_change_notify_id) {
		gconf_client_notify_remove (prefs->gconf, prefs->labels_change_notify_id);

		prefs->labels_change_notify_id = 0;
	}

        ((GObjectClass *)(parent_class))->dispose (obj);
}

static void
color_button_set_color (GtkColorButton *color_button, const gchar *spec)
{
	GdkColor color;

	if (gdk_color_parse (spec, &color))
		gtk_color_button_set_color (color_button, &color);
}

static void
citation_color_set (GtkColorButton *color_button, EMMailerPrefs *prefs)
{
	GdkColor color;
	gchar spec[16];

	gtk_color_button_get_color (color_button, &color);
	g_snprintf (spec, sizeof (spec), "#%04x%04x%04x",
		color.red, color.green, color.blue);

	gconf_client_set_string (prefs->gconf,
		"/apps/evolution/mail/display/citation_colour",
		spec, NULL);
}

enum {
	LABEL_LIST_COLUMN_COLOR,
	LABEL_LIST_COLUMN_TAG,
	LABEL_LIST_COLUMN_NAME
};

enum {
	JH_LIST_COLUMN_NAME,
	JH_LIST_COLUMN_VALUE
};
static void
label_sensitive_buttons (EMMailerPrefs *prefs)
{
	gboolean can_remove = FALSE, have_selected = FALSE, locked;

	g_return_if_fail (prefs);

	/* it's not sensitive if it's locked for updates */
	locked = !GTK_WIDGET_IS_SENSITIVE (prefs->label_tree);

	if (!locked) {
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->label_tree));
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			gchar *tag = NULL;

			gtk_tree_model_get (model, &iter, LABEL_LIST_COLUMN_TAG, &tag, -1);

			can_remove = tag && !e_util_labels_is_system (tag);
			have_selected = TRUE;

			g_free (tag);
		}
	}

	gtk_widget_set_sensitive (prefs->label_remove, !locked && can_remove);
	gtk_widget_set_sensitive (prefs->label_edit,  !locked && have_selected);
}

static void
label_tree_cursor_changed (GtkWidget *widget, gpointer user_data)
{
	label_sensitive_buttons (user_data);
}

static void
label_tree_refill (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *)user_data;
	GSList *labels, *l;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter last_iter;
	gchar *last_path = NULL;

	g_return_if_fail (prefs != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->label_tree));
	if (gtk_tree_selection_get_selected (selection, &model, &last_iter))
		last_path = gtk_tree_model_get_string_from_iter (model, &last_iter);

	store = GTK_LIST_STORE (model);
	gtk_list_store_clear (store);

	/* cannot use mail-config cache here, because it's (or can be) updated later than this function call */
	labels = e_util_labels_parse (client);

	for (l = labels; l; l = l->next) {
		GdkColor color;
		GtkTreeIter iter;
		EUtilLabel *label = l->data;

		if (label->colour)
			gdk_color_parse (label->colour, &color);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			LABEL_LIST_COLUMN_COLOR, label->colour ? &color : NULL,
			LABEL_LIST_COLUMN_NAME, label->name,
			LABEL_LIST_COLUMN_TAG, label->tag,
			-1);
	}

	if (last_path) {
		gint children;

		children = gtk_tree_model_iter_n_children (model, NULL);
		if (children > 0) {
			GtkTreePath *path;

			if (!gtk_tree_model_get_iter_from_string (model, &last_iter, last_path))
				gtk_tree_model_iter_nth_child (model, &last_iter, NULL, children - 1);

			path = gtk_tree_model_get_path (model, &last_iter);
			if (path) {
				GtkTreeViewColumn *focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (prefs->label_tree), LABEL_LIST_COLUMN_NAME);

				gtk_tree_view_set_cursor (GTK_TREE_VIEW (prefs->label_tree), path, focus_col, FALSE);
				gtk_tree_view_row_activated (GTK_TREE_VIEW (prefs->label_tree), path, focus_col);
				gtk_tree_path_free (path);
			}
		}

		g_free (last_path);
	}

	label_sensitive_buttons (prefs);
	e_util_labels_free (labels);
}

static void
jh_tree_refill (EMMailerPrefs *prefs)
{
	GtkListStore *store;
	GSList *l, *cjh = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/junk/custom_header", GCONF_VALUE_STRING, NULL);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (prefs->junk_header_tree)));
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
	gint response;
	dialog = gtk_dialog_new_with_buttons (_("Add Custom Junk Header"), (GtkWindow *)gtk_widget_get_toplevel (widget), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

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
	gtk_container_add ((GtkContainer *)((GtkDialog *)dialog)->vbox, vbox);
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

static gboolean
init_junk_tree (GtkTreeView *jh_tree, EMMailerPrefs *prefs)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	gint col;

	g_return_val_if_fail (jh_tree != NULL, FALSE);
	g_return_val_if_fail (prefs != NULL, FALSE);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (jh_tree), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (jh_tree), -1, _("Header"), renderer, "text", JH_LIST_COLUMN_NAME, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (jh_tree), -1, _("Contains Value"), renderer, "text", JH_LIST_COLUMN_VALUE, NULL);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

	jh_tree_refill (prefs);

	return TRUE;
}

static gboolean
init_label_tree (GtkWidget *label_tree, EMMailerPrefs *prefs, gboolean locked)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	gint col;

	g_return_val_if_fail (label_tree != NULL, FALSE);
	g_return_val_if_fail (prefs != NULL, FALSE);

	store = gtk_list_store_new (3, GDK_TYPE_COLOR, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (label_tree), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = e_cell_renderer_color_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Color"), renderer, "color", LABEL_LIST_COLUMN_COLOR, NULL);

	renderer = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Tag"), renderer, "text", LABEL_LIST_COLUMN_TAG, NULL);
	g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
	gtk_tree_view_column_set_visible (gtk_tree_view_get_column (GTK_TREE_VIEW (label_tree), col - 1), FALSE);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Name"), renderer, "text", LABEL_LIST_COLUMN_NAME, NULL);
	g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

	if (!locked)
		g_signal_connect (label_tree, "cursor-changed", G_CALLBACK (label_tree_cursor_changed), prefs);

	label_tree_refill (NULL, 0, NULL, prefs);

	prefs->labels_change_notify_id = gconf_client_notify_add (prefs->gconf, E_UTIL_LABELS_GCONF_KEY, label_tree_refill, prefs, NULL, NULL);

	return TRUE;
}

static void
label_add_cb (GtkWidget *widget, gpointer user_data)
{
	gchar *tag;

	tag = e_util_labels_add_with_dlg (GTK_WINDOW (gtk_widget_get_toplevel (widget)), NULL);

	g_free (tag);
}

static void
label_remove_cb (GtkWidget *widget, gpointer user_data)
{
	EMMailerPrefs *prefs = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (prefs != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->label_tree));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *tag = NULL;

		gtk_tree_model_get (model, &iter, LABEL_LIST_COLUMN_TAG, &tag, -1);

		if (tag && !e_util_labels_is_system (tag))
			e_util_labels_remove (tag);

		g_free (tag);
	}
}

static void
label_edit_cb (GtkWidget *widget, gpointer user_data)
{
	EMMailerPrefs *prefs = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (prefs != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->label_tree));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *tag = NULL;

		gtk_tree_model_get (model, &iter, LABEL_LIST_COLUMN_TAG, &tag, -1);

		if (tag) {
			gchar *str = e_util_labels_add_with_dlg (GTK_WINDOW (gtk_widget_get_toplevel (widget)), tag);

			g_free (str);
		}

		g_free (tag);
	}
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
		struct _EMMailerPrefsHeader h;
		gboolean enabled;
		gchar *xml;

		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter,
				    HEADER_LIST_HEADER_COLUMN, &h.name,
				    HEADER_LIST_ENABLED_COLUMN, &enabled,
				    -1);
		h.enabled = enabled;

		if ((xml = em_mailer_prefs_header_to_xml (&h)))
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
mark_seen_timeout_changed (GtkSpinButton *spin, EMMailerPrefs *prefs)
{
	gint timeout;

	timeout = (gint) (gtk_spin_button_get_value (prefs->timeout) * 1000.0);
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/mark_seen_timeout", timeout, NULL);
}

static void
address_compress_count_changed (GtkSpinButton *spin, EMMailerPrefs *prefs)
{
	gint count;

	count = (gint) gtk_spin_button_get_value (prefs->address_count);

	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/address_count", count, NULL);
}

static void
mlimit_count_changed (GtkSpinButton *spin, EMMailerPrefs *prefs)
{
	gint count;

	count = (gint) gtk_spin_button_get_value (prefs->mlimit_count);

	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/message_text_part_limit", count, NULL);
}

static void
spin_button_init (EMMailerPrefs *prefs, GtkSpinButton *spin, const gchar *key, gfloat div, GCallback value_changed)
{
	GError *err = NULL;
	double min, max;
	gchar *mkey, *p;
	gint val;

	gtk_spin_button_get_range (spin, &min, &max);

	mkey = g_alloca (strlen (key) + 5);
	p = g_stpcpy (mkey, key);
	*p++ = '_';

	/* see if the admin locked down the min value */
	strcpy (p, "min");
	val = gconf_client_get_int (prefs->gconf, mkey, &err);
	if (err == NULL)
		g_clear_error (&err);
	else
		min = (1.0 * val) / div;

	/* see if the admin locked down the max value */
	strcpy (p, "max");
	val = gconf_client_get_int (prefs->gconf, mkey, &err);
	if (err == NULL)
		g_clear_error (&err);
	else
		max = (1.0 * val) / div;

	gtk_spin_button_set_range (spin, min, max);

	/* get the value */
	val = gconf_client_get_int (prefs->gconf, key, NULL);
	gtk_spin_button_set_value (spin, (1.0 * val) / div);

	if (value_changed) {
		g_object_set_data ((GObject *) spin, "key", (gpointer) key);
		g_signal_connect (spin, "value-changed", value_changed, prefs);
	}

	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) spin, FALSE);
}

static void
toggle_button_toggled (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	const gchar *key;

	key = g_object_get_data ((GObject *) toggle, "key");
	gconf_client_set_bool (prefs->gconf, key, gtk_toggle_button_get_active (toggle), NULL);
}

static void
photo_toggle_changed (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	toggle_button_toggled (toggle, prefs);
	if (gtk_toggle_button_get_active (toggle))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->photo_local, TRUE);
	else
		gtk_widget_set_sensitive ((GtkWidget *) prefs->photo_local, FALSE);
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
custom_font_changed (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	gboolean use_custom;

	use_custom = !gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_fixed), use_custom);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_variable), use_custom);

	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/fonts/use_custom", use_custom, NULL);
}

static void
font_changed (GtkFontButton *font_button, EMMailerPrefs *prefs)
{
	const gchar *key;
	const gchar *font_name;

	key = g_object_get_data (G_OBJECT (font_button), "key");
	font_name = gtk_font_button_get_font_name (font_button);
	gconf_client_set_string (prefs->gconf, key, font_name, NULL);
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
charset_activate (GtkWidget *item, EMMailerPrefs *prefs)
{
	GtkWidget *menu;
	gchar *string;

	menu = gtk_option_menu_get_menu (prefs->charset);
	if (!(string = e_charset_picker_get_charset (menu)))
		string = g_strdup (camel_iconv_locale_charset ());

	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/charset", string, NULL);
	g_free (string);
}

static void
charset_menu_init (EMMailerPrefs *prefs)
{
	GtkWidget *menu, *item;
	GList *items;
	gchar *buf;

	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/charset", NULL);
	menu = e_charset_picker_new (buf && *buf ? buf : camel_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	g_free (buf);

	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		g_signal_connect (item, "activate", G_CALLBACK (charset_activate), prefs);
		items = items->next;
	}

	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/charset", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->charset, FALSE);
}

static void
trash_days_changed (GtkComboBox *combobox, EMMailerPrefs *prefs)
{
	gint idx;

	idx = gtk_combo_box_get_active (combobox);
	g_return_if_fail (idx >= 0 && idx < G_N_ELEMENTS (empty_trash_frequency));

	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", empty_trash_frequency [idx].days, NULL);
}

static void
emmp_empty_trash_init (EMMailerPrefs *prefs)
{
	gint locked, days, hist = 0, i;

	toggle_button_init (prefs, prefs->empty_trash, FALSE,
			    "/apps/evolution/mail/trash/empty_on_exit",
			    G_CALLBACK (toggle_button_toggled));

	days = gconf_client_get_int(prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL);

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (prefs->empty_trash_days)));
	for (i = 0; i < G_N_ELEMENTS (empty_trash_frequency); i++) {
		if (days >= empty_trash_frequency[i].days)
			hist = i;

		gtk_combo_box_append_text  (prefs->empty_trash_days, _(empty_trash_frequency[i].label));
	}

	g_signal_connect (prefs->empty_trash_days, "changed", G_CALLBACK (trash_days_changed), prefs);
	gtk_combo_box_set_active (prefs->empty_trash_days, hist);

	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->empty_trash_days, !locked);
}

static void
junk_days_changed (GtkComboBox *combobox, EMMailerPrefs *prefs)
{
	gint idx;

	idx = gtk_combo_box_get_active (combobox);
	g_return_if_fail (idx >= 0 && idx < G_N_ELEMENTS (empty_trash_frequency));

	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/junk/empty_on_exit_days", empty_trash_frequency[idx].days, NULL);
}

static void
emmp_empty_junk_init (EMMailerPrefs *prefs)
{
	gint locked, days, hist = 0, i;

	toggle_button_init (prefs, prefs->empty_junk, FALSE,
			    "/apps/evolution/mail/junk/empty_on_exit",
			    G_CALLBACK (toggle_button_toggled));

	days = gconf_client_get_int(prefs->gconf, "/apps/evolution/mail/junk/empty_on_exit_days", NULL);

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (prefs->empty_junk_days)));
	for (i = 0; i < G_N_ELEMENTS (empty_trash_frequency); i++) {
		if (days >= empty_trash_frequency[i].days)
			hist = i;

		gtk_combo_box_append_text  (prefs->empty_junk_days, _(empty_trash_frequency[i].label));
	}

	g_signal_connect (prefs->empty_junk_days, "changed", G_CALLBACK (junk_days_changed), prefs);
	gtk_combo_box_set_active (prefs->empty_junk_days, hist);

	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/junk/empty_on_exit_days", NULL);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->empty_junk_days, !locked);
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
emmp_widget_glade(EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMMailerPrefs *prefs = data;

	return glade_xml_get_widget(prefs->gui, item->label);
}

/* plugin meta-data */
static EMConfigItem emmp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "preferences_toplevel", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "vboxGeneral", emmp_widget_glade },
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
emmp_free(EConfig *ec, GSList *items, gpointer data)
{
	/* the prefs data is freed automagically */

	g_slist_free(items);
}

static void
junk_plugin_changed (GtkWidget *combo, EMMailerPrefs *prefs)
{
	gchar *def_plugin = gtk_combo_box_get_active_text(GTK_COMBO_BOX (combo));
	const GList *plugins = mail_session_get_junk_plugins();

	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/junk/default_plugin", def_plugin, NULL);
	while (plugins) {
		struct _EMJunkHookItem *item = plugins->data;;

		if (item->plugin_name && def_plugin && !strcmp (item->plugin_name, def_plugin)) {
			gboolean status;

			session->junk_plugin = CAMEL_JUNK_PLUGIN (&(item->csp));
			status = e_plugin_invoke (item->hook->hook.plugin, item->validate_binary, NULL) != NULL;
			if ((gboolean)status == TRUE) {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-info", GTK_ICON_SIZE_MENU);
				text = g_strdup_printf (_("%s plugin is available and the binary is installed."), item->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			} else {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-warning", GTK_ICON_SIZE_MENU);
				text = g_strdup_printf (_("%s plugin is not available. Please check whether the package is installed."), item->plugin_name);
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
junk_plugin_setup (GtkWidget *combo, EMMailerPrefs *prefs)
{
	gint index = 0;
	gboolean def_set = FALSE;
	const GList *plugins = mail_session_get_junk_plugins();
	gchar *pdefault = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/junk/default_plugin", NULL);

	if (!plugins || !g_list_length ((GList *)plugins)) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("No Junk plugin available"));
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
		gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);
		gtk_widget_hide (GTK_WIDGET (prefs->plugin_image));
		gtk_widget_hide (GTK_WIDGET (prefs->plugin_status));
		gtk_image_set_from_stock (prefs->plugin_image, NULL, 0);
		g_free (pdefault);

		return;
	}

	while (plugins) {
		struct _EMJunkHookItem *item = plugins->data;;

		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), item->plugin_name);
		if (!def_set && pdefault && item->plugin_name && !strcmp(pdefault, item->plugin_name)) {
			gboolean status;

			def_set = TRUE;
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo), index);
			status = e_plugin_invoke (item->hook->hook.plugin, item->validate_binary, NULL) != NULL;
			if (status) {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-info", GTK_ICON_SIZE_MENU);
				/* May be a better text */
				text = g_strdup_printf (_("%s plugin is available and the binary is installed."), item->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			} else {
				gchar *text, *html;
				gtk_image_set_from_stock (prefs->plugin_image, "gtk-dialog-warning", GTK_ICON_SIZE_MENU);
				/* May be a better text */
				text = g_strdup_printf (_("%s plugin is not available. Please check whether the package is installed."), item->plugin_name);
				html = g_strdup_printf ("<i>%s</i>", text);
				gtk_label_set_markup (prefs->plugin_status, html);
				g_free (html);
				g_free (text);
			}
		}
		plugins = plugins->next;
		index++;
	}

	g_signal_connect (combo, "changed", G_CALLBACK(junk_plugin_changed), prefs);
	g_free (pdefault);
}

GtkWidget *
create_combo_text_widget (void) {
	return gtk_combo_box_new_text ();
}

static void
em_mailer_prefs_construct (EMMailerPrefs *prefs)
{
	GSList *header_config_list, *header_add_list, *p;
	GHashTable *default_header_hash;
	GtkWidget *toplevel, *table;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	gchar *font, *buf;
	GladeXML *gui;
	gboolean locked;
	gint val, i;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;
	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "preferences_toplevel", NULL);
	g_free (gladefile);

	prefs->gui = gui;

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
	for (i=0;i<sizeof(emmp_items)/sizeof(emmp_items[0]);i++)
		l = g_slist_prepend(l, &emmp_items[i]);
	e_config_add_items((EConfig *)ec, l, NULL, NULL, emmp_free, prefs);

	/* General tab */

	/* Message Display */
	prefs->timeout_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkMarkTimeout"));
	toggle_button_init (prefs, prefs->timeout_toggle, FALSE,
			    "/apps/evolution/mail/display/mark_seen",
			    G_CALLBACK (toggle_button_toggled));

	prefs->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	spin_button_init (prefs, prefs->timeout,
			  "/apps/evolution/mail/display/mark_seen_timeout",
			  1000.0, G_CALLBACK (mark_seen_timeout_changed));

	prefs->mlimit_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "mlimit_checkbutton"));
	toggle_button_init (prefs, prefs->mlimit_toggle, FALSE,
			    "/apps/evolution/mail/display/force_message_limit",
			    G_CALLBACK (toggle_button_toggled));

	prefs->magic_spacebar = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "magic_spacebar_checkbox"));
	toggle_button_init (prefs, prefs->magic_spacebar, FALSE,
			    "/apps/evolution/mail/display/magic_spacebar",
			    G_CALLBACK (toggle_button_toggled));

	prefs->mlimit_count = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "mlimit_spin"));
	spin_button_init (prefs, prefs->mlimit_count,
			  "/apps/evolution/mail/display/message_text_part_limit",
			  1, G_CALLBACK (mlimit_count_changed));

	prefs->address_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "address_checkbox"));
	toggle_button_init (prefs, prefs->address_toggle, FALSE,
			    "/apps/evolution/mail/display/address_compress",
			    G_CALLBACK (toggle_button_toggled));

	prefs->address_count = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "address_spin"));
	spin_button_init (prefs, prefs->address_count,
			  "/apps/evolution/mail/display/address_count",
			  1, G_CALLBACK (address_compress_count_changed));

	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	charset_menu_init (prefs);

	prefs->citation_highlight = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHighlightCitations"));
	toggle_button_init (prefs, prefs->citation_highlight, FALSE,
			    "/apps/evolution/mail/display/mark_citations",
			    G_CALLBACK (toggle_button_toggled));

	prefs->citation_color = GTK_COLOR_BUTTON (glade_xml_get_widget (gui, "colorButtonHighlightCitations"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/citation_colour", NULL);
	color_button_set_color (prefs->citation_color, buf ? buf : "#737373");
	g_signal_connect (prefs->citation_color, "color-set", G_CALLBACK (citation_color_set), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/citation_colour", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->citation_color, FALSE);
	g_free (buf);

	prefs->enable_search_folders = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEnableSearchFolders"));
	toggle_button_init (prefs, prefs->enable_search_folders, FALSE,
			    "/apps/evolution/mail/display/enable_vfolders",
			    G_CALLBACK (toggle_button_toggled));

	/* Deleting Mail */
	prefs->empty_trash = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEmptyTrashOnExit"));
	prefs->empty_trash_days = GTK_COMBO_BOX (glade_xml_get_widget (gui, "comboboxEmptyTrashDays"));
	emmp_empty_trash_init (prefs);

	prefs->confirm_expunge = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkConfirmExpunge"));
	toggle_button_init (prefs, prefs->confirm_expunge, FALSE,
			    "/apps/evolution/mail/prompts/expunge",
			    G_CALLBACK (toggle_button_toggled));

	/* Mail  Fonts */
	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL);
	prefs->font_fixed = GTK_FONT_BUTTON (glade_xml_get_widget (gui, "FontFixed"));
	gtk_font_button_set_font_name (prefs->font_fixed, font);
	g_free (font);
	g_object_set_data ((GObject *) prefs->font_fixed, "key", (gpointer) "/apps/evolution/mail/display/fonts/monospace");
	g_signal_connect (prefs->font_fixed, "font-set", G_CALLBACK (font_changed), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->font_fixed, FALSE);

	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/variable", NULL);
	prefs->font_variable = GTK_FONT_BUTTON (glade_xml_get_widget (gui, "FontVariable"));
	gtk_font_button_set_font_name (prefs->font_variable, font);
	g_free (font);
	g_object_set_data ((GObject *) prefs->font_variable, "key", (gpointer) "/apps/evolution/mail/display/fonts/variable");
	g_signal_connect (prefs->font_variable, "font-set", G_CALLBACK (font_changed), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/fonts/variable", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->font_variable, FALSE);

	prefs->font_share = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radFontUseSame"));
	toggle_button_init (prefs, prefs->font_share, TRUE,
			    "/apps/evolution/mail/display/fonts/use_custom",
			    G_CALLBACK (custom_font_changed));
	custom_font_changed (prefs->font_share, prefs);

	/* HTML Mail tab */

	/* Loading Images */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);

	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);
	prefs->images_never = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesNever"));
	gtk_toggle_button_set_active (prefs->images_never, val == MAIL_CONFIG_HTTP_NEVER);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_never, FALSE);

	prefs->images_sometimes = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesSometimes"));
	gtk_toggle_button_set_active (prefs->images_sometimes, val == MAIL_CONFIG_HTTP_SOMETIMES);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_sometimes, FALSE);

	prefs->images_always = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesAlways"));
	gtk_toggle_button_set_active (prefs->images_always, val == MAIL_CONFIG_HTTP_ALWAYS);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_always, FALSE);

	g_signal_connect (prefs->images_never, "toggled", G_CALLBACK (http_images_changed), prefs);
	g_signal_connect (prefs->images_sometimes, "toggled", G_CALLBACK (http_images_changed), prefs);
	g_signal_connect (prefs->images_always, "toggled", G_CALLBACK (http_images_changed), prefs);

	prefs->show_animated = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkShowAnimatedImages"));
	toggle_button_init (prefs, prefs->show_animated, FALSE,
			    "/apps/evolution/mail/display/animate_images",
			    G_CALLBACK (toggle_button_toggled));

	prefs->prompt_unwanted_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptWantHTML"));
	toggle_button_init (prefs, prefs->prompt_unwanted_html, FALSE,
			    "/apps/evolution/mail/prompts/unwanted_html",
			    G_CALLBACK (toggle_button_toggled));

	/* Labels... */
	locked = !gconf_client_key_is_writable (prefs->gconf, E_UTIL_LABELS_GCONF_KEY, NULL);
	prefs->label_add    = glade_xml_get_widget (gui, "labelAdd");
	prefs->label_edit   = glade_xml_get_widget (gui, "labelEdit");
	prefs->label_remove = glade_xml_get_widget (gui, "labelRemove");
	prefs->label_tree   = glade_xml_get_widget (gui, "labelTree");

	gtk_widget_set_sensitive (prefs->label_add, !locked);
	gtk_widget_set_sensitive (prefs->label_remove, !locked);
	gtk_widget_set_sensitive (prefs->label_edit, !locked);
	gtk_widget_set_sensitive (prefs->label_tree, !locked);

	init_label_tree (prefs->label_tree, prefs, locked);

	if (!locked) {
		g_signal_connect (G_OBJECT (prefs->label_add), "clicked", G_CALLBACK (label_add_cb), prefs);
		g_signal_connect (G_OBJECT (prefs->label_remove), "clicked", G_CALLBACK (label_remove_cb), prefs);
		g_signal_connect (G_OBJECT (prefs->label_edit), "clicked", G_CALLBACK (label_edit_cb), prefs);
	}

	/* headers */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/headers", NULL);

	prefs->photo_show= GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "photo_show"));
	toggle_button_init (prefs, prefs->photo_show, FALSE,
			    "/apps/evolution/mail/display/sender_photo",
			    G_CALLBACK (photo_toggle_changed));
	prefs->photo_local = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "photo_local"));
	toggle_button_init (prefs, prefs->photo_local, FALSE,
			    "/apps/evolution/mail/display/photo_local",
			    G_CALLBACK (toggle_button_toggled));
	if (!gtk_toggle_button_get_active (prefs->photo_show))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->photo_local, FALSE);

	/* always de-sensitised until the user types something in the entry */
	prefs->add_header = GTK_BUTTON (glade_xml_get_widget (gui, "cmdHeadersAdd"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->add_header, FALSE);

	/* always de-sensitised until the user selects a header in the list */
	prefs->remove_header = GTK_BUTTON (glade_xml_get_widget (gui, "cmdHeadersRemove"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->remove_header, FALSE);

	prefs->entry_header = GTK_ENTRY (glade_xml_get_widget (gui, "txtHeaders"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->entry_header, !locked);

	prefs->header_list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "treeHeaders"));
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
		struct _EMMailerPrefsHeader *h;

		h = g_malloc (sizeof (struct _EMMailerPrefsHeader));
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
		struct _EMMailerPrefsHeader *h, *def;
		gchar *xml = (gchar *) p->data;

		h = em_mailer_prefs_header_from_xml (xml);
		if (h) {
			def = g_hash_table_lookup (default_header_hash, h->name);
			if (def) {
				def->enabled = h->enabled;
				em_mailer_prefs_header_free (h);
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
		struct _EMMailerPrefsHeader *h = (struct _EMMailerPrefsHeader *) p->data;
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

		em_mailer_prefs_header_free (h);
		p = p->next;
	}

	g_slist_free (header_add_list);

	/* date/time format */
	table = glade_xml_get_widget (gui, "datetime_format_table");
	e_datetime_format_add_setup_widget (table, 0, "mail", "header", DTFormatKindDateTime, _("Date header:"));
	e_datetime_format_add_setup_widget (table, 1, "mail", "table",  DTFormatKindDateTime, _("Table column:"));

	/* Junk prefs */
	prefs->check_incoming = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkCheckIncomingMail"));
	toggle_button_init (prefs, prefs->check_incoming, FALSE,
			    "/apps/evolution/mail/junk/check_incoming",
			    G_CALLBACK (toggle_button_toggled));

	prefs->empty_junk = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "junk_empty_check"));
	prefs->empty_junk_days = GTK_COMBO_BOX (glade_xml_get_widget (gui, "junk_empty_combobox"));
	emmp_empty_junk_init (prefs);

	prefs->default_junk_plugin = GTK_COMBO_BOX (glade_xml_get_widget (gui, "default_junk_plugin"));
	prefs->plugin_status = GTK_LABEL (glade_xml_get_widget (gui, "plugin_status"));
	prefs->plugin_image = GTK_IMAGE (glade_xml_get_widget (gui, "plugin_image"));
	junk_plugin_setup (GTK_WIDGET (prefs->default_junk_plugin), prefs);

	prefs->junk_header_check = (GtkToggleButton *)glade_xml_get_widget (gui, "junk_header_check");
	prefs->junk_header_tree = (GtkTreeView *)glade_xml_get_widget (gui, "junk_header_tree");
	prefs->junk_header_add = (GtkButton *)glade_xml_get_widget (gui, "junk_header_add");
	prefs->junk_header_remove = (GtkButton *)glade_xml_get_widget (gui, "junk_header_remove");
	prefs->junk_book_lookup = (GtkToggleButton *)glade_xml_get_widget (gui, "lookup_book");
	prefs->junk_lookup_local_only = (GtkToggleButton *)glade_xml_get_widget (gui, "junk_lookup_local_only");
	toggle_button_init (prefs, prefs->junk_book_lookup, FALSE,
			    "/apps/evolution/mail/junk/lookup_addressbook",
			    G_CALLBACK (junk_book_lookup_button_toggled));

	toggle_button_init (prefs, prefs->junk_lookup_local_only, FALSE,
			    "/apps/evolution/mail/junk/lookup_addressbook_local_only",
			    G_CALLBACK (toggle_button_toggled));

	junk_book_lookup_button_toggled (prefs->junk_book_lookup, prefs);

	init_junk_tree ((GtkTreeView *)prefs->junk_header_tree, prefs);
	toggle_button_init (prefs, prefs->junk_header_check, FALSE,
			    "/apps/evolution/mail/junk/check_custom_header",
			    G_CALLBACK (custom_junk_button_toggled));

	custom_junk_button_toggled (prefs->junk_header_check, prefs);
	jh_tree_refill (prefs);
	g_signal_connect (G_OBJECT (prefs->junk_header_add), "clicked", G_CALLBACK (jh_add_cb), prefs);
	g_signal_connect (G_OBJECT (prefs->junk_header_remove), "clicked", G_CALLBACK (jh_remove_cb), prefs);

	/* get our toplevel widget */
	target = em_config_target_new_prefs(ec, prefs->gconf);
	e_config_set_target((EConfig *)ec, (EConfigTarget *)target);
	toplevel = e_config_create_widget((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
}

GtkWidget *
em_mailer_prefs_new (void)
{
	EMMailerPrefs *new;

	new = (EMMailerPrefs *) g_object_new (em_mailer_prefs_get_type (), NULL);
	em_mailer_prefs_construct (new);

	return (GtkWidget *) new;
}

static struct _EMMailerPrefsHeader *
emmp_header_from_xmldoc (xmlDocPtr doc)
{
	struct _EMMailerPrefsHeader *h;
	xmlNodePtr root;
	xmlChar *name;

	if (doc == NULL)
		return NULL;

	root = doc->children;
	if (strcmp ((gchar *)root->name, "header") != 0)
		return NULL;

	name = xmlGetProp (root, (const guchar *)"name");
	if (name == NULL)
		return NULL;

	h = g_malloc0 (sizeof (struct _EMMailerPrefsHeader));
	h->name = g_strdup ((gchar *)name);
	xmlFree (name);

	if (xmlHasProp (root, (const guchar *)"enabled"))
		h->enabled = 1;
	else
		h->enabled = 0;

	return h;
}

/**
 * em_mailer_prefs_header_from_xml
 * @xml: XML configuration data
 *
 * Parses passed XML data, which should be of
 * the format <header name="foo" enabled />, and
 * returns a EMMailerPrefs structure, or NULL if there
 * is an error.
 **/
struct _EMMailerPrefsHeader *
em_mailer_prefs_header_from_xml (const gchar *xml)
{
	struct _EMMailerPrefsHeader *header;
	xmlDocPtr doc;

	if (!(doc = xmlParseDoc ((guchar *) xml)))
		return NULL;

	header = emmp_header_from_xmldoc (doc);
	xmlFreeDoc (doc);

	return header;
}

/**
 * em_mailer_prefs_header_free
 * @header: header to free
 *
 * Frees the memory associated with the passed header
 * structure.
 */
void
em_mailer_prefs_header_free (struct _EMMailerPrefsHeader *header)
{
	if (header == NULL)
		return;

	g_free (header->name);
	g_free (header);
}

/**
 * em_mailer_prefs_header_to_xml
 * @header: header from which to generate XML
 *
 * Returns the passed header as a XML structure,
 * or NULL on error
 */
gchar *
em_mailer_prefs_header_to_xml (struct _EMMailerPrefsHeader *header)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xml;
	gchar *out;
	gint size;

	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (header->name != NULL, NULL);

	doc = xmlNewDoc ((const guchar *)"1.0");

	root = xmlNewDocNode (doc, NULL, (const guchar *)"header", NULL);
	xmlSetProp (root, (const guchar *)"name", (guchar *)header->name);
	if (header->enabled)
		xmlSetProp (root, (const guchar *)"enabled", NULL);

	xmlDocSetRootElement (doc, root);
	xmlDocDumpMemory (doc, &xml, &size);
	xmlFreeDoc (doc);

	out = g_malloc (size + 1);
	memcpy (out, xml, size);
	out[size] = '\0';
	xmlFree (xml);

	return out;
}
