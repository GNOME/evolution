/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "e-util/e-signature.h"
#include "e-util/e-signature-list.h"

#include "em-composer-prefs.h"
#include "composer/e-msg-composer.h"

#include <bonobo/bonobo-generic-factory.h>

#include <gal/util/e-iconv.h>
#include <gal/widgets/e-gui-utils.h>

#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>

#include "widgets/misc/e-charset-picker.h"
#include <e-util/e-icon-factory.h>

#include "mail-config.h"


#define d(x)

static void em_composer_prefs_class_init (EMComposerPrefsClass *class);
static void em_composer_prefs_init       (EMComposerPrefs *dialog);
static void em_composer_prefs_destroy    (GtkObject *obj);
static void em_composer_prefs_finalise   (GObject *obj);


static GtkVBoxClass *parent_class = NULL;


GType
em_composer_prefs_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMComposerPrefsClass),
			NULL, NULL,
			(GClassInitFunc) em_composer_prefs_class_init,
			NULL, NULL,
			sizeof (EMComposerPrefs),
			0,
			(GInstanceInitFunc) em_composer_prefs_init,
		};
		
		type = g_type_register_static (gtk_vbox_get_type (), "EMComposerPrefs", &info, 0);
	}
	
	return type;
}

static void
em_composer_prefs_class_init (EMComposerPrefsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	
	object_class->destroy = em_composer_prefs_destroy;
	gobject_class->finalize = em_composer_prefs_finalise;
}

static void
em_composer_prefs_init (EMComposerPrefs *prefs)
{
	prefs->enabled_pixbuf = e_icon_factory_get_icon ("stock_mark", 16);
	prefs->sig_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
row_free (ESignature *sig, GtkTreeRowReference *row, gpointer user_data)
{
	gtk_tree_row_reference_free (row);
}

static void
em_composer_prefs_finalise (GObject *obj)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) obj;
	
	g_object_unref (prefs->gui);
	g_object_unref (prefs->enabled_pixbuf);
	
	g_hash_table_foreach (prefs->sig_hash, (GHFunc) row_free, NULL);
	g_hash_table_destroy (prefs->sig_hash);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_composer_prefs_destroy (GtkObject *obj)
{
	EMComposerPrefs *prefs = (EMComposerPrefs *) obj;
	ESignatureList *signatures;
	
	signatures = mail_config_get_signatures ();
	
	if (prefs->sig_added_id != 0) {
		g_signal_handler_disconnect (signatures, prefs->sig_added_id);
		prefs->sig_added_id = 0;
	}
	
	if (prefs->sig_removed_id != 0) {
		g_signal_handler_disconnect (signatures, prefs->sig_removed_id);
		prefs->sig_removed_id = 0;
	}
	
	if (prefs->sig_changed_id != 0) {
		g_signal_handler_disconnect (signatures, prefs->sig_changed_id);
		prefs->sig_changed_id = 0;
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}


static void
sig_load_preview (EMComposerPrefs *prefs, ESignature *sig)
{
	char *str;
	
	if (!sig) {
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), " ", 1);
		return;
	}
	
	if (sig->script)
		str = mail_config_signature_run_script (sig->filename);
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
signature_added (ESignatureList *signatures, ESignature *sig, EMComposerPrefs *prefs)
{
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	/* autogen signature is special */
	if (sig->autogen)
		return;
	
	model = gtk_tree_view_get_model (prefs->sig_list);
	gtk_list_store_append ((GtkListStore *) model, &iter);
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, sig->name, 1, sig, -1);
	
	path = gtk_tree_model_get_path (model, &iter);
	row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);
	
	g_hash_table_insert (prefs->sig_hash, sig, row);
}

static void
signature_removed (ESignatureList *signatures, ESignature *sig, EMComposerPrefs *prefs)
{
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!(row = g_hash_table_lookup (prefs->sig_hash, sig)))
		return;
	
	g_hash_table_remove (prefs->sig_hash, sig);
	
	model = gtk_tree_view_get_model (prefs->sig_list);
	path = gtk_tree_row_reference_get_path (row);
	gtk_tree_row_reference_free (row);
	
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}
	
	gtk_list_store_remove ((GtkListStore *) model, &iter);
}

static void
signature_changed (ESignatureList *signatures, ESignature *sig, EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	ESignature *cur;
	
	if (!(row = g_hash_table_lookup (prefs->sig_hash, sig)))
		return;
	
	model = gtk_tree_view_get_model (prefs->sig_list);
	path = gtk_tree_row_reference_get_path (row);
	
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}
	
	gtk_tree_path_free (path);
	
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, sig->name, -1);
	
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 1, &cur, -1);
		if (cur == sig)
			sig_load_preview (prefs, sig);
	}
}

static void
sig_edit_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkWidget *parent;
	GtkTreeIter iter;
	ESignature *sig;
	
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, 1, &sig, -1);
	
	if (!sig->script) {
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
		gnome_file_entry_set_filename ((GnomeFileEntry *) entry, sig->filename);
		
		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		gtk_entry_set_text (GTK_ENTRY (entry), sig->name);
		
		g_object_set_data ((GObject *) entry, "sig", sig);
		
		gtk_window_present ((GtkWindow *) prefs->sig_script_dialog);
	}
}

void
em_composer_prefs_new_signature (GtkWindow *parent, gboolean html, const char *script)
{
	ESignature *sig;
	
	sig = mail_config_signature_new (script, script ? TRUE : FALSE, html);
	mail_signature_editor (sig, parent, TRUE);
}

static void
sig_delete_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESignature *sig;
	
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 1, &sig, -1);
		mail_config_remove_signature (sig);
	}
}

static void
sig_add_cb (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GConfClient *gconf;
	gboolean send_html;
	GtkWidget *parent;
	
	gconf = mail_config_get_gconf_client ();
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	
	parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
	
	em_composer_prefs_new_signature ((GtkWindow *) parent, send_html, NULL);
}

static void
sig_add_script_response (GtkWidget *widget, int button, EMComposerPrefs *prefs)
{
	const char *name;
	char *script;
	GtkWidget *dialog;
	GtkWidget *entry;
	
	if (button == GTK_RESPONSE_ACCEPT) {
		entry = glade_xml_get_widget (prefs->sig_script_gui, "fileentry_add_script_script");
		script = gnome_file_entry_get_full_path((GnomeFileEntry *)entry, FALSE);
		
		entry = glade_xml_get_widget (prefs->sig_script_gui, "entry_add_script_name");
		name = gtk_entry_get_text (GTK_ENTRY (entry));
		if (script && *script) {
			struct stat st;
			
			if (stat (script, &st) && S_ISREG (st.st_mode) && access (script, X_OK) == 0) {
				GtkWidget *parent;
				ESignature *sig;
				
				parent = gtk_widget_get_toplevel ((GtkWidget *) prefs);
				parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
				
				if ((sig = g_object_get_data ((GObject *) entry, "sig"))) {
					/* we're just editing an existing signature script */
					g_free (sig->name);
					sig->name = g_strdup (name);
					e_signature_list_change (mail_config_get_signatures (), sig);
				} else {
					em_composer_prefs_new_signature ((GtkWindow *) parent, TRUE, script);
				}
				
				gtk_widget_hide (prefs->sig_script_dialog);
				g_free(script);
				
				return;
			}
		}
		
		g_free(script);
		dialog = gtk_message_dialog_new (GTK_WINDOW (prefs->sig_script_dialog),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("You must specify a valid script name."));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
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
sig_selection_changed (GtkTreeSelection *selection, EMComposerPrefs *prefs)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESignature *sig;
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
sig_fill_list (EMComposerPrefs *prefs)
{
	ESignatureList *signatures;
	GtkListStore *model;
	EIterator *it;
	
	model = (GtkListStore *) gtk_tree_view_get_model (prefs->sig_list);
	gtk_list_store_clear (model);
	
	signatures = mail_config_get_signatures ();
	it = e_list_get_iterator ((EList *) signatures);
	
	while (e_iterator_is_valid (it)) {
		ESignature *sig;
		
		sig = (ESignature *) e_iterator_get (it);
		signature_added (signatures, sig, prefs);
		
		e_iterator_next (it);
	}
	
	g_object_unref (it);
	
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, FALSE);
	
	prefs->sig_added_id = g_signal_connect (signatures, "signature-added", G_CALLBACK (signature_added), prefs);
	prefs->sig_removed_id = g_signal_connect (signatures, "signature-removed", G_CALLBACK (signature_removed), prefs);
	prefs->sig_changed_id = g_signal_connect (signatures, "signature-changed", G_CALLBACK (signature_changed), prefs);
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


/*
 *
 * Spell checking cut'n'pasted from gnome-spell/capplet/main.c
 *
 */

#include "Spell.h"

#define GNOME_SPELL_GCONF_DIR "/GNOME/Spell"
#define SPELL_API_VERSION "0.3"

static void
spell_set_ui (EMComposerPrefs *prefs)
{
	GHashTable *present;
	GtkListStore *model;
	GtkTreeIter iter;
	GError *err = NULL;
	char **strv = NULL;
	guint r, g, b;
	gboolean go;
	char *lang;
	int i;
	
	prefs->spell_active = FALSE;
	
	/* setup the language list */
	if (!(lang = gconf_client_get_string (prefs->gconf, GNOME_SPELL_GCONF_DIR "/language", &err)) || err) {
		g_free (lang);
		g_error_free (err);
		lang = g_strdup (e_iconv_locale_language ());
	}
	
	present = g_hash_table_new (g_str_hash, g_str_equal);
	if (lang && (strv = g_strsplit (lang, " ", 0))) {
		for (i = 0; strv[i]; i++)
			g_hash_table_insert (present, strv[i], strv[i]);
	}
	
	g_free (lang);
	
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
	
	r = gconf_client_get_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_red", NULL);
	g = gconf_client_get_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_green", NULL);
	b = gconf_client_get_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_blue", NULL);
	
	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (prefs->colour), r, g, b, 0xffff);
	
	prefs->spell_active = TRUE;
}

static void
spell_color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, EMComposerPrefs *prefs)
{
	gconf_client_set_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_red", r, NULL);
	gconf_client_set_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_green", g, NULL);
	gconf_client_set_int (prefs->gconf, GNOME_SPELL_GCONF_DIR "/spell_error_color_blue", b, NULL);
}

static void
spell_live_toggled (GtkWidget *widget, gpointer user_data)
{
	/* FIXME: what gconf key is this? */
}

static void
spell_language_selection_changed (GtkTreeSelection *selection, EMComposerPrefs *prefs)
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

static char *
spell_get_language_str (EMComposerPrefs *prefs)
{
	GtkListStore *model;
	GtkTreeIter iter;
	GString *str;
	char *rv;
	
	model = (GtkListStore *) gtk_tree_view_get_model (prefs->language);
	if (!gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter))
		return NULL;
	
	str = g_string_new ("");
	
	do {
		gboolean state;
		char *abbr;
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 0, &state, 2, &abbr, -1);
		
		if (state) {
			if (str->len)
				g_string_append_c (str, ' ');
			g_string_append (str, abbr);
		}
		
		if (!gtk_tree_model_iter_next ((GtkTreeModel *) model, &iter))
			break;
	} while (1);
	
	rv = str->str;
	g_string_free (str, FALSE);
	
	return rv;
}

static void
spell_language_enable (GtkWidget *widget, EMComposerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean state;
	char *str;
	
	selection = gtk_tree_view_get_selection (prefs->language);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, 0, &state, -1);
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, !state, -1);
	gtk_button_set_label ((GtkButton *) prefs->spell_able_button, state ? _("Enable") : _("Disable"));
	
	str = spell_get_language_str (prefs);
	gconf_client_set_string (prefs->gconf, GNOME_SPELL_GCONF_DIR "/language", str ? str : "", NULL);
	g_free (str);
}

static gboolean
spell_language_button_press (GtkTreeView *treeview, GdkEventButton *event, EMComposerPrefs *prefs)
{
	GtkTreeViewColumn *column = NULL;
	GtkTreePath *path = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean enabled;
	char *str;
	
	if (!(gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, &column, NULL, NULL)))
		return FALSE;
	
	/* FIXME: This routine should just be a "toggled" event handler on the checkbox cell renderer which
	   has "activatable" set. */
	
	if (strcmp (gtk_tree_view_column_get_title (column), _("Enabled")) != 0)
		return FALSE;
	
	model = gtk_tree_view_get_model (treeview);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 0, &enabled, -1);
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, !enabled, -1);
	gtk_button_set_label ((GtkButton *) prefs->spell_able_button, enabled ? _("Enable") : _("Disable"));
	
	str = spell_get_language_str (prefs);
	gconf_client_set_string (prefs->gconf, GNOME_SPELL_GCONF_DIR "/language", str ? str : "", NULL);
	g_free (str);
	
	return FALSE;
}

static void
spell_setup (EMComposerPrefs *prefs)
{
	GtkListStore *model;
	GtkTreeIter iter;
	GtkWidget *widget;
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
	
	spell_set_ui (prefs);
	
	widget = glade_xml_get_widget (prefs->gui, "colorpickerSpellCheckColor");
	g_signal_connect (widget, "color_set", G_CALLBACK (spell_color_set), prefs);
	
	widget = glade_xml_get_widget (prefs->gui, "buttonSpellCheckEnable");
	g_signal_connect (widget, "clicked", G_CALLBACK (spell_language_enable), prefs);
	
	widget = glade_xml_get_widget (prefs->gui, "chkEnableSpellChecking");
	g_signal_connect (widget, "toggled", G_CALLBACK (spell_live_toggled), prefs);
	
	g_signal_connect (prefs->language, "button_press_event", G_CALLBACK (spell_language_button_press), prefs);
}

static gboolean
spell_setup_check_options (EMComposerPrefs *prefs)
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
attach_style_info (GtkWidget *item, gpointer user_data)
{
	int *style = user_data;
	
	g_object_set_data ((GObject *) item, "style", GINT_TO_POINTER (*style));
	
	(*style)++;
}

static void
toggle_button_toggled (GtkToggleButton *toggle, EMComposerPrefs *prefs)
{
	const char *key;
	
	key = g_object_get_data ((GObject *) toggle, "key");
	gconf_client_set_bool (prefs->gconf, key, gtk_toggle_button_get_active (toggle), NULL);
}

static void
style_activate (GtkWidget *item, EMComposerPrefs *prefs)
{
	const char *key;
	int style;
	
	key = g_object_get_data ((GObject *) item, "key");
	style = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "style"));
	
	gconf_client_set_int (prefs->gconf, key, style, NULL);
}

static void
charset_activate (GtkWidget *item, EMComposerPrefs *prefs)
{
	GtkWidget *menu;
	char *string;
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	if (!(string = e_charset_picker_get_charset (menu)))
		string = g_strdup (e_iconv_locale_charset ());
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/composer/charset", string, NULL);
	g_free (string);
}

static void
option_menu_connect (EMComposerPrefs *prefs, GtkOptionMenu *omenu, GCallback callback, const char *key)
{
	GtkWidget *menu, *item;
	GList *items;
	
	menu = gtk_option_menu_get_menu (omenu);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		g_object_set_data ((GObject *) item, "key", (void *) key);
		g_signal_connect (item, "activate", callback, prefs);
		items = items->next;
	}
	
	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) omenu, FALSE);
}

static void
toggle_button_init (EMComposerPrefs *prefs, GtkToggleButton *toggle, int not, const char *key)
{
	gboolean bool;
	
	bool = gconf_client_get_bool (prefs->gconf, key, NULL);
	gtk_toggle_button_set_active (toggle, not ? !bool : bool);
	
	g_object_set_data ((GObject *) toggle, "key", (void *) key);
	g_signal_connect (toggle, "toggled", G_CALLBACK (toggle_button_toggled), prefs);
	
	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) toggle, FALSE);
}

static void
em_composer_prefs_construct (EMComposerPrefs *prefs)
{
	GtkWidget *toplevel, *widget, *menu, *info_pixmap;
	GtkDialog *dialog;
	GladeXML *gui;
	GtkListStore *model;
	GtkTreeSelection *selection;
	int style;
	char *buf;
	
	prefs->gconf = mail_config_get_gconf_client ();
	
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
	toggle_button_init (prefs, prefs->send_html, FALSE,
			    "/apps/evolution/mail/composer/send_html");
	
	prefs->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	toggle_button_init (prefs, prefs->prompt_empty_subject, FALSE,
			    "/apps/evolution/mail/prompts/empty_subject");
	
	prefs->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	toggle_button_init (prefs, prefs->prompt_bcc_only, FALSE,
			    "/apps/evolution/mail/prompts/only_bcc");
	
	prefs->auto_smileys = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkAutoSmileys"));
	toggle_button_init (prefs, prefs->auto_smileys, FALSE,
			    "/apps/evolution/mail/composer/magic_smileys");
	
	prefs->spell_check = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEnableSpellChecking"));
	toggle_button_init (prefs, prefs->spell_check, FALSE,
			    "/apps/evolution/mail/composer/inline_spelling");
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/composer/charset", NULL);
	menu = e_charset_picker_new (buf && *buf ? buf : e_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs, prefs->charset, G_CALLBACK (charset_activate),
			     "/apps/evolution/mail/composer/charset");
	g_free (buf);
	
	/* Spell Checking: GNOME Spell part */
	prefs->colour = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerSpellCheckColor"));
	prefs->language = GTK_TREE_VIEW (glade_xml_get_widget (gui, "listSpellCheckLanguage"));
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
	
	prefs->spell_able_button = glade_xml_get_widget (gui, "buttonSpellCheckEnable");
	info_pixmap = glade_xml_get_widget (gui, "pixmapSpellInfo");
	gtk_image_set_from_stock (GTK_IMAGE (info_pixmap), GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_BUTTON);
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
	option_menu_connect (prefs, prefs->forward_style, G_CALLBACK (style_activate),
			     "/apps/evolution/mail/format/forward_style");
	
	prefs->reply_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuReplyStyle"));
	style = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/format/reply_style", NULL);
	gtk_option_menu_set_history (prefs->reply_style, style);
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->reply_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs, prefs->reply_style, G_CALLBACK (style_activate),
			     "/apps/evolution/mail/format/reply_style");
	
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
	
	prefs->sig_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureAdd"));
	g_signal_connect (prefs->sig_add, "clicked", G_CALLBACK (sig_add_cb), prefs);
	
	prefs->sig_add_script = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureAddScript"));
	g_signal_connect (prefs->sig_add_script, "clicked", G_CALLBACK (sig_add_script_cb), prefs);
	
	prefs->sig_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureEdit"));
	g_signal_connect (prefs->sig_edit, "clicked", G_CALLBACK (sig_edit_cb), prefs);
	
	prefs->sig_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureDelete"));
	g_signal_connect (prefs->sig_delete, "clicked", G_CALLBACK (sig_delete_cb), prefs);
	
	prefs->sig_list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "listSignatures"));
	model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_view_set_model (prefs->sig_list, (GtkTreeModel *)model);
	gtk_tree_view_insert_column_with_attributes (prefs->sig_list, -1, _("Signature(s)"),
						     gtk_cell_renderer_text_new (),
						     "text", 0,
						     NULL);
	selection = gtk_tree_view_get_selection (prefs->sig_list);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed", G_CALLBACK (sig_selection_changed), prefs);
	
	sig_fill_list (prefs);
	
	/* preview GtkHTML widget */
	widget = glade_xml_get_widget (gui, "scrolled-sig");
	prefs->sig_preview = (GtkHTML *) gtk_html_new ();
	g_signal_connect (prefs->sig_preview, "url_requested", G_CALLBACK (url_requested), NULL);
	gtk_widget_show (GTK_WIDGET (prefs->sig_preview));
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (prefs->sig_preview));
}


GtkWidget *
em_composer_prefs_new (void)
{
	EMComposerPrefs *new;
	
	new = (EMComposerPrefs *) g_object_new (em_composer_prefs_get_type (), NULL);
	em_composer_prefs_construct (new);
	
	return (GtkWidget *) new;
}
