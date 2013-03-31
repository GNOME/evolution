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

#include "em-composer-prefs.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtkhtml/gtkhtml.h>
#include <editor/gtkhtml-spell-language.h>

#include <composer/e-msg-composer.h>

#include <shell/e-shell-utils.h>

#include <mail/em-config.h>
#include <mail/em-folder-selection-button.h>
#include <mail/e-mail-junk-options.h>

G_DEFINE_TYPE (
	EMComposerPrefs,
	em_composer_prefs,
	GTK_TYPE_VBOX)

static gboolean
composer_prefs_map_string_to_color (GValue *value,
                                    GVariant *variant,
                                    gpointer user_data)
{
	GdkColor color;
	const gchar *string;
	gboolean success = FALSE;

	string = g_variant_get_string (variant, NULL);
	if (gdk_color_parse (string, &color)) {
		g_value_set_boxed (value, &color);
		success = TRUE;
	}

	return success;
}

static GVariant *
composer_prefs_map_color_to_string (const GValue *value,
                                    const GVariantType *expected_type,
                                    gpointer user_data)
{
	GVariant *variant;
	const GdkColor *color;

	color = g_value_get_boxed (value);
	if (color == NULL) {
		variant = g_variant_new_string ("");
	} else {
		gchar *string;

		/* Encode the color manually because CSS styles expect
		 * color codes as #rrggbb, whereas gdk_color_to_string()
		 * returns color codes as #rrrrggggbbbb. */
		string = g_strdup_printf (
			"#%02x%02x%02x",
			(gint) color->red * 256 / 65536,
			(gint) color->green * 256 / 65536,
			(gint) color->blue * 256 / 65536);
		variant = g_variant_new_string (string);
		g_free (string);
	}

	return variant;
}

static void
composer_prefs_dispose (GObject *object)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) object;

	if (prefs->builder != NULL) {
		g_object_unref (prefs->builder);
		prefs->builder = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_composer_prefs_parent_class)->dispose (object);
}

static void
em_composer_prefs_class_init (EMComposerPrefsClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = composer_prefs_dispose;
}

static void
em_composer_prefs_init (EMComposerPrefs *prefs)
{
}

static void
spell_language_toggled_cb (GtkCellRendererToggle *renderer,
                           const gchar *path_string,
                           EMComposerPrefs *prefs)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean active;
	gboolean valid;

	model = prefs->language_model;

	/* Convert the path string to a tree iterator. */
	path = gtk_tree_path_new_from_string (path_string);
	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_if_fail (valid);

	/* Toggle the active state. */
	gtk_tree_model_get (model, &iter, 0, &active, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, !active, -1);
}

static void
spell_language_save (EMComposerPrefs *prefs)
{
	GList *spell_languages = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	model = prefs->language_model;

	/* Build a list of active spell languages. */
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		const GtkhtmlSpellLanguage *language;
		gboolean active;

		gtk_tree_model_get (
			model, &iter, 0, &active, 2, &language, -1);

		if (active)
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
	spell_languages = g_list_reverse (spell_languages);

	/* Update the GSettings value. */
	e_save_spell_languages (spell_languages);

	g_list_free (spell_languages);
}

static void
spell_setup (EMComposerPrefs *prefs)
{
	const GList *available_languages;
	GList *active_languages;
	GtkListStore *store;

	store = GTK_LIST_STORE (prefs->language_model);
	available_languages = gtkhtml_spell_language_get_available ();

	active_languages = e_load_spell_languages ();

	/* Populate the GtkListStore. */
	while (available_languages != NULL) {
		const GtkhtmlSpellLanguage *language;
		GtkTreeIter tree_iter;
		const gchar *name;
		gboolean active;

		language = available_languages->data;
		name = gtkhtml_spell_language_get_name (language);
		active = (g_list_find (active_languages, language) != NULL);

		gtk_list_store_append (store, &tree_iter);

		gtk_list_store_set (
			store, &tree_iter,
			0, active, 1, name, 2, language, -1);

		available_languages = available_languages->next;
	}

	g_list_free (active_languages);
}

static GtkWidget *
emcp_widget_glade (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer data)
{
	EMComposerPrefs *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

/* plugin meta-data */
static EMConfigItem emcp_items[] = {

	{ E_CONFIG_BOOK,
	  (gchar *) "",
	  (gchar *) "composer_toplevel",
	  emcp_widget_glade },

	{ E_CONFIG_PAGE,
	  (gchar *) "00.general",
	  (gchar *) "vboxComposerGeneral",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "00.general/00.behavior",
	  (gchar *) "default-behavior-vbox",
	  emcp_widget_glade },

	{ E_CONFIG_PAGE,
	  (gchar *) "10.signatures",
	  (gchar *) "vboxSignatures",
	  emcp_widget_glade },

	/* signature/signatures and signature/preview parts not usable */

	{ E_CONFIG_PAGE,
	  (gchar *) "20.spellcheck",
	  (gchar *) "vboxSpellChecking",
	  emcp_widget_glade }
};

static void
emcp_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	/* the prefs data is freed automagically */
	g_slist_free (items);
}

static void
em_composer_prefs_construct (EMComposerPrefs *prefs,
                             EShell *shell)
{
	GtkWidget *toplevel, *widget, *info_pixmap;
	GtkWidget *container;
	GSettings *settings;
	ESourceRegistry *registry;
	GtkTreeView *view;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;
	gint i;

	registry = e_shell_get_registry (shell);

	settings = g_settings_new ("org.gnome.evolution.mail");

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	E_TYPE_MAIL_JUNK_OPTIONS;
	EM_TYPE_FOLDER_SELECTION_BUTTON;

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	/** @HookPoint-EMConfig: Mail Composer Preferences
	 * @Id: org.gnome.evolution.mail.composerPrefs
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The mail composer preferences settings page.
	 */
	ec = em_config_new ("org.gnome.evolution.mail.composerPrefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emcp_items); i++)
		l = g_slist_prepend (l, &emcp_items[i]);
	e_config_add_items ((EConfig *) ec, l, emcp_free, prefs);

	/* General tab */

	/* Default Behavior */

	widget = e_builder_get_widget (prefs->builder, "chkSendHTML");
	g_settings_bind (
		settings, "composer-send-html",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptEmptySubject");
	g_settings_bind (
		settings, "prompt-on-empty-subject",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptBccOnly");
	g_settings_bind (
		settings, "prompt-on-only-bcc",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptPrivateListReply");
	g_settings_bind (
		settings, "prompt-on-private-list-reply",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptReplyManyRecips");
	g_settings_bind (
		settings, "prompt-on-reply-many-recips",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptListReplyTo");
	g_settings_bind (
		settings, "prompt-on-list-reply-to",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkPromptSendInvalidRecip");
	g_settings_bind (
		settings, "prompt-on-invalid-recip",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkAutoSmileys");
	g_settings_bind (
		settings, "composer-magic-smileys",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkRequestReceipt");
	g_settings_bind (
		settings, "composer-request-receipt",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkReplyStartBottom");
	g_settings_bind (
		settings, "composer-reply-start-bottom",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkOutlookFilenames");
	g_settings_bind (
		settings, "composer-outlook-filenames",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkIgnoreListReplyTo");
	g_settings_bind (
		settings, "composer-ignore-list-reply-to",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkGroupReplyToList");
	g_settings_bind (
		settings, "composer-group-reply-to-list",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkSignReplyIfSigned");
	g_settings_bind (
		settings, "composer-sign-reply-if-signed",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkTopSignature");
	g_settings_bind (
		settings, "composer-top-signature",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "chkEnableSpellChecking");
	g_settings_bind (
		settings, "composer-inline-spelling",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_charset_combo_box_new ();
	container = e_builder_get_widget (prefs->builder, "hboxComposerCharset");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_settings_bind (
		settings, "composer-charset",
		widget, "charset",
		G_SETTINGS_BIND_DEFAULT);

	container = e_builder_get_widget (prefs->builder, "lblCharset");
	gtk_label_set_mnemonic_widget (GTK_LABEL (container), widget);

	/* Spell Checking */
	widget = e_builder_get_widget (prefs->builder, "listSpellCheckLanguage");
	view = GTK_TREE_VIEW (widget);
	store = gtk_list_store_new (
		3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	g_signal_connect_swapped (
		store, "row-changed",
		G_CALLBACK (spell_language_save), prefs);
	prefs->language_model = GTK_TREE_MODEL (store);
	gtk_tree_view_set_model (view, prefs->language_model);
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (spell_language_toggled_cb), prefs);
	gtk_tree_view_insert_column_with_attributes (
		view, -1, _("Enabled"),
		renderer, "active", 0, NULL);

	gtk_tree_view_insert_column_with_attributes (
		view, -1, _("Language(s)"),
		gtk_cell_renderer_text_new (),
		"text", 1, NULL);
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
	info_pixmap = e_builder_get_widget (prefs->builder, "pixmapSpellInfo");
	gtk_image_set_from_stock (
		GTK_IMAGE (info_pixmap),
		GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_BUTTON);

	widget = e_builder_get_widget (prefs->builder, "colorButtonSpellCheckColor");
	g_settings_bind_with_mapping (
		settings, "composer-spell-color",
		widget, "color",
		G_SETTINGS_BIND_DEFAULT,
		composer_prefs_map_string_to_color,
		composer_prefs_map_color_to_string,
		NULL, (GDestroyNotify) NULL);

	spell_setup (prefs);

	/* Forwards and Replies */
	widget = e_builder_get_widget (prefs->builder, "comboboxForwardStyle");
	g_settings_bind (
		settings, "forward-style-name",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "comboboxReplyStyle");
	g_settings_bind (
		settings, "reply-style-name",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT);

	/* Signatures */
	container = e_builder_get_widget (
		prefs->builder, "signature-alignment");
	widget = e_mail_signature_manager_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* The mail shell backend responds to the "window-added" signal
	 * that this triggers and configures it with composer preferences. */
	g_signal_connect_swapped (
		widget, "editor-created",
		G_CALLBACK (gtk_application_add_window), shell);

	g_settings_bind (
		settings, "composer-send-html",
		widget, "prefer-html",
		G_SETTINGS_BIND_GET);

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *) ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	g_object_unref (settings);
}

GtkWidget *
em_composer_prefs_new (EPreferencesWindow *window)
{
	EShell *shell;
	EMComposerPrefs *prefs;

	shell = e_preferences_window_get_shell (window);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	prefs = g_object_new (EM_TYPE_COMPOSER_PREFS, NULL);
	em_composer_prefs_construct (prefs, shell);

	return GTK_WIDGET (prefs);
}
