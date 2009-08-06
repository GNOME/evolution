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

#include "e-util/e-signature.h"
#include "e-util/e-signature-list.h"
#include "e-util/gconf-bridge.h"

#include "em-composer-prefs.h"
#include "composer/e-msg-composer.h"

#include <bonobo/bonobo-generic-factory.h>

#include <camel/camel-iconv.h>

#include <misc/e-gui-utils.h>

#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <gtkhtml/gtkhtml.h>
#include <editor/gtkhtml-spell-language.h>

#include "misc/e-charset-picker.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include "mail-config.h"
#include "mail-signature-editor.h"
#include "em-config.h"

static gpointer parent_class;

static void
composer_prefs_dispose (GObject *object)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) object;
	ESignatureList *signature_list;

	signature_list = mail_config_get_signatures ();

	if (prefs->sig_added_id != 0) {
		g_signal_handler_disconnect (
			signature_list, prefs->sig_added_id);
		prefs->sig_added_id = 0;
	}

	if (prefs->sig_removed_id != 0) {
		g_signal_handler_disconnect (
			signature_list, prefs->sig_removed_id);
		prefs->sig_removed_id = 0;
	}

	if (prefs->sig_changed_id != 0) {
		g_signal_handler_disconnect (
			signature_list, prefs->sig_changed_id);
		prefs->sig_changed_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
composer_prefs_finalize (GObject *object)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) object;

	g_object_unref (prefs->gui);

	g_hash_table_destroy (prefs->sig_hash);

	/* Chain up to parent's finalize() method. */
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
composer_prefs_class_init (EMComposerPrefsClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = composer_prefs_dispose;
	object_class->finalize = composer_prefs_finalize;
}

static void
composer_prefs_init (EMComposerPrefs *prefs)
{
	prefs->sig_hash = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) gtk_tree_row_reference_free);
}

GType
em_composer_prefs_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMComposerPrefsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) composer_prefs_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMComposerPrefs),
			0,     /* n_allocs */
			(GInstanceInitFunc) composer_prefs_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_VBOX, "EMComposerPrefs", &type_info, 0);
	}

	return type;
}

static void
sig_load_preview (EMComposerPrefs *prefs,
                  ESignature *signature)
{
	GtkHTML *html;
	gchar *str;
	const gchar *filename;
	gboolean is_html;

	html = prefs->sig_preview;

	if (signature == NULL) {
		gtk_html_load_from_string (html, " ", 1);
		return;
	}

	filename = e_signature_get_filename (signature);
	is_html = e_signature_get_is_html (signature);

	if (e_signature_get_is_script (signature))
		str = mail_config_signature_run_script (filename);
	else
		str = e_msg_composer_get_sig_file_content (filename, is_html);
	if (!str || !*str) {
		/* make html stream happy and write at least one character */
		g_free (str);
		str = g_strdup (" ");
	}

	if (is_html)
		gtk_html_load_from_string (html, str, strlen (str));
	else {
		GtkHTMLStream *stream;
		gint len;

		len = strlen (str);
		stream = gtk_html_begin_content (html, "text/html; charset=utf-8");
		gtk_html_write (html, stream, "<PRE>", 5);
		if (len)
			gtk_html_write (html, stream, str, len);
		gtk_html_write (html, stream, "</PRE>", 6);
		gtk_html_end (html, stream, GTK_HTML_STREAM_OK);
	}

	g_free (str);
}

static void
signature_added (ESignatureList *signature_list,
                 ESignature *signature,
                 EMComposerPrefs *prefs)
{
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *name;

	/* autogen signature is special */
	if (e_signature_get_autogenerated (signature))
		return;

	name = e_signature_get_name (signature);

	model = gtk_tree_view_get_model (prefs->sig_list);
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		0, name, 1, signature, -1);

	path = gtk_tree_model_get_path (model, &iter);
	row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	g_hash_table_insert (prefs->sig_hash, signature, row);
}

static void
signature_removed (ESignatureList *signature_list,
                   ESignature *signature,
                   EMComposerPrefs *prefs)
{
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!(row = g_hash_table_lookup (prefs->sig_hash, signature)))
		return;

	model = gtk_tree_view_get_model (prefs->sig_list);
	path = gtk_tree_row_reference_get_path (row);
	g_hash_table_remove (prefs->sig_hash, signature);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_list_store_remove ((GtkListStore *) model, &iter);
}

static void
signature_changed (ESignatureList *signature_list,
                   ESignature *signature,
                   EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	ESignature *cur;
	const gchar *name;

	if (!(row = g_hash_table_lookup (prefs->sig_hash, signature)))
		return;

	model = gtk_tree_view_get_model (prefs->sig_list);
	path = gtk_tree_row_reference_get_path (row);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_path_free (path);

	name = e_signature_get_name (signature);
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, name, -1);

	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 1, &cur, -1);
		if (cur == signature)
			sig_load_preview (prefs, signature);
	}
}

static void
sig_edit_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkWidget *parent;
	GtkTreeIter iter;
	ESignature *signature;
	const gchar *filename;
	const gchar *name;

	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, 1, &signature, -1);

	filename = e_signature_get_filename (signature);
	name = e_signature_get_name (signature);

	if (!e_signature_get_is_script (signature)) {
		GtkWidget *editor;

		filename = e_signature_get_filename (signature);

		/* normal signature */
		if (filename == NULL || *filename == '\0') {
			e_signature_set_filename (signature, _("Unnamed"));
			filename = e_signature_get_filename (signature);
		}

		editor = e_signature_editor_new ();
		e_signature_editor_set_signature (
			E_SIGNATURE_EDITOR (editor), signature);

		parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
		if (GTK_WIDGET_TOPLEVEL (parent))
			gtk_window_set_transient_for (
				GTK_WINDOW (editor), GTK_WINDOW (parent));

		gtk_widget_show (editor);
	} else {
		/* signature script */
		GtkWidget *entry;

		entry = glade_xml_get_widget (prefs->sig_script_gui, "filechooserbutton_add_script");
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (entry), filename);

		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		gtk_entry_set_text (GTK_ENTRY (entry), name);

		g_object_set_data ((GObject *) entry, "sig", signature);

		gtk_window_present ((GtkWindow *) prefs->sig_script_dialog);
	}
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
sig_delete_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESignature *signature;

	selection = gtk_tree_view_get_selection (prefs->sig_list);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 1, &signature, -1);
		mail_config_remove_signature (signature);
	}
	gtk_widget_grab_focus ((GtkWidget *)prefs->sig_list);
}

static void
sig_add_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	gboolean send_html;
	GtkWidget *parent;

	send_html = gconf_client_get_bool (
		mail_config_get_gconf_client (),
		"/apps/evolution/mail/composer/send_html", NULL);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (prefs));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	em_composer_prefs_new_signature (GTK_WINDOW (parent), send_html);
	gtk_widget_grab_focus (GTK_WIDGET (prefs->sig_list));
}

static void
sig_add_script_response (GtkWidget *widget, gint button, EMComposerPrefs *prefs)
{
	gchar *script, **argv = NULL;
	GtkWidget *entry;
	const gchar *name;
	gint argc;

	if (button == GTK_RESPONSE_ACCEPT) {
		entry = glade_xml_get_widget (prefs->sig_script_gui, "filechooserbutton_add_script");
		script = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (entry));

		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		name = gtk_entry_get_text (GTK_ENTRY (entry));
		if (script && *script && g_shell_parse_argv (script, &argc, &argv, NULL)) {
			struct stat st;

			if (g_stat (argv[0], &st) == 0 && S_ISREG (st.st_mode) && g_access (argv[0], X_OK) == 0) {
				ESignature *signature;

				if ((signature = g_object_get_data ((GObject *) entry, "sig"))) {
					/* we're just editing an existing signature script */
					e_signature_set_name (signature, name);
					e_signature_set_filename (signature, script);
					e_signature_list_change (mail_config_get_signatures (), signature);
				} else {
					signature = mail_config_signature_new (script, TRUE, TRUE);
					e_signature_set_name (signature, name);

					e_signature_list_add (mail_config_get_signatures (), signature);
					g_object_unref (signature);
				}

				mail_config_save_signatures();

				gtk_widget_hide (prefs->sig_script_dialog);
				g_strfreev (argv);
				g_free (script);

				return;
			}
		}

		e_error_run((GtkWindow *)prefs->sig_script_dialog, "mail:signature-notscript", argv ? argv[0] : script, NULL);
		g_strfreev (argv);
		g_free (script);
		return;
	}

	gtk_widget_hide (widget);
}

static void
sig_add_script_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkWidget *entry;

	entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
	gtk_entry_set_text (GTK_ENTRY (entry), _("Unnamed"));

	g_object_set_data ((GObject *) entry, "sig", NULL);

	gtk_window_present ((GtkWindow *) prefs->sig_script_dialog);
}

static void
sig_selection_changed (GtkTreeSelection *selection,
                       EMComposerPrefs *prefs)
{
	ESignature *signature;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_selection_get_selected (selection, &model, &iter);

	if (valid) {
		gtk_tree_model_get (model, &iter, 1, &signature, -1);
		sig_load_preview (prefs, signature);
	} else
		sig_load_preview (prefs, NULL);

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->sig_delete), valid);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->sig_edit), valid);
}

static void
sig_fill_list (EMComposerPrefs *prefs)
{
	ESignatureList *signature_list;
	GtkTreeModel *model;
	EIterator *iterator;

	model = gtk_tree_view_get_model (prefs->sig_list);
	gtk_list_store_clear (GTK_LIST_STORE (model));

	signature_list = mail_config_get_signatures ();
	iterator = e_list_get_iterator ((EList *) signature_list);

	while (e_iterator_is_valid (iterator)) {
		ESignature *signature;

		signature = (ESignature *) e_iterator_get (iterator);
		signature_added (signature_list, signature, prefs);

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->sig_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->sig_delete), FALSE);

	prefs->sig_added_id = g_signal_connect (
		signature_list, "signature-added",
		G_CALLBACK (signature_added), prefs);

	prefs->sig_removed_id = g_signal_connect (
		signature_list, "signature-removed",
		G_CALLBACK (signature_removed), prefs);

	prefs->sig_changed_id = g_signal_connect (
		signature_list, "signature-changed",
		G_CALLBACK (signature_changed), prefs);
}

static void
url_requested (GtkHTML *html,
               const gchar *url,
               GtkHTMLStream *handle)
{
	GtkHTMLStreamStatus status;
	gchar buf[128];
	gssize size;
	gint fd;
	gchar *filename;

	if (strncmp (url, "file:", 5) == 0)
		filename = g_filename_from_uri (url, NULL, NULL);
	else
		filename = g_strdup (url);
	fd = g_open (filename, O_RDONLY | O_BINARY, 0);
	g_free (filename);

	status = GTK_HTML_STREAM_OK;
	if (fd != -1) {
		while ((size = read (fd, buf, sizeof (buf)))) {
			if (size == -1) {
				status = GTK_HTML_STREAM_ERROR;
				break;
			} else
				gtk_html_write (html, handle, buf, size);
		}
	} else
		status = GTK_HTML_STREAM_ERROR;

	gtk_html_end (html, handle, status);
	if (fd > 0)
		close (fd);
}

static void
spell_color_set (GtkColorButton *color_button,
                 EMComposerPrefs *prefs)
{
	GConfClient *client;
	const gchar *key;
	GdkColor color;
	gchar *string;

	gtk_color_button_get_color (color_button, &color);
	string = gdk_color_to_string (&color);

	client = mail_config_get_gconf_client ();
	key = "/apps/evolution/mail/composer/spell_color";
	gconf_client_set_string (client, key, string, NULL);

	g_free (string);
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
	GConfClient *client;
	GtkListStore *store;
	GdkColor color;
	const gchar *key;
	gchar *string;

	client = mail_config_get_gconf_client ();
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

	key = "/apps/evolution/mail/composer/spell_color";
	string = gconf_client_get_string (client, key, NULL);
	if (string == NULL || !gdk_color_parse (string, &color))
		gdk_color_parse ("Red", &color);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (prefs->color), &color);

	g_signal_connect (
		prefs->color, "color_set",
		G_CALLBACK (spell_color_set), prefs);
}

static gint
reply_style_new_order (gint style_id,
                              gboolean from_enum_to_option_id)
{
	gint values[] = {
		MAIL_CONFIG_REPLY_ATTACH, 0,
		MAIL_CONFIG_REPLY_OUTLOOK, 1,
		MAIL_CONFIG_REPLY_QUOTED, 2,
		MAIL_CONFIG_REPLY_DO_NOT_QUOTE, 3,
		-1, -1};
	gint ii;

	for (ii = from_enum_to_option_id ? 0 : 1; values[ii] != -1; ii += 2) {
		if (values[ii] == style_id)
			return values [from_enum_to_option_id ? ii + 1 : ii - 1];
	}

	return style_id;
}

static void
style_changed (GtkComboBox *combobox, const gchar *key)
{
	GConfClient *client;
	gint style;

	client = mail_config_get_gconf_client ();
	style = gtk_combo_box_get_active (combobox);
	g_return_if_fail (style >= 0);

	if (g_str_has_suffix (key, "/reply_style"))
		style = reply_style_new_order (style, FALSE);

	gconf_client_set_int (client, key, style, NULL);
}

static void
charset_activate (GtkWidget *item,
                  EMComposerPrefs *prefs)
{
	GConfClient *client;
	GtkWidget *menu;
	gchar *string;

	client = mail_config_get_gconf_client ();
	menu = gtk_option_menu_get_menu (prefs->charset);
	string = e_charset_picker_get_charset (menu);

	if (string == NULL)
		string = g_strdup (camel_iconv_locale_charset ());

	gconf_client_set_string (
		client, "/apps/evolution/mail/composer/charset",
		string, NULL);

	g_free (string);
}

static void
option_menu_connect (EMComposerPrefs *prefs,
                     GtkOptionMenu *omenu,
                     GCallback callback,
                     const gchar *key)
{
	GConfClient *client;
	GtkWidget *menu;
	GList *list;

	client = mail_config_get_gconf_client ();
	menu = gtk_option_menu_get_menu (omenu);
	list = GTK_MENU_SHELL (menu)->children;

	while (list != NULL) {
		GtkWidget *widget = list->data;

		g_object_set_data (G_OBJECT (widget), "key", (gpointer) key);
		g_signal_connect (widget, "activate", callback, prefs);
		list = list->next;
	}

	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (GTK_WIDGET (omenu), FALSE);
}

static GtkWidget *
emcp_widget_glade (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gpointer data)
{
	EMComposerPrefs *prefs = data;

	return glade_xml_get_widget (prefs->gui, item->label);
}

/* plugin meta-data */
static EMConfigItem emcp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "composer_toplevel", emcp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "vboxGeneral", emcp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.behavior", (gchar *) "vboxBehavior", emcp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/10.alerts", (gchar *) "vboxAlerts", emcp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "10.signatures", (gchar *) "vboxSignatures", emcp_widget_glade },
	/* signature/signatures and signature/preview parts not usable */

	{ E_CONFIG_PAGE, (gchar *) "20.spellcheck", (gchar *) "vboxSpellChecking", emcp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "20.spellcheck/00.languages", (gchar *) "vbox178", emcp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "20.spellcheck/00.options", (gchar *) "vboxOptions", emcp_widget_glade },
};

static void
emcp_free (EConfig *ec, GSList *items, gpointer data)
{
	/* the prefs data is freed automagically */
	g_slist_free (items);
}

static gboolean
signature_key_press_cb (GtkTreeView *tree_view,
                        GdkEventKey *event,
                        EMComposerPrefs *prefs)
{
	/* No need to care about anything other than DEL key */
	if (event->keyval == GDK_Delete) {
		sig_delete_cb (GTK_WIDGET (tree_view), prefs);
		return TRUE;
	}

	return FALSE;
}

static gboolean
sig_tree_event_cb (GtkTreeView *tree_view,
                   GdkEvent *event,
                   EMComposerPrefs *prefs)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		sig_edit_cb (GTK_WIDGET (tree_view), prefs);
		return TRUE;
	}

	return FALSE;
}

static void
em_composer_prefs_construct (EMComposerPrefs *prefs)
{
	GtkWidget *toplevel, *widget, *menu, *info_pixmap;
	GtkDialog *dialog;
	GladeXML *gui;
	GtkTreeView *view;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GConfBridge *bridge;
	GConfClient *client;
	const gchar *key;
	gint style;
	gchar *buf;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	GSList *l;
	gint i;
	gchar *gladefile;
	gboolean sensitive;

	bridge = gconf_bridge_get ();
	client = mail_config_get_gconf_client ();

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "composer_toplevel", NULL);
	prefs->gui = gui;
	prefs->sig_script_gui = glade_xml_new (gladefile, "vbox_add_script_signature", NULL);
	g_free (gladefile);

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
		l = g_slist_prepend(l, &emcp_items[i]);
	e_config_add_items((EConfig *)ec, l, NULL, NULL, emcp_free, prefs);

	/* General tab */

	/* Default Behavior */
	key = "/apps/evolution/mail/composer/send_html";
	widget = glade_xml_get_widget (gui, "chkSendHTML");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/prompts/empty_subject";
	widget = glade_xml_get_widget (gui, "chkPromptEmptySubject");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/prompts/only_bcc";
	widget = glade_xml_get_widget (gui, "chkPromptBccOnly");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/magic_smileys";
	widget = glade_xml_get_widget (gui, "chkAutoSmileys");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/request_receipt";
	widget = glade_xml_get_widget (gui, "chkRequestReceipt");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/reply_start_bottom";
	widget = glade_xml_get_widget (gui, "chkReplyStartBottom");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/outlook_filenames";
	widget = glade_xml_get_widget (gui, "chkOutlookFilenames");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/top_signature";
	widget = glade_xml_get_widget (gui, "chkTopSignature");
	if (!gconf_client_key_is_writable (client, key, NULL))
		gtk_widget_set_sensitive (widget, FALSE);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	key = "/apps/evolution/mail/composer/inline_spelling";
	widget = glade_xml_get_widget (gui, "chkEnableSpellChecking");
	gconf_bridge_bind_property (bridge, key, G_OBJECT (widget), "active");

	prefs->charset = GTK_OPTION_MENU (
		glade_xml_get_widget (gui, "omenuCharset1"));
	buf = gconf_client_get_string (
		client, "/apps/evolution/mail/composer/charset", NULL);
	menu = e_charset_picker_new (
		buf && *buf ? buf : camel_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (
		prefs, prefs->charset,
		G_CALLBACK (charset_activate),
		"/apps/evolution/mail/composer/charset");
	g_free (buf);

	/* Spell Checking */
	widget = glade_xml_get_widget (gui, "colorButtonSpellCheckColor");
	prefs->color = GTK_COLOR_BUTTON (widget);
	widget = glade_xml_get_widget (gui, "listSpellCheckLanguage");
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
	info_pixmap = glade_xml_get_widget (gui, "pixmapSpellInfo");
	gtk_image_set_from_stock (
		GTK_IMAGE (info_pixmap),
		GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_BUTTON);
	spell_setup (prefs);

	/* Forwards and Replies */
	prefs->forward_style = GTK_COMBO_BOX (glade_xml_get_widget (gui, "comboboxForwardStyle"));
	style = gconf_client_get_int (client, "/apps/evolution/mail/format/forward_style", NULL);
	gtk_combo_box_set_active (prefs->forward_style, style);
	g_signal_connect (prefs->forward_style, "changed", G_CALLBACK (style_changed), (gpointer) "/apps/evolution/mail/format/forward_style");

	prefs->reply_style = GTK_COMBO_BOX (glade_xml_get_widget (gui, "comboboxReplyStyle"));
	style = gconf_client_get_int (client, "/apps/evolution/mail/format/reply_style", NULL);
	gtk_combo_box_set_active (prefs->reply_style, reply_style_new_order (style, TRUE));
	g_signal_connect (prefs->reply_style, "changed", G_CALLBACK (style_changed), (gpointer) "/apps/evolution/mail/format/reply_style");

	/* Signatures */
	dialog = (GtkDialog *) gtk_dialog_new ();

	gtk_widget_realize ((GtkWidget *) dialog);
	gtk_container_set_border_width ((GtkContainer *)dialog->action_area, 12);
	gtk_container_set_border_width ((GtkContainer *)dialog->vbox, 0);

	prefs->sig_script_dialog = (GtkWidget *) dialog;
	gtk_dialog_add_buttons (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_has_separator (dialog, FALSE);
	gtk_window_set_title ((GtkWindow *) dialog, _("Add signature script"));
	g_signal_connect (dialog, "response", G_CALLBACK (sig_add_script_response), prefs);
	widget = glade_xml_get_widget (prefs->sig_script_gui, "vbox_add_script_signature");
	gtk_box_pack_start ((GtkBox *) dialog->vbox, widget, TRUE, TRUE, 0);

	key = "/apps/evolution/mail/signatures";
	sensitive = gconf_client_key_is_writable (client, key, NULL);

	widget = glade_xml_get_widget (gui, "cmdSignatureAdd");
	gtk_widget_set_sensitive (widget, sensitive);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sig_add_cb), prefs);
	prefs->sig_add = GTK_BUTTON (widget);

	widget = glade_xml_get_widget (gui, "cmdSignatureAddScript");
	gtk_widget_set_sensitive (widget, sensitive && !mail_config_scripts_disabled ());
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sig_add_script_cb), prefs);
	prefs->sig_add_script = GTK_BUTTON (widget);

	widget = glade_xml_get_widget (gui, "cmdSignatureEdit");
	gtk_widget_set_sensitive (widget, sensitive);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sig_edit_cb), prefs);
	prefs->sig_edit = GTK_BUTTON (widget);

	widget = glade_xml_get_widget (gui, "cmdSignatureDelete");
	gtk_widget_set_sensitive (widget, sensitive);
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (sig_delete_cb), prefs);
	prefs->sig_delete = GTK_BUTTON (widget);

	widget = glade_xml_get_widget (gui, "listSignatures");
	gtk_widget_set_sensitive (widget, sensitive);
	prefs->sig_list = GTK_TREE_VIEW (widget);
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_view_set_model (prefs->sig_list, GTK_TREE_MODEL (store));
	gtk_tree_view_insert_column_with_attributes (
		prefs->sig_list, -1, _("Signature(s)"),
		gtk_cell_renderer_text_new (), "text", 0, NULL);
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (sig_selection_changed), prefs);
	g_signal_connect (
		prefs->sig_list, "event",
		G_CALLBACK (sig_tree_event_cb), prefs);

	sig_fill_list (prefs);

	/* preview GtkHTML widget */
	widget = glade_xml_get_widget (gui, "scrolled-sig");
	prefs->sig_preview = (GtkHTML *) gtk_html_new ();
	g_signal_connect (
		prefs->sig_preview, "url_requested",
		G_CALLBACK (url_requested), NULL);
	gtk_widget_show (GTK_WIDGET (prefs->sig_preview));
	gtk_container_add (
		GTK_CONTAINER (widget),
		GTK_WIDGET (prefs->sig_preview));

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec, client);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	g_signal_connect (
		prefs->sig_list, "key-press-event",
		G_CALLBACK (signature_key_press_cb), prefs);
}

GtkWidget *
em_composer_prefs_new (void)
{
	EMComposerPrefs *prefs;

	prefs = g_object_new (EM_TYPE_COMPOSER_PREFS, NULL);
	em_composer_prefs_construct (prefs);

	return GTK_WIDGET (prefs);
}
