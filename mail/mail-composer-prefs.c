/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "mail-composer-prefs.h"
#include "composer/e-msg-composer.h"

#include <bonobo/bonobo-generic-factory.h>

#include <gal/util/e-iconv.h>
#include <gal/widgets/e-gui-utils.h>

#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>

#include "widgets/misc/e-charset-picker.h"

#include "mail-config.h"

#include "art/mark.xpm"


#define d(x)

static void mail_composer_prefs_class_init (MailComposerPrefsClass *class);
static void mail_composer_prefs_init       (MailComposerPrefs *dialog);
static void mail_composer_prefs_destroy    (GtkObject *obj);
static void mail_composer_prefs_finalise   (GObject *obj);

static void sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs);

static GtkVBoxClass *parent_class = NULL;


GType
mail_composer_prefs_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (MailComposerPrefsClass),
			NULL, NULL,
			(GClassInitFunc) mail_composer_prefs_class_init,
			NULL, NULL,
			sizeof (MailComposerPrefs),
			0,
			(GInstanceInitFunc) mail_composer_prefs_init,
		};
		
		type = g_type_register_static (gtk_vbox_get_type (), "MailComposerPrefs", &info, 0);
	}
	
	return type;
}

static void
mail_composer_prefs_class_init (MailComposerPrefsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	
	object_class->destroy = mail_composer_prefs_destroy;
	gobject_class->finalize = mail_composer_prefs_finalise;
}

static void
mail_composer_prefs_init (MailComposerPrefs *composer_prefs)
{
	composer_prefs->enabled_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) mark_xpm);
	gdk_pixbuf_render_pixmap_and_mask (composer_prefs->enabled_pixbuf,
					   &composer_prefs->mark_pixmap, &composer_prefs->mark_bitmap, 128);
}

static void
mail_composer_prefs_finalise (GObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;
	
	g_object_unref (prefs->gui);
	g_object_unref (prefs->pman);
	g_object_unref (prefs->enabled_pixbuf);
	gdk_pixmap_unref (prefs->mark_pixmap);
	g_object_unref (prefs->mark_bitmap);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
mail_composer_prefs_destroy (GtkObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;

	mail_config_signature_unregister_client ((MailConfigSignatureClient) sig_event_client, prefs);
	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
attach_style_info (GtkWidget *item, gpointer user_data)
{
	int *style = user_data;
	
	g_object_set_data ((GObject *) item, "style", GINT_TO_POINTER (*style));
	
	(*style)++;
}

static void
toggle_button_toggled (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
menu_changed (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
option_menu_connect (GtkOptionMenu *omenu, gpointer user_data)
{
	GtkWidget *menu, *item;
	GList *items;
	
	menu = gtk_option_menu_get_menu (omenu);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		g_signal_connect (item, "activate", G_CALLBACK (menu_changed), user_data);
		items = items->next;
	}
}

static void
sig_load_preview (MailComposerPrefs *prefs, MailConfigSignature *sig)
{
	char *str;
	
	if (!sig) {
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), " ", 1);
		return;
	}
	
	if (sig->script)
		str = mail_config_signature_run_script (sig->script);
	else
		str = e_msg_composer_get_sig_file_content (sig->filename, sig->html);
	if (!str)
		str = g_strdup ("");
	
	/* printf ("HTML: %s\n", str); */
	if (sig->html) {
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), str, strlen (str));
	} else {
		GtkHTMLStream *stream;
		int len;
		
		len = strlen (str);
		stream = gtk_html_begin_content (GTK_HTML (prefs->sig_preview), "text/html; charset=utf-8");
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "<PRE>", 5);
		if (len)
			gtk_html_write (GTK_HTML (prefs->sig_preview), stream, str, len);
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "</PRE>", 6);
		gtk_html_end (GTK_HTML (prefs->sig_preview), stream, GTK_HTML_STREAM_OK);
	}
	
	g_free (str);
}

static void
sig_edit_cb (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	MailConfigSignature *sig;
	GtkTreeModel *model;
	GtkWidget *parent;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, 1, &sig, -1);
	
	if (sig->script == NULL) {
		/* normal signature */
		if (!sig->filename || *sig->filename == '\0') {
			g_free (sig->filename);
			sig->filename = g_strdup (_("Unnamed"));
		}
		
		parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
		parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
		
		mail_signature_editor (sig, (GtkWindow *) parent, FALSE);
	} else {
		/* signature script */
		GtkWidget *entry;
		
		entry = glade_xml_get_widget (prefs->sig_script_gui, "fileentry_add_script_script");
		gtk_entry_set_text (GTK_ENTRY (entry), sig->name);
		
		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		gtk_entry_set_text (GTK_ENTRY (entry), sig->name);
		
		g_object_set_data ((GObject *) entry, "script", sig);
		
		gtk_widget_show (prefs->sig_script_dialog);
		gdk_window_raise (prefs->sig_script_dialog->window);
	}
}

MailConfigSignature *
mail_composer_prefs_new_signature (GtkWindow *parent, gboolean html, const char *script)
{
	MailConfigSignature *sig;
	
	sig = mail_config_signature_new (html, script);
	mail_signature_editor (sig, parent, TRUE);
	
	return sig;
}

static void
sig_delete_cb (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 1, &sig, -1);
		gtk_list_store_remove ((GtkListStore *) model, &iter);
		mail_config_signature_delete (sig);
	}
}

static void
sig_add_cb (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GConfClient *gconf;
	gboolean send_html;
	GtkWidget *parent;
	
	gconf = gconf_client_get_default ();
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	
	parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
	
	mail_composer_prefs_new_signature ((GtkWindow *) parent, send_html, NULL);
}

static void
sig_add_script_response (GtkWidget *widget, int button, MailComposerPrefs *prefs)
{
	const char *script, *name;
	GtkWidget *dialog;
	GtkWidget *entry;
	
	if (button == GTK_RESPONSE_ACCEPT) {
		entry = glade_xml_get_widget (prefs->sig_script_gui, "fileentry_add_script_script");
		script = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (entry))));
		
		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		name = gtk_entry_get_text (GTK_ENTRY (entry));
		if (script && *script) {
			struct stat st;
			
			if (!stat (script, &st) && S_ISREG (st.st_mode) && access (script, X_OK) == 0) {
				MailConfigSignature *sig;
				GtkWidget *parent;
				
				parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
				parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
				
				if ((sig = g_object_get_data ((GObject *) entry, "script"))) {
					/* we're just editing an existing signature script */
					mail_config_signature_set_name (sig, name);
				} else {
					sig = mail_composer_prefs_new_signature ((GtkWindow *) parent, TRUE, script);
					mail_config_signature_set_name (sig, name);
					mail_config_signature_add (sig);
				}
				
				gtk_widget_hide (prefs->sig_script_dialog);
				
				return;
			}
		}
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (prefs->sig_script_dialog),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("You must specify a valid script name."));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
	}
	
	gtk_widget_hide (widget);
}

static void
sig_add_script_cb (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GtkWidget *entry;
	
	entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
	gtk_entry_set_text (GTK_ENTRY (entry), _("Unnamed"));
	
	g_object_set_data ((GObject *) entry, "script", NULL);
	
	gtk_widget_show (prefs->sig_script_dialog);
	gdk_window_raise (prefs->sig_script_dialog->window);
}

static void
sig_selection_changed (GtkTreeSelection *selection, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig;
	GtkTreeModel *model;
	GtkTreeIter iter;
	int state;
	
	state = gtk_tree_selection_get_selected (selection, &model, &iter);
	if (state) {
		gtk_tree_model_get (model, &iter, 1, &sig, -1);
		sig_load_preview (prefs, sig);
	}
	
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, state);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, state);
}

static void
sig_fill_clist (GtkTreeView *clist)
{
	GSList *l;
	GtkListStore *model;
	GtkTreeIter iter;
	
	model = (GtkListStore *) gtk_tree_view_get_model (clist);
	gtk_list_store_clear (model);
	
	for (l = mail_config_get_signature_list (); l; l = l->next) {
		MailConfigSignature *sig = l->data;
		char *name = NULL, *val;
		
		gtk_list_store_append (model, &iter);
		
		if (sig->script)
			name = val = g_strconcat (sig->name, " ", _("[script]"), NULL);
		else
			val = sig->name;
		gtk_list_store_set (model, &iter, 0, val, 1, sig, -1);
		g_free (name);
	}
}

static void
url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle)
{
	GtkHTMLStreamStatus status;
	char buf[128];
	ssize_t size;
	int fd;
	
	if (!strncmp (url, "file:", 5))
		url += 5;
	
	fd = open (url, O_RDONLY);
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
}

static void
sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs)
{
	MailConfigSignature *current;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char path[16];
	
	switch (event) {
	case MAIL_CONFIG_SIG_EVENT_ADDED:
		d(printf ("signature ADDED\n"));
		
		model = gtk_tree_view_get_model (prefs->sig_list);
		gtk_list_store_append ((GtkListStore *) model, &iter);
		gtk_list_store_set ((GtkListStore *) model, &iter, 0, sig->name, 1, sig, -1);
		break;
	case MAIL_CONFIG_SIG_EVENT_NAME_CHANGED:
		d(printf ("signature NAME CHANGED\n"));
		
		/* this is one bizarro interface */
		model = gtk_tree_view_get_model (prefs->sig_list);
		sprintf (path, "%d", sig->id);
		if (gtk_tree_model_get_iter_from_string (model, &iter, path)) {
			char *val, *name = NULL;
			
			if (sig->script)
				name = val = g_strconcat (sig->name, " ", _("[script]"), NULL);
			else
				val = sig->name;			
			
			gtk_list_store_set ((GtkListStore *) model, &iter, 0, val, -1);
			g_free (name);
		}
		break;
	case MAIL_CONFIG_SIG_EVENT_CONTENT_CHANGED:
		d(printf ("signature CONTENT CHANGED\n"));
		selection = gtk_tree_view_get_selection (prefs->sig_list);
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			gtk_tree_model_get (model, &iter, 1, &current, -1);
			if (sig == current)
				sig_load_preview (prefs, sig);
		}
		break;
	default:
		;
	}
}

/*
 *
 * Spell checking cut'n'pasted from gnome-spell/capplet/main.c
 *
 */

#include "Spell.h"

#define GNOME_SPELL_GCONF_DIR "/GNOME/Spell"
#define SPELL_API_VERSION "0.3"

static void
spell_set_ui (MailComposerPrefs *prefs)
{
	GtkListStore *model;
	GtkTreeIter iter;
	GHashTable *present;
	gboolean go;
	char **strv = NULL;
	int i;

	prefs->spell_active = FALSE;

	/* setup the language list */
	present = g_hash_table_new (g_str_hash, g_str_equal);
	if (prefs->language_str && (strv = g_strsplit (prefs->language_str, " ", 0))) {
		for (i = 0; strv[i]; i++)
			g_hash_table_insert (present, strv[i], strv[i]);
	}
	
	model = (GtkListStore *) gtk_tree_view_get_model (prefs->language);
	for (go = gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter); go;
	     go = gtk_tree_model_iter_next ((GtkTreeModel *) model, &iter)) {
		char *abbr;
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 2, &abbr, -1);
		gtk_list_store_set (model, &iter, 0, g_hash_table_lookup (present, abbr) != NULL, -1);
	}
	
	g_hash_table_destroy (present);
	if (strv != NULL)
		g_strfreev (strv);
	
	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (prefs->colour),
				    prefs->spell_error_color.red,
				    prefs->spell_error_color.green,
				    prefs->spell_error_color.blue, 0xffff);
	
	prefs->spell_active = TRUE;
}

static gchar *
spell_get_language_str (MailComposerPrefs *prefs)
{
	GString *str = g_string_new ("");
	GtkListStore *model;
	GtkTreeIter iter;
	gboolean go;
	char *rv;

	model = (GtkListStore *) gtk_tree_view_get_model (prefs->language);
	for (go = gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter);
	     go;
	     go = gtk_tree_model_iter_next ((GtkTreeModel *) model, &iter)) {
		char *abbr;
		gboolean state;
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 0, &state, 2, &abbr, -1);
		if (state) {
			if (str->len)
				g_string_append_c (str, ' ');
			g_string_append (str, abbr);
		}
	}
	
	rv = str->str;
	g_string_free (str, FALSE);
	
	return rv;
}

static void
spell_get_ui (MailComposerPrefs *prefs)
{
	gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (prefs->colour),
				    &prefs->spell_error_color.red,
				    &prefs->spell_error_color.green,
				    &prefs->spell_error_color.blue, NULL);
	g_free (prefs->language_str);
	prefs->language_str = spell_get_language_str (prefs);
}

#define GET(t,x,prop,f,c) \
        val = gconf_client_get_without_default (prefs->gconf, GNOME_SPELL_GCONF_DIR x, NULL); \
        if (val) { f; prop = c (gconf_value_get_ ## t (val)); \
        gconf_value_free (val); }

static void
spell_save_orig (MailComposerPrefs *prefs)
{
	g_free (prefs->language_str_orig);
	prefs->language_str_orig = g_strdup (prefs->language_str ? prefs->language_str : "");
	prefs->spell_error_color_orig = prefs->spell_error_color;
}

/* static void
spell_load_orig (MailComposerPrefs *prefs)
{
	g_free (prefs->language_str);
	prefs->language_str = g_strdup (prefs->language_str_orig);
	prefs->spell_error_color = prefs->spell_error_color_orig;
} */

static void
spell_load_values (MailComposerPrefs *prefs)
{
	GConfValue *val;
	char *def_lang;

	def_lang = g_strdup (e_iconv_locale_language ());
	g_free (prefs->language_str);
	prefs->language_str = g_strdup (def_lang);
	prefs->spell_error_color.red   = 0xffff;
	prefs->spell_error_color.green = 0;
	prefs->spell_error_color.blue  = 0;
	
	GET (int, "/spell_error_color_red",   prefs->spell_error_color.red, (void)0, (int));
	GET (int, "/spell_error_color_green", prefs->spell_error_color.green, (void)0, (int));
	GET (int, "/spell_error_color_blue",  prefs->spell_error_color.blue, (void)0, (int));
	GET (string, "/language", prefs->language_str, g_free (prefs->language_str), g_strdup);
	
	if (prefs->language_str == NULL)
		prefs->language_str = g_strdup (def_lang);
	
	spell_save_orig (prefs);
	
	g_free (def_lang);
}

#define SET(t,x,prop) \
        gconf_client_set_ ## t (prefs->gconf, GNOME_SPELL_GCONF_DIR x, prop, NULL);

#define STR_EQUAL(str1, str2) ((str1 == NULL && str2 == NULL) || (str1 && str2 && !strcmp (str1, str2)))

static void
spell_save_values (MailComposerPrefs *prefs, gboolean force)
{
	if (force || !gdk_color_equal (&prefs->spell_error_color, &prefs->spell_error_color_orig)) {
		SET (int, "/spell_error_color_red",   prefs->spell_error_color.red);
		SET (int, "/spell_error_color_green", prefs->spell_error_color.green);
		SET (int, "/spell_error_color_blue",  prefs->spell_error_color.blue);
	}
	
	if (force || !STR_EQUAL (prefs->language_str, prefs->language_str_orig)) {
		SET (string, "/language", prefs->language_str ? prefs->language_str : "");
	}
	
	gconf_client_suggest_sync (prefs->gconf, NULL);
}

static void
spell_apply (MailComposerPrefs *prefs)
{
	spell_get_ui (prefs);
	spell_save_values (prefs, FALSE);
}

/* static void
spell_revert (MailComposerPrefs *prefs)
{
	spell_load_orig (prefs);
	spell_set_ui (prefs);
	spell_save_values (prefs, TRUE);
} */

static void
spell_changed (gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
spell_color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, gpointer user_data)
{
	spell_changed (user_data);
}

static void
spell_language_selection_changed (GtkTreeSelection *selection, MailComposerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean state = FALSE;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 0, &state, -1);
		gtk_button_set_label ((GtkButton *) prefs->spell_able_button, state ? _("Disable") : _("Enable"));
		state = TRUE;
	}
	gtk_widget_set_sensitive (prefs->spell_able_button, state);
}

static void
spell_language_enable (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean state;
	
	selection = gtk_tree_view_get_selection (prefs->language);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 0, &state, -1);
		gtk_list_store_set ((GtkListStore *) model, &iter, 0, !state, -1);
		gtk_button_set_label ((GtkButton *) prefs->spell_able_button, state ? _("Enable") : _("Disable"));
		spell_changed (prefs);
	}
}

static gboolean
spell_language_button_press (GtkTreeView *tv, GdkEventButton *event, MailComposerPrefs *prefs)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *column = NULL;
	gtk_tree_view_get_path_at_pos (tv, event->x, event->y, &path, &column, NULL, NULL);

	/* FIXME: This routine should just be a "toggled" event handler on the checkbox cell renderer which
	   has "activatable" set. */

	if (path != NULL && column != NULL && !strcmp (gtk_tree_view_column_get_title (column), _("Enabled"))) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		gboolean enabled;

		model = gtk_tree_view_get_model (tv);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, 0, &enabled, -1);
		gtk_list_store_set ((GtkListStore *) model, &iter, 0, !enabled, -1);
		gtk_button_set_label ((GtkButton *) prefs->spell_able_button, enabled ? _("Enable") : _("Disable"));
		spell_changed (prefs);
	}

	return FALSE;
}

static void
spell_setup (MailComposerPrefs *prefs)
{
	GtkListStore *model;
	GtkTreeIter iter;
	int i;
	
	model = (GtkListStore *) gtk_tree_view_get_model (prefs->language);
	
	if (prefs->language_seq) {
		for (i = 0; i < prefs->language_seq->_length; i++) {
			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					    1, _(prefs->language_seq->_buffer[i].name),
					    2, prefs->language_seq->_buffer[i].abbreviation,
					    -1);
		}
	}
	
	spell_load_values (prefs);
	spell_set_ui (prefs);
	
	glade_xml_signal_connect_data (prefs->gui, "spellColorSet", G_CALLBACK (spell_color_set), prefs);
	glade_xml_signal_connect_data (prefs->gui, "spellLanguageEnable", GTK_SIGNAL_FUNC (spell_language_enable), prefs);
	
	g_signal_connect (prefs->language, "button_press_event", G_CALLBACK (spell_language_button_press), prefs);
}

static gboolean
spell_setup_check_options (MailComposerPrefs *prefs)
{
	GNOME_Spell_Dictionary dict;
	CORBA_Environment ev;
	char *dictionary_id;
	
	dictionary_id = "OAFIID:GNOME_Spell_Dictionary:" SPELL_API_VERSION;
	dict = bonobo_activation_activate_from_id (dictionary_id, 0, NULL, NULL);
	if (dict == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate %s", dictionary_id);
		
		return FALSE;
	}
	
	CORBA_exception_init (&ev);
	prefs->language_seq = GNOME_Spell_Dictionary_getLanguages (dict, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		prefs->language_seq = NULL;
	CORBA_exception_free (&ev);
	
	if (prefs->language_seq == NULL)
		return FALSE;
	
	gconf_client_add_dir (prefs->gconf, GNOME_SPELL_GCONF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	
        spell_setup (prefs);
	
	return TRUE;
}

/*
 * End of Spell checking
 */

static void
mail_composer_prefs_construct (MailComposerPrefs *prefs)
{
	GtkWidget *toplevel, *widget, *menu, *info_pixmap;
	GtkDialog *dialog;
	GladeXML *gui;
	GtkListStore *model;
	GtkTreeSelection *selection;
	gboolean bool;
	int style;
	char *buf;
	char *names[][2] = {
		{ "live_spell_check", "chkEnableSpellChecking" },
		{ "magic_smileys_check", "chkAutoSmileys" },
		{ "gtk_html_prop_keymap_option", "omenuShortcutsType" },
		{ NULL, NULL }
	};
	
	prefs->gconf = gconf_client_get_default ();
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "composer_tab", NULL);
	prefs->gui = gui;
	prefs->sig_script_gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "vbox_add_script_signature", NULL);
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	/* General tab */
	
	/* Default Behavior */
	prefs->send_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/composer/send_html", NULL);
	gtk_toggle_button_set_active (prefs->send_html, bool);
	g_signal_connect (prefs->send_html, "toggled", G_CALLBACK (toggle_button_toggled), prefs);
	
	prefs->auto_smileys = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkAutoSmileys"));
	/* FIXME: set active? */
	g_signal_connect (prefs->auto_smileys, "toggled", G_CALLBACK (toggle_button_toggled), prefs);
	
	prefs->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/prompts/empty_subject", NULL);
	gtk_toggle_button_set_active (prefs->prompt_empty_subject, bool);
	g_signal_connect (prefs->prompt_empty_subject, "toggled", G_CALLBACK (toggle_button_toggled), prefs);
	
	prefs->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/prompts/only_bcc", NULL);
	gtk_toggle_button_set_active (prefs->prompt_bcc_only, bool);
	g_signal_connect (prefs->prompt_bcc_only, "toggled", G_CALLBACK (toggle_button_toggled), prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/composer/charset", NULL);
	menu = e_charset_picker_new (buf ? buf : e_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	g_free (buf);
	
#warning "gtkhtml prop manager"
#if 0	
	/* Spell Checking: GtkHTML part */
	prefs->pman = GTK_HTML_PROPMANAGER (gtk_html_propmanager_new (NULL));
	g_signal_connect (prefs->pman, "changed", G_CALLBACK(toggle_button_toggled), prefs);
	g_object_ref (prefs->pman);
	
	gtk_html_propmanager_set_names (prefs->pman, names);
	gtk_html_propmanager_set_gui (prefs->pman, gui, NULL);
#endif

	/* Spell Checking: GNOME Spell part */
	prefs->colour = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerSpellCheckColor"));
	prefs->language = GTK_TREE_VIEW (glade_xml_get_widget (gui, "clistSpellCheckLanguage"));
	model = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_view_set_model (prefs->language, (GtkTreeModel *) model);
	gtk_tree_view_insert_column_with_attributes (prefs->language, -1, _("Enabled"),
						     gtk_cell_renderer_toggle_new (),
						     "active", 0,
						     NULL);
	gtk_tree_view_insert_column_with_attributes (prefs->language, -1, _("Language(s)"),
						     gtk_cell_renderer_text_new (),
						     "text", 1,
						     NULL);
	selection = gtk_tree_view_get_selection (prefs->language);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed", G_CALLBACK (spell_language_selection_changed), prefs);
#if 0
	gtk_clist_set_column_justification (prefs->language, 0, GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_auto_resize (prefs->language, 0, TRUE);
#endif

	prefs->spell_able_button = glade_xml_get_widget (gui, "buttonSpellCheckEnable");
	info_pixmap = glade_xml_get_widget (gui, "pixmapSpellInfo");
	gtk_image_set_from_file (GTK_IMAGE (info_pixmap), EVOLUTION_IMAGES "/info-bulb.png");	
	if (!spell_setup_check_options (prefs)) {
		gtk_widget_hide (GTK_WIDGET (prefs->colour));
		gtk_widget_hide (GTK_WIDGET (prefs->language));
	}
	
	/* Forwards and Replies */
	prefs->forward_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuForwardStyle"));
	style = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/format/forward_style", NULL);
	gtk_option_menu_set_history (prefs->forward_style, style);
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->forward_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->forward_style, prefs);
	
	prefs->reply_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuReplyStyle"));
	style = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/format/reply_style", NULL);
	gtk_option_menu_set_history (prefs->reply_style, style);
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->reply_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->reply_style, prefs);
	
	/* Signatures */
	dialog = (GtkDialog *) gtk_dialog_new ();
	prefs->sig_script_dialog = (GtkWidget *) dialog;
	gtk_dialog_add_buttons (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_title ((GtkWindow *) dialog, _("Add script signature"));
	g_signal_connect (dialog, "response", G_CALLBACK (sig_add_script_response), prefs);
	widget = glade_xml_get_widget (prefs->sig_script_gui, "vbox_add_script_signature");
	gtk_box_pack_start_defaults ((GtkBox *) dialog->vbox, widget);
	
	prefs->sig_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureAdd"));
	g_signal_connect (prefs->sig_add, "clicked", G_CALLBACK (sig_add_cb), prefs);
	
	glade_xml_signal_connect_data (gui, "cmdSignatureAddScriptClicked",
				       G_CALLBACK (sig_add_script_cb), prefs);
	
	prefs->sig_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureEdit"));
	g_signal_connect (prefs->sig_edit, "clicked", G_CALLBACK (sig_edit_cb), prefs);
	
	prefs->sig_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureDelete"));
	g_signal_connect (prefs->sig_delete, "clicked", G_CALLBACK (sig_delete_cb), prefs);
	
	prefs->sig_list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "clistSignatures"));
	model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_view_set_model (prefs->sig_list, (GtkTreeModel *)model);
	gtk_tree_view_insert_column_with_attributes (prefs->sig_list, -1, _("Signature(s)"),
						     gtk_cell_renderer_text_new (),
						     "text", 0,
						     NULL);
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed", G_CALLBACK (sig_selection_changed), prefs);
	
	sig_fill_clist (prefs->sig_list);
	if (mail_config_get_signature_list () == NULL) {
		gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, FALSE);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, FALSE);
	}
	
	/* preview GtkHTML widget */
	widget = glade_xml_get_widget (gui, "scrolled-sig");
	prefs->sig_preview = (GtkHTML *) gtk_html_new ();
	g_signal_connect (prefs->sig_preview, "url_requested", G_CALLBACK (url_requested), NULL);
	gtk_widget_show (GTK_WIDGET (prefs->sig_preview));
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (prefs->sig_preview));
	
	mail_config_signature_register_client ((MailConfigSignatureClient) sig_event_client, prefs);
}


GtkWidget *
mail_composer_prefs_new (void)
{
	MailComposerPrefs *new;
	
	new = (MailComposerPrefs *) g_object_new (mail_composer_prefs_get_type (), NULL);
	mail_composer_prefs_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_composer_prefs_apply (MailComposerPrefs *prefs)
{
	GtkWidget *menu, *item;
	char *string;
	int val;
	
	/* General tab */
	
	/* Default Behavior */
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/composer/send_html",
			       gtk_toggle_button_get_active (prefs->send_html), NULL);
	
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/prompts/empty_subject",
			       gtk_toggle_button_get_active (prefs->prompt_empty_subject), NULL);
	
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/prompts/only_bcc",
			       gtk_toggle_button_get_active (prefs->prompt_bcc_only), NULL);
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	if (!(string = e_charset_picker_get_charset (menu)))
		string = g_strdup (e_iconv_locale_charset ());
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/composer/charset", string, NULL);
	g_free (string);
	
	/* Spell Checking */
#warning "gtkhtml propmanager"
#if 0
	gtk_html_propmanager_apply (prefs->pman);
#endif
	spell_apply (prefs);
	
	/* Forwards and Replies */
	menu = gtk_option_menu_get_menu (prefs->forward_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "style"));
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/format/forward_style", val, NULL);
	
	menu = gtk_option_menu_get_menu (prefs->reply_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "style"));
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/format/reply_style", val, NULL);
	
	/* Keyboard Shortcuts */
	/* FIXME: implement me */
	
	/* Signatures */
	/* FIXME: implement me */
	
	gconf_client_suggest_sync (prefs->gconf, NULL);
}
