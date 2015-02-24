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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>

#include "em-mailer-prefs.h"

#include <libxml/tree.h>

#include <shell/e-shell-utils.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-junk-options.h>
#include <mail/e-mail-label-manager.h>
#include <mail/e-mail-reader-utils.h>
#include <mail/em-folder-selection-button.h>
#include <mail/em-config.h>

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

#define EM_FORMAT_HEADER_XMAILER "x-evolution-mailer"

/* Keep this synchronized with the "show-headers" key
 * in the "org.gnome.evolution.mail" GSettings schema. */
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
	EM_FORMAT_HEADER_XMAILER /* DO NOT translate */
};

/* for empty trash on exit frequency */
static const struct {
	const gchar *label;
	gint days;
} empty_trash_frequency[] = {
	{ N_("On exit, every time"), 0 },
	{ N_("Once per day"), 1 },
	{ N_("Once per week"), 7 },
	{ N_("Once per month"), 30 },
	{ N_("Immediately, on folder leave"), -1 }
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
	g_object_unref (prefs->settings);

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
	preferences->settings = e_util_ref_settings ("org.gnome.evolution.mail");
}

static gboolean
mailer_prefs_map_milliseconds_to_seconds (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
	gint32 milliseconds;
	gdouble seconds;

	milliseconds = g_variant_get_int32 (variant);
	seconds = milliseconds / 1000.0;
	g_value_set_double (value, seconds);

	return TRUE;
}

static GVariant *
mailer_prefs_map_seconds_to_milliseconds (const GValue *value,
                                          const GVariantType *expected_type,
                                          gpointer user_data)
{
	gint32 milliseconds;
	gdouble seconds;

	seconds = g_value_get_double (value);
	milliseconds = seconds * 1000;

	return g_variant_new_int32 (milliseconds);
}

static gboolean
mailer_prefs_map_string_to_rgba (GValue *value,
                                 GVariant *variant,
                                 gpointer user_data)
{
	GdkRGBA rgba;
	const gchar *string;
	gboolean success = FALSE;

	string = g_variant_get_string (variant, NULL);
	if (gdk_rgba_parse (&rgba, string)) {
		g_value_set_boxed (value, &rgba);
		success = TRUE;
	}

	return success;
}

static GVariant *
mailer_prefs_map_rgba_to_string (const GValue *value,
                                 const GVariantType *expected_type,
                                 gpointer user_data)
{
	GVariant *variant;
	const GdkRGBA *rgba;

	rgba = g_value_get_boxed (value);
	if (rgba == NULL) {
		variant = g_variant_new_string ("");
	} else {
		gchar *string;

		/* Encode the color manually. */
		string = g_strdup_printf (
			"#%02x%02x%02x",
			((gint) (rgba->red * 255)) % 255,
			((gint) (rgba->green * 255)) % 255,
			((gint) (rgba->blue * 255)) % 255);
		variant = g_variant_new_string (string);
		g_free (string);
	}

	return variant;
}

enum {
	JH_LIST_COLUMN_NAME,
	JH_LIST_COLUMN_VALUE
};

static void
jh_tree_refill (EMMailerPrefs *prefs)
{
	GtkListStore *store = prefs->junk_header_list_store;
	gchar **strv;
	gint ii;

	strv = g_settings_get_strv (prefs->settings, "junk-custom-header");

	gtk_list_store_clear (store);

	for (ii = 0; strv[ii] != NULL; ii++) {
		GtkTreeIter iter;
		gchar **tokens = g_strsplit (strv[ii], "=", 2);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			JH_LIST_COLUMN_NAME , tokens[0] ? tokens[0] : "",
			JH_LIST_COLUMN_VALUE, tokens[1] ? tokens[1] : "" ,
			-1);
		g_strfreev (tokens);
	}

	g_strfreev (strv);
}

static void
jh_dialog_entry_changed_cb (GtkEntry *entry,
                            gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
	GtkWidget *ok_button, *entry1, *entry2;
	const gchar *name, *value;

	ok_button = e_builder_get_widget (builder, "junk-header-ok");
	entry1 = e_builder_get_widget (builder, "junk-header-name");
	entry2 = e_builder_get_widget (builder, "junk-header-content");

	name = gtk_entry_get_text (GTK_ENTRY (entry1));
	value = gtk_entry_get_text (GTK_ENTRY (entry2));

	gtk_widget_set_sensitive (ok_button, name && *name && value && *value);
}

static void
jh_add_cb (GtkWidget *widget,
           gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	GtkBuilder *builder = gtk_builder_new ();
	gchar *tok;
	const gchar *name, *value;

	g_type_ensure (E_TYPE_MAIL_JUNK_OPTIONS);

	e_load_ui_builder_definition (builder, "mail-config.ui");
	dialog = e_builder_get_widget (builder, "add-custom-junk-header");
	jh_dialog_entry_changed_cb (NULL, builder);

	entry = e_builder_get_widget (builder, "junk-header-name");
	g_signal_connect (
		entry, "changed",
		G_CALLBACK (jh_dialog_entry_changed_cb), builder);
	entry = e_builder_get_widget (builder, "junk-header-content");
	g_signal_connect (
		entry, "changed",
		G_CALLBACK (jh_dialog_entry_changed_cb), builder);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar **strv;
		GPtrArray *array;
		gint ii;

		name = gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (builder, "junk-header-name")));
		value = gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (builder, "junk-header-content")));

		strv = g_settings_get_strv (prefs->settings, "junk-custom-header");
		array = g_ptr_array_new ();
		for (ii = 0; strv[ii] != NULL; ii++)
			g_ptr_array_add (array, strv[ii]);
		tok = g_strdup_printf ("%s=%s", name, value);
		g_ptr_array_add (array, tok);
		g_ptr_array_add (array, NULL);
		g_settings_set_strv (prefs->settings, "junk-custom-header", (const gchar * const *) array->pdata);

		g_ptr_array_free (array, TRUE);
		g_strfreev (strv);
	}

	g_object_unref (builder);
	gtk_widget_destroy (dialog);

	jh_tree_refill (prefs);
}

static void
jh_remove_cb (GtkWidget *widget,
              gpointer user_data)
{
	EMMailerPrefs *prefs = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (prefs != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->junk_header_tree));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GPtrArray *array = g_ptr_array_new ();
		gchar *name = NULL, *value = NULL;
		gchar **strv;
		gint ii;

		strv = g_settings_get_strv (prefs->settings, "junk-custom-header");
		gtk_tree_model_get (model, &iter, JH_LIST_COLUMN_NAME, &name, JH_LIST_COLUMN_VALUE, &value, -1);
		for (ii = 0; strv[ii] != NULL; ii++) {
			gchar *test;
			gint len = strlen (name);
			test = strncmp (strv[ii], name, len) == 0 ? (gchar *) strv[ii] + len : NULL;

			if (test) {
				test++;
				if (strcmp (test, value) == 0)
					continue;
			}

			g_ptr_array_add (array, strv[ii]);
		}

		g_ptr_array_add (array, NULL);

		g_settings_set_strv (prefs->settings, "junk-custom-header", (const gchar * const *) array->pdata);

		g_strfreev (strv);
		g_ptr_array_free (array, TRUE);
		g_free (name);
		g_free (value);

		jh_tree_refill (prefs);
	}
}

static GtkListStore *
init_junk_tree (GtkWidget *label_tree,
                EMMailerPrefs *prefs)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;

	g_return_val_if_fail (label_tree != NULL, NULL);
	g_return_val_if_fail (prefs != NULL, NULL);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (label_tree), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Header"), renderer, "text", JH_LIST_COLUMN_NAME, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (label_tree), -1, _("Contains Value"), renderer, "text", JH_LIST_COLUMN_VALUE, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);

	return store;
}

static void
emmp_header_remove_sensitivity (EMMailerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prefs->header_list);
	gboolean is_default;

	/* remove button should be sensitive if the currenlty selected entry in the list view
	 * is not a default header. if there are no entries, or none is selected, it should be
	 * disabled
	*/
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (
			GTK_TREE_MODEL (prefs->header_list_store), &iter,
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
	 * a valid header string, that is not a duplicate with something already
	 * in the list view
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

		gtk_tree_model_get (
			GTK_TREE_MODEL (prefs->header_list_store), &iter,
			HEADER_LIST_HEADER_COLUMN, &header_name,
			-1);
		if (g_ascii_strcasecmp (header_name, entry_contents) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), FALSE);
			g_free (header_name);
			return;
		}

		g_free (header_name);

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), TRUE);
}

static void
emmp_save_headers (EMMailerPrefs *prefs)
{
	GVariantBuilder builder;
	GtkTreeModel *model;
	GVariant *variant;
	GtkTreeIter iter;
	gboolean valid;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sb)"));

	model = GTK_TREE_MODEL (prefs->header_list_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gchar *name = NULL;
		gboolean enabled = TRUE;

		gtk_tree_model_get (
			model, &iter,
			HEADER_LIST_HEADER_COLUMN, &name,
			HEADER_LIST_ENABLED_COLUMN, &enabled,
			-1);

		if (name != NULL) {
			g_variant_builder_add (
				&builder, "(sb)", name, enabled);
			g_free (name);
		}

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	variant = g_variant_builder_end (&builder);
	g_settings_set_value (prefs->settings, "show-headers", variant);
}

static void
emmp_header_list_enabled_toggled (GtkCellRendererToggle *cell,
                                  const gchar *path_string,
                                  EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;
	gint enabled;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (
		model, &iter,
		HEADER_LIST_ENABLED_COLUMN, &enabled, -1);
	enabled = !enabled;
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		HEADER_LIST_ENABLED_COLUMN, enabled, -1);
	gtk_tree_path_free (path);

	emmp_save_headers (prefs);
}

static void
emmp_header_add_header (GtkWidget *widget,
                        EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeIter iter;
	const gchar *text = gtk_entry_get_text (prefs->entry_header);

	g_strstrip ((gchar *) text);

	if (text && (strlen (text) > 0)) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
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
emmp_header_remove_header (GtkWidget *button,
                           gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (prefs->header_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	emmp_header_remove_sensitivity (prefs);

	emmp_save_headers (prefs);
}

static void
emmp_header_list_row_selected (GtkTreeSelection *selection,
                               gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;

	emmp_header_remove_sensitivity (prefs);
}

static void
emmp_header_entry_changed (GtkWidget *entry,
                           gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;

	emmp_header_add_sensitivity (prefs);
}

static void
toggle_button_toggled (GtkToggleButton *toggle,
                       EMMailerPrefs *prefs)
{
	const gchar *key;

	key = g_object_get_data ((GObject *) toggle, "key");
	g_settings_set_boolean (
		prefs->settings, key,
		gtk_toggle_button_get_active (toggle));
}

static void
junk_book_lookup_button_toggled (GtkToggleButton *toggle,
                                 EMMailerPrefs *prefs)
{
	toggle_button_toggled (toggle, prefs);
	gtk_widget_set_sensitive (
		GTK_WIDGET (prefs->junk_lookup_local_only),
		gtk_toggle_button_get_active (toggle));
}

static void
custom_junk_button_toggled (GtkToggleButton *toggle,
                            EMMailerPrefs *prefs)
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
toggle_button_init (EMMailerPrefs *prefs,
                    GtkToggleButton *toggle,
                    gint not,
                    const gchar *key,
                    GCallback toggled)
{
	gboolean v_bool;

	v_bool = g_settings_get_boolean (prefs->settings, key);
	gtk_toggle_button_set_active (toggle, not ? !v_bool : v_bool);

	if (toggled) {
		g_object_set_data ((GObject *) toggle, "key", (gpointer) key);
		g_signal_connect (
			toggle, "toggled", toggled, prefs);
	}

	if (!g_settings_is_writable (prefs->settings, key))
		gtk_widget_set_sensitive (GTK_WIDGET (toggle), FALSE);
}

static void
trash_days_changed (GtkComboBox *combo_box,
                    EMMailerPrefs *prefs)
{
	gint index;

	index = gtk_combo_box_get_active (combo_box);
	g_return_if_fail (index >= 0);
	g_return_if_fail (index < G_N_ELEMENTS (empty_trash_frequency));

	g_settings_set_int (
		prefs->settings,
		"trash-empty-on-exit-days",
		empty_trash_frequency[index].days);
}

static void
emmp_empty_trash_init (EMMailerPrefs *prefs,
                       GtkComboBox *combo_box)
{
	gint days, hist = 0, ii;
	GtkListStore *store;
	GtkTreeIter iter;

	days = g_settings_get_int (
		prefs->settings,
		"trash-empty-on-exit-days");

	store = GTK_LIST_STORE (gtk_combo_box_get_model (combo_box));
	gtk_list_store_clear (store);

	for (ii = 0; ii < G_N_ELEMENTS (empty_trash_frequency); ii++) {
		if (days == empty_trash_frequency[ii].days ||
		    (empty_trash_frequency[ii].days != -1 && days > empty_trash_frequency[ii].days))
			hist = ii;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0,  gettext (empty_trash_frequency[ii].label),
			-1);
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

	g_settings_set_int (
		prefs->settings,
		"junk-empty-on-exit-days",
		empty_trash_frequency[index].days);
}

static void
emmp_empty_junk_init (EMMailerPrefs *prefs,
                      GtkComboBox *combo_box)
{
	gint days, hist = 0, ii;
	GtkListStore *store;
	GtkTreeIter iter;

	days = g_settings_get_int (
		prefs->settings,
		"junk-empty-on-exit-days");

	store = GTK_LIST_STORE (gtk_combo_box_get_model (combo_box));
	gtk_list_store_clear (store);

	for (ii = 0; ii < G_N_ELEMENTS (empty_trash_frequency); ii++) {
		if (days == empty_trash_frequency[ii].days ||
		    (empty_trash_frequency[ii].days != -1 && days >= empty_trash_frequency[ii].days))
			hist = ii;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, gettext (empty_trash_frequency[ii].label),
			-1);
	}

	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (junk_days_changed), prefs);

	gtk_combo_box_set_active (combo_box, hist);
}

static void
image_loading_policy_always_cb (GtkToggleButton *toggle_button)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		g_settings_set_enum (
			settings, "image-loading-policy",
			E_IMAGE_LOADING_POLICY_ALWAYS);

		g_object_unref (settings);
	}
}

static void
image_loading_policy_sometimes_cb (GtkToggleButton *toggle_button)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		g_settings_set_enum (
			settings, "image-loading-policy",
			E_IMAGE_LOADING_POLICY_SOMETIMES);

		g_object_unref (settings);
	}
}

static void
image_loading_policy_never_cb (GtkToggleButton *toggle_button)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		g_settings_set_enum (
			settings, "image-loading-policy",
			E_IMAGE_LOADING_POLICY_NEVER);

		g_object_unref (settings);
	}
}

static GtkWidget *
emmp_widget_glade (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer data)
{
	EMMailerPrefs *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

/* plugin meta-data */
static EMConfigItem emmp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "preferences_toplevel", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "vboxMailGeneral", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/10.display", (gchar *) "message-display-vbox", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/20.delete", (gchar *) "delete-mail-vbox", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "10.html", (gchar *) "vboxHtmlMail", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.html/00.general", (gchar *) "html-general-vbox", emmp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.html/10.images", (gchar *) "loading-images-vbox", emmp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "20.labels", (gchar *) "frameColours", emmp_widget_glade },
	/* this is a table, so we can't use it { E_CONFIG_SECTION, "20.labels/00.labels", "tableColours", emmp_widget_glade }, */
	{ E_CONFIG_PAGE, (gchar *) "30.headers", (gchar *) "vboxHeaderTab", emmp_widget_glade },
	/* no subvbox for section { E_CONFIG_PAGE, "30.headers/00.headers", "vbox199", emmp_widget_glade }, */
	{ E_CONFIG_PAGE, (gchar *) "40.junk", (gchar *) "vboxJunk", emmp_widget_glade },
	/* no subvbox for section { E_CONFIG_SECTION, "40.junk/00.general", xxx, emmp_widget_glade } */
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.junk/10.options", (gchar *) "junk-general-table", emmp_widget_glade },
};

static void
emmp_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	/* the prefs data is freed automagically */

	g_slist_free (items);
}

static void
em_mailer_prefs_construct (EMMailerPrefs *prefs,
                           EMailSession *session,
                           EShell *shell)
{
	GSettings *settings;
	GHashTable *default_header_hash;
	GtkWidget *toplevel;
	GtkWidget *container;
	GtkWidget *table;
	GtkWidget *widget;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GVariant *variant;
	gsize ii, n_children;
	gboolean locked;
	gboolean writable;
	gint val, i;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_MAIL_JUNK_OPTIONS);

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	/** @HookPoint-EMConfig: Mail Preferences Page
	 * @Id: org.gnome.evolution.mail.prefs
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The main mail preferences page.
	 */
	ec = em_config_new ("org.gnome.evolution.mail.prefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emmp_items); i++)
		l = g_slist_prepend (l, &emmp_items[i]);
	e_config_add_items ((EConfig *) ec, l, emmp_free, prefs);

	/* General tab */

	widget = e_builder_get_widget (prefs->builder, "chkCheckMailOnStart");
	g_settings_bind (
		settings, "send-recv-on-start",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkCheckMailInAllOnStart");
	g_settings_bind (
		settings, "send-recv-all-on-start",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "send-recv-on-start",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	/* Message Display */

	widget = e_builder_get_widget (prefs->builder, "chkMarkTimeout");
	g_settings_bind (
		settings, "mark-seen",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	/* The "mark seen" timeout requires special transform functions
	 * because we display the timeout value to the user in seconds
	 * but store the settings value in milliseconds. */
	widget = e_builder_get_widget (prefs->builder, "spinMarkTimeout");
	g_settings_bind_with_mapping (
		settings, "mark-seen-timeout",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT,
		mailer_prefs_map_milliseconds_to_seconds,
		mailer_prefs_map_seconds_to_milliseconds,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "mark-seen",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "view-check");
	g_settings_bind (
		settings, "global-view-setting",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_charset_combo_box_new ();
	container = e_builder_get_widget (prefs->builder, "hboxDefaultCharset");
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (e_builder_get_widget (
		prefs->builder, "lblDefaultCharset")), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_settings_bind (
		settings, "charset",
		widget, "charset",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkHighlightCitations");
	g_settings_bind (
		settings, "mark-citations",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "colorButtonHighlightCitations");
	g_settings_bind_with_mapping (
		settings, "citation-color",
		widget, "rgba",
		G_SETTINGS_BIND_DEFAULT,
		mailer_prefs_map_string_to_rgba,
		mailer_prefs_map_rgba_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "mark-citations",
		widget, "sensitive",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "thread-by-subject");
	g_settings_bind (
		settings, "thread-subject",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	/* Deleting Mail */
	widget = e_builder_get_widget (prefs->builder, "chkEmptyTrashOnExit");
	g_settings_bind (
		settings, "trash-empty-on-exit",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "comboboxEmptyTrashDays");
	g_settings_bind (
		settings, "trash-empty-on-exit",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);
	emmp_empty_trash_init (prefs, GTK_COMBO_BOX (widget));

	widget = e_builder_get_widget (prefs->builder, "chkConfirmExpunge");
	g_settings_bind (
		settings, "prompt-on-expunge",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	/* Mail Fonts */
	widget = e_builder_get_widget (prefs->builder, "radFontUseSame");
	g_settings_bind (
		settings, "use-custom-font",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	widget = e_builder_get_widget (prefs->builder, "FontFixed");
	g_settings_bind (
		settings, "monospace-font",
		widget, "font-name",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "use-custom-font",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "FontVariable");
	g_settings_bind (
		settings, "variable-width-font",
		widget, "font-name",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "use-custom-font",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	/* HTML Mail tab */

	/* Loading Images */
	writable = g_settings_is_writable (
		prefs->settings, "image-loading-policy");

	val = g_settings_get_enum (prefs->settings, "image-loading-policy");
	widget = e_builder_get_widget (
		prefs->builder, "radImagesNever");
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (widget),
		val == E_IMAGE_LOADING_POLICY_NEVER);
	gtk_widget_set_sensitive (widget, writable);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (image_loading_policy_never_cb), NULL);

	widget = e_builder_get_widget (
		prefs->builder, "radImagesSometimes");
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (widget),
		val == E_IMAGE_LOADING_POLICY_SOMETIMES);
	gtk_widget_set_sensitive (widget, writable);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (image_loading_policy_sometimes_cb), NULL);

	widget = e_builder_get_widget (
		prefs->builder, "radImagesAlways");
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (widget),
		val == E_IMAGE_LOADING_POLICY_ALWAYS);
	gtk_widget_set_sensitive (widget, writable);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (image_loading_policy_always_cb), NULL);

	widget = e_builder_get_widget (prefs->builder, "chkShowAnimatedImages");
	g_settings_bind (
		settings, "show-animated-images",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptWantHTML");
	g_settings_bind (
		settings, "prompt-on-unwanted-html",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	container = e_builder_get_widget (prefs->builder, "labels-alignment");
	widget = e_mail_label_manager_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		session, "label-store",
		widget, "list-store",
		G_BINDING_SYNC_CREATE);

	/* headers */
	locked = !g_settings_is_writable (prefs->settings, "headers");

	widget = e_builder_get_widget (prefs->builder, "photo_show");
	g_settings_bind (
		settings, "show-sender-photo",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "search_gravatar");
	g_settings_bind (
		settings, "search-gravatar-for-photo",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "show-sender-photo",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	container = e_builder_get_widget (prefs->builder, "archive-mail-hbox");
	widget = em_folder_selection_button_new (session, "", _("Choose a folder to archive messages to."));
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (e_builder_get_widget (prefs->builder, "lblArchiveMailFolder")), widget);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_settings_bind (
		settings, "local-archive-folder",
		widget, "folder-uri",
		G_SETTINGS_BIND_DEFAULT);

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
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (emmp_header_list_row_selected), prefs);
	g_signal_connect (
		prefs->entry_header, "changed",
		G_CALLBACK (emmp_header_entry_changed), prefs);
	g_signal_connect (
		prefs->entry_header,
		"activate", G_CALLBACK (emmp_header_add_header), prefs);
	/* initialise the tree with appropriate headings */
	prefs->header_list_store = gtk_list_store_newv (HEADER_LIST_N_COLUMNS, col_types);
	g_signal_connect (
		prefs->add_header, "clicked",
		G_CALLBACK (emmp_header_add_header), prefs);
	g_signal_connect (
		prefs->remove_header, "clicked",
		G_CALLBACK (emmp_header_remove_header), prefs);
	gtk_tree_view_set_model (prefs->header_list, GTK_TREE_MODEL (prefs->header_list_store));

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (renderer, "activatable", TRUE, NULL);
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (emmp_header_list_enabled_toggled), prefs);
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (prefs->header_list), -1,
		"Enabled", renderer,
		"active", HEADER_LIST_ENABLED_COLUMN,
		NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (prefs->header_list), -1,
		"Name", renderer,
		"text", HEADER_LIST_NAME_COLUMN,
		NULL);

	/* Populate the list store with entries.  Firstly we add all the
	 * default headers, and then we add read header configuration out of
	 * settings. If a header in settings is a default header, we update
	 * the enabled flag accordingly. */

	/* FIXME Split the headers section into a separate widget to
	 *       better isolate its functionality.  There's too much
	 *       complexity to just embed it like this. */

	default_header_hash = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_path_free);

	tree_model = GTK_TREE_MODEL (prefs->header_list_store);

	for (ii = 0; ii < G_N_ELEMENTS (default_headers); ii++) {
		GtkTreeIter iter;
		const gchar *display_name;
		const gchar *header_name;
		gboolean enabled;

		header_name = default_headers[ii];
		if (g_strcmp0 (header_name, EM_FORMAT_HEADER_XMAILER) == 0) {
			display_name = _("Mailer");
			enabled = FALSE;
		} else {
			display_name = _(header_name);
			enabled = TRUE;
		}

		gtk_list_store_append (
			GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			HEADER_LIST_NAME_COLUMN, display_name,
			HEADER_LIST_ENABLED_COLUMN, enabled,
			HEADER_LIST_IS_DEFAULT_COLUMN, TRUE,
			HEADER_LIST_HEADER_COLUMN, header_name,
			-1);

		g_hash_table_insert (
			default_header_hash, g_strdup (header_name),
			gtk_tree_model_get_path (tree_model, &iter));
	}

	variant = g_settings_get_value (prefs->settings, "show-headers");
	n_children = g_variant_n_children (variant);

	for (ii = 0; ii < n_children; ii++) {
		GtkTreeIter iter;
		GtkTreePath *path;
		const gchar *header_name = NULL;
		gboolean enabled = FALSE;

		g_variant_get_child (
			variant, ii, "(&sb)", &header_name, &enabled);

		if (header_name == NULL) {
			g_warn_if_reached ();
			continue;
		}

		path = g_hash_table_lookup (default_header_hash, header_name);
		if (path != NULL) {
			gtk_tree_model_get_iter (tree_model, &iter, path);
			gtk_list_store_set (
				GTK_LIST_STORE (tree_model), &iter,
				HEADER_LIST_ENABLED_COLUMN, enabled,
				-1);
		} else {
			gtk_list_store_append (
				GTK_LIST_STORE (tree_model), &iter);

			gtk_list_store_set (
				GTK_LIST_STORE (tree_model), &iter,
				HEADER_LIST_NAME_COLUMN, _(header_name),
				HEADER_LIST_ENABLED_COLUMN, enabled,
				HEADER_LIST_IS_DEFAULT_COLUMN, FALSE,
				HEADER_LIST_HEADER_COLUMN, header_name,
				-1);
		}
	}

	g_variant_unref (variant);

	g_hash_table_destroy (default_header_hash);

	/* date/time format */
	table = e_builder_get_widget (prefs->builder, "datetime-format-table");
	/* To Translators: 'Table column' is a label for configurable date/time format for table columns showing a date in message list */
	e_datetime_format_add_setup_widget (table, 0, "mail", "table",  DTFormatKindDateTime, _("_Table column:"));
	/* To Translators: 'Date header' is a label for configurable date/time format for 'Date' header in mail message window/preview */
	e_datetime_format_add_setup_widget (table, 1, "mail", "header", DTFormatKindDateTime, _("_Date header:"));
	widget = gtk_check_button_new_with_mnemonic (_("Show _original header value"));
	gtk_widget_show (widget);
	gtk_table_attach ((GtkTable *) table, widget, 0, 3, 2, 3, GTK_EXPAND | GTK_FILL, 0, 12, 0);
	g_settings_bind (
		settings, "show-real-date",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	/* Junk prefs */
	widget = e_builder_get_widget (prefs->builder, "chkCheckIncomingMail");
	g_settings_bind (
		settings, "junk-check-incoming",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "junk_empty_check");
	g_settings_bind (
		settings, "junk-empty-on-exit",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "junk_empty_combobox");
	emmp_empty_junk_init (prefs, GTK_COMBO_BOX (widget));
	g_settings_bind (
		settings, "junk-empty-on-exit",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "junk-module-options");
	e_mail_junk_options_set_session (E_MAIL_JUNK_OPTIONS (widget), session);

	prefs->junk_header_check = (GtkToggleButton *) e_builder_get_widget (prefs->builder, "junk_header_check");
	prefs->junk_header_tree = (GtkTreeView *) e_builder_get_widget (prefs->builder, "junk_header_tree");
	prefs->junk_header_add = (GtkButton *) e_builder_get_widget (prefs->builder, "junk_header_add");
	prefs->junk_header_remove = (GtkButton *) e_builder_get_widget (prefs->builder, "junk_header_remove");
	prefs->junk_book_lookup = (GtkToggleButton *) e_builder_get_widget (prefs->builder, "lookup_book");
	prefs->junk_lookup_local_only = (GtkToggleButton *) e_builder_get_widget (prefs->builder, "junk_lookup_local_only");
	toggle_button_init (
		prefs, prefs->junk_book_lookup,
		FALSE, "junk-lookup-addressbook",
		G_CALLBACK (junk_book_lookup_button_toggled));

	toggle_button_init (
		prefs, prefs->junk_lookup_local_only,
		FALSE, "junk-lookup-addressbook-local-only",
		G_CALLBACK (toggle_button_toggled));

	junk_book_lookup_button_toggled (prefs->junk_book_lookup, prefs);

	prefs->junk_header_list_store = init_junk_tree ((GtkWidget *) prefs->junk_header_tree, prefs);
	toggle_button_init (
		prefs, prefs->junk_header_check,
		FALSE, "junk-check-custom-header",
		G_CALLBACK (custom_junk_button_toggled));

	custom_junk_button_toggled (prefs->junk_header_check, prefs);
	jh_tree_refill (prefs);
	g_signal_connect (
		prefs->junk_header_add, "clicked",
		G_CALLBACK (jh_add_cb), prefs);
	g_signal_connect (
		prefs->junk_header_remove, "clicked",
		G_CALLBACK (jh_remove_cb), prefs);

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *) ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	g_object_unref (settings);
}

GtkWidget *
em_mailer_prefs_new (EPreferencesWindow *window)
{
	EMMailerPrefs *new;
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	/* XXX Figure out a better way to get the EMailSession. */
	shell = e_preferences_window_get_shell (window);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	new = g_object_new (EM_TYPE_MAILER_PREFS, NULL);

	/* FIXME Kill this function. */
	em_mailer_prefs_construct (new, session, shell);

	return GTK_WIDGET (new);
}
