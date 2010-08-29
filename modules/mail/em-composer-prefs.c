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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "e-util/e-binding.h"
#include "e-util/e-signature-utils.h"
#include "e-util/gconf-bridge.h"

#include "em-composer-prefs.h"
#include "composer/e-msg-composer.h"
#include "shell/e-shell-utils.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtkhtml/gtkhtml.h>
#include <editor/gtkhtml-spell-language.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "widgets/misc/e-charset-combo-box.h"
#include "widgets/misc/e-signature-editor.h"
#include "widgets/misc/e-signature-manager.h"
#include "widgets/misc/e-signature-preview.h"

#include "mail-config.h"
#include "em-config.h"
#include "em-folder-selection-button.h"

G_DEFINE_TYPE (
	EMComposerPrefs,
	em_composer_prefs,
	GTK_TYPE_VBOX)

static gboolean
transform_color_to_string (const GValue *src_value,
                           GValue *dst_value,
                           gpointer user_data)
{
	const GdkColor *color;
	gchar *string;

	color = g_value_get_boxed (src_value);
	string = gdk_color_to_string (color);
	g_value_set_string (dst_value, string);
	g_free (string);

	return TRUE;
}

static gboolean
transform_string_to_color (const GValue *src_value,
                           GValue *dst_value,
                           gpointer user_data)
{
	GdkColor color;
	const gchar *string;
	gboolean success = FALSE;

	string = g_value_get_string (src_value);
	if (gdk_color_parse (string, &color)) {
		g_value_set_boxed (dst_value, &color);
		success = TRUE;
	}

	return success;
}

static gboolean
transform_old_to_new_reply_style (const GValue *src_value,
                                  GValue *dst_value,
                                  gpointer user_data)
{
	gboolean success = TRUE;

	/* XXX This is the kind of legacy crap we wind up
	 *     with when we don't migrate things properly. */

	switch (g_value_get_int (src_value)) {
		case 0:  /* Quoted: 0 -> 2 */
			g_value_set_int (dst_value, 2);
			break;

		case 1:  /* Do Not Quote: 1 -> 3 */
			g_value_set_int (dst_value, 3);
			break;

		case 2:  /* Attach: 2 -> 0 */
			g_value_set_int (dst_value, 0);
			break;

		case 3:  /* Outlook: 3 -> 1 */
			g_value_set_int (dst_value, 1);
			break;

		default:
			success = FALSE;
			break;
	}

	return success;
}

static gboolean
transform_new_to_old_reply_style (const GValue *src_value,
                                  GValue *dst_value,
                                  gpointer user_data)
{
	gboolean success = TRUE;

	/* XXX This is the kind of legacy crap we wind up
	 *     with when we don't migrate things properly. */

	switch (g_value_get_int (src_value)) {
		case 0:  /* Attach: 0 -> 2 */
			g_value_set_int (dst_value, 2);
			break;

		case 1:  /* Outlook: 1 -> 3 */
			g_value_set_int (dst_value, 3);
			break;

		case 2:  /* Quoted: 2 -> 0 */
			g_value_set_int (dst_value, 0);
			break;

		case 3:  /* Do Not Quote: 3 -> 1 */
			g_value_set_int (dst_value, 1);
			break;

		default:
			success = FALSE;
			break;
	}

	return success;
}

static void
composer_prefs_finalize (GObject *object)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) object;

	g_object_unref (prefs->builder);

	/* Chain up to parent's finalize() method. */
        G_OBJECT_CLASS (em_composer_prefs_parent_class)->finalize (object);
}

static void
em_composer_prefs_class_init (EMComposerPrefsClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = composer_prefs_finalize;
}

static void
em_composer_prefs_init (EMComposerPrefs *prefs)
{
}

void
em_composer_prefs_new_signature (GtkWindow *parent,
                                 gboolean html_mode)
{
	GtkWidget *editor;

	editor = e_signature_editor_new ();
	gtkhtml_editor_set_html_mode (GTKHTML_EDITOR (editor), html_mode);
	gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	gtk_widget_show (editor);
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

	/* Update the GConf value. */
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
	  (gchar *) "vboxBehavior",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "00.general/10.alerts",
	  (gchar *) "vboxAlerts",
	  emcp_widget_glade },

	{ E_CONFIG_PAGE,
	  (gchar *) "10.signatures",
	  (gchar *) "vboxSignatures",
	  emcp_widget_glade },

	/* signature/signatures and signature/preview parts not usable */

	{ E_CONFIG_PAGE,
	  (gchar *) "20.spellcheck",
	  (gchar *) "vboxSpellChecking",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "20.spellcheck/00.languages",
	  (gchar *) "vbox178",
	  emcp_widget_glade },

	{ E_CONFIG_SECTION,
	  (gchar *) "20.spellcheck/00.options",
	  (gchar *) "vboxOptions",
	  emcp_widget_glade },
};

static void
emcp_free (EConfig *ec, GSList *items, gpointer data)
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
	EShellSettings *shell_settings;
	ESignatureList *signature_list;
	ESignatureTreeView *signature_tree_view;
	GtkTreeView *view;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GConfClient *client;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;
	gint i;

	client = mail_config_get_gconf_client ();
	shell_settings = e_shell_get_shell_settings (shell);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	EM_TYPE_FOLDER_SELECTION_BUTTON;

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	/** @HookPoint-EMConfig: Mail Composer Preferences
	 * @Id: org.gnome.evolution.mail.composerPrefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The mail composer preferences settings page.
	 */
	ec = em_config_new(E_CONFIG_BOOK, "org.gnome.evolution.mail.composerPrefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emcp_items); i++)
		l = g_slist_prepend (l, &emcp_items[i]);
	e_config_add_items ((EConfig *)ec, l, NULL, NULL, emcp_free, prefs);

	/* General tab */

	/* Default Behavior */

	/* Express mode does not honor this setting. */
	widget = e_builder_get_widget (prefs->builder, "chkSendHTML");
	if (e_shell_get_express_mode (shell))
		gtk_widget_hide (widget);
	else
		e_mutual_binding_new (
			shell_settings, "composer-format-html",
			widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptEmptySubject");
	e_mutual_binding_new (
		shell_settings, "composer-prompt-empty-subject",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptBccOnly");
	e_mutual_binding_new (
		shell_settings, "composer-prompt-only-bcc",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptPrivateListReply");
	e_mutual_binding_new (
		shell_settings, "composer-prompt-private-list-reply",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptReplyManyRecips");
	e_mutual_binding_new (
		shell_settings, "composer-prompt-reply-many-recips",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkPromptListReplyTo");
	e_mutual_binding_new (
		shell_settings, "composer-prompt-list-reply-to",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkAutoSmileys");
	e_mutual_binding_new (
		shell_settings, "composer-magic-smileys",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkRequestReceipt");
	e_mutual_binding_new (
		shell_settings, "composer-request-receipt",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkReplyStartBottom");
	e_mutual_binding_new (
		shell_settings, "composer-reply-start-bottom",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkOutlookFilenames");
	e_mutual_binding_new (
		shell_settings, "composer-outlook-filenames",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkIgnoreListReplyTo");
	e_mutual_binding_new (
		shell_settings, "composer-ignore-list-reply-to",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkGroupReplyToList");
	e_mutual_binding_new (
		shell_settings, "composer-group-reply-to-list",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkTopSignature");
	e_mutual_binding_new (
		shell_settings, "composer-top-signature",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "chkEnableSpellChecking");
	e_mutual_binding_new (
		shell_settings, "composer-inline-spelling",
		widget, "active");

	widget = e_charset_combo_box_new ();
	container = e_builder_get_widget (prefs->builder, "hboxComposerCharset");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	e_mutual_binding_new (
		shell_settings, "composer-charset",
		widget, "charset");

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
	e_mutual_binding_new_full (
		shell_settings, "composer-spell-color",
		widget, "color",
		transform_string_to_color,
		transform_color_to_string,
		NULL, NULL);

	spell_setup (prefs);

	/* Forwards and Replies */
	widget = e_builder_get_widget (prefs->builder, "comboboxForwardStyle");
	e_mutual_binding_new (
		shell_settings, "mail-forward-style",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "comboboxReplyStyle");
	e_mutual_binding_new_full (
		shell_settings, "mail-reply-style",
		widget, "active",
		transform_old_to_new_reply_style,
		transform_new_to_old_reply_style,
		NULL, NULL);

	/* Signatures */
	signature_list = e_get_signature_list ();
	container = e_builder_get_widget (prefs->builder, "alignSignatures");
	widget = e_signature_manager_new (signature_list);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* The mail shell backend responds to the "window-created" signal
	 * that this triggers and configures it with composer preferences. */
	g_signal_connect_swapped (
		widget, "editor-created",
		G_CALLBACK (e_shell_watch_window), shell);

	/* Express mode does not honor this setting. */
	if (!e_shell_get_express_mode (shell))
		e_binding_new (
			shell_settings, "composer-format-html",
			widget, "prefer-html");

#ifndef G_OS_WIN32
	e_binding_new_with_negation (
		shell_settings, "disable-command-line",
		widget, "allow-scripts");
#endif

	signature_tree_view = e_signature_manager_get_tree_view (
		E_SIGNATURE_MANAGER (widget));

	container = e_builder_get_widget (prefs->builder, "scrolled-sig");
	widget = e_signature_preview_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

#ifndef G_OS_WIN32
	e_binding_new_with_negation (
		shell_settings, "disable-command-line",
		widget, "allow-scripts");
#endif

	e_binding_new (
		signature_tree_view, "selected",
		widget, "signature");

	/* Sanitize the dialog for Express mode */
	e_shell_hide_widgets_for_express_mode (shell, prefs->builder,
					       "chkOutlookFilenames",
					       "vboxTopPosting",
					       "labelAlerts",
					       "chkPromptEmptySubject",
					       NULL);

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec, client);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
}

GtkWidget *
em_composer_prefs_new (EPreferencesWindow *window)
{
	EMComposerPrefs *prefs;
	EShell *shell = e_preferences_window_get_shell (window);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	prefs = g_object_new (EM_TYPE_COMPOSER_PREFS, NULL);
	em_composer_prefs_construct (prefs, shell);

	return GTK_WIDGET (prefs);
}
