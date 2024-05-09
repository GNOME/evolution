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
 */

#include "evolution-config.h"

#include "e-mail-display-popup-text-highlight.h"
#include "mail/e-mail-display-popup-extension.h"
#include "mail/e-mail-display.h"
#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include "mail/e-mail-browser.h"

#include <libebackend/libebackend.h>

#include <glib/gi18n-lib.h>

#include "languages.h"

#define d(x)

typedef struct _EMailDisplayPopupTextHighlight {
	EExtension parent;

	GtkActionGroup *action_group;

	volatile gint updating;
	gchar *iframe_src;
	gchar *iframe_id;
} EMailDisplayPopupTextHighlight;

typedef struct _EMailDisplayPopupTextHighlightClass {
	EExtensionClass parent_class;
} EMailDisplayPopupTextHighlightClass;

#define E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), e_mail_display_popup_text_highlight_get_type (), EMailDisplayPopupTextHighlight))

GType e_mail_display_popup_text_highlight_get_type (void);
static void e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailDisplayPopupTextHighlight,
	e_mail_display_popup_text_highlight,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION,
		e_mail_display_popup_extension_interface_init));

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-2'>"
"      <separator />"
"      <menu action='format-as-menu'>"
"	 <placeholder name='format-as-actions' />"
"	 <menu action='format-as-other-menu'>"
"	 </menu>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static const gchar *ui_reader =
"<ui>"
"  <popup name='mail-preview-popup'>"
"    <placeholder name='mail-preview-popup-actions'>"
"      <separator />"
"      <menu action='format-as-menu'>"
"	 <placeholder name='format-as-actions' />"
"	 <menu action='format-as-other-menu'>"
"	 </menu>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

static GtkActionEntry entries[] = {

	{ "format-as-menu",
	  NULL,
	  N_("_Format as…"),
	  NULL,
	  NULL,
	  NULL
	},

	{ "format-as-other-menu",
	  NULL,
	  N_("_Other languages"),
	  NULL,
	  NULL,
	  NULL
	}
};

static void
set_popup_place (EMailDisplayPopupTextHighlight *extension,
		 const gchar *iframe_src,
		 const gchar *iframe_id)
{
	if (g_strcmp0 (extension->iframe_src, iframe_src)) {
		g_free (extension->iframe_src);
		extension->iframe_src = g_strdup (iframe_src);
	}

	if (g_strcmp0 (extension->iframe_id, iframe_id)) {
		g_free (extension->iframe_id);
		extension->iframe_id = g_strdup (iframe_id);
	}
}

static void
reformat (GtkAction *old,
          GtkAction *action,
          gpointer user_data)
{
	EMailDisplayPopupTextHighlight *th_extension;
	GUri *guri;
	GHashTable *query;
	gchar *uri, *query_str;

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (user_data);

	if (g_atomic_int_get (&th_extension->updating))
		return;

	if (th_extension->iframe_src)
		guri = g_uri_parse (th_extension->iframe_src, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	else
		guri = NULL;

	if (!guri)
		return;

	if (!g_uri_get_query (guri)) {
		g_uri_unref (guri);
		return;
	}

	query = soup_form_decode (g_uri_get_query (guri));
	g_hash_table_replace (
		query, g_strdup ("__formatas"), (gpointer) gtk_action_get_name (action));
	g_hash_table_replace (
		query, g_strdup ("mime_type"), (gpointer) "text/plain");
	g_hash_table_replace (
		query, g_strdup ("__force_highlight"), (gpointer) "true");

	#ifdef HAVE_MARKDOWN
	if (g_strcmp0 (gtk_action_get_name (action), "markdown") == 0) {
		g_hash_table_remove (query, "__formatas");
		g_hash_table_remove (query, "__force_highlight");
		g_hash_table_replace (query, g_strdup ("mime_type"), (gpointer) "text/markdown");
	}
	#endif

	query_str = soup_form_encode_hash (query);
	e_util_change_uri_component (&guri, SOUP_URI_QUERY, query_str);
	g_hash_table_unref (query);
	g_free (query_str);

	uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
	g_uri_unref (guri);

	e_web_view_set_iframe_src (E_WEB_VIEW (e_extension_get_extensible (E_EXTENSION (th_extension))),
		th_extension->iframe_id, uri);

	g_free (uri);
}

static GtkActionGroup *
create_group (EMailDisplayPopupExtension *extension)
{
	EExtensible *extensible;
	EWebView *web_view;
	GtkUIManager *ui_manager, *shell_ui_manager;
	GtkActionGroup *group;
	EShell *shell;
	GtkWindow *shell_window;
	gint i;
	gsize len;
	guint merge_id, shell_merge_id;
	Language *languages;
	GSList *radio_group;
	gint action_index;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	web_view = E_WEB_VIEW (extensible);

	ui_manager = e_web_view_get_ui_manager (web_view);
	shell = e_shell_get_default ();
	shell_window = e_shell_get_active_window (shell);
	if (E_IS_SHELL_WINDOW (shell_window)) {
		shell_ui_manager = e_shell_window_get_ui_manager (E_SHELL_WINDOW (shell_window));
	} else if (E_IS_MAIL_BROWSER (shell_window)) {
		shell_ui_manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (shell_window));
	} else {
		return NULL;
	}

	group = gtk_action_group_new ("format-as");
	gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), NULL);

	gtk_ui_manager_insert_action_group (ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	gtk_ui_manager_insert_action_group (shell_ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (shell_ui_manager, ui_reader, -1, NULL);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	shell_merge_id = gtk_ui_manager_new_merge_id (shell_ui_manager);

	languages = get_default_langauges (&len);
	radio_group = NULL;
	action_index = 0;
	for (i = 0; i < len; i++) {

		GtkRadioAction *action;
		action = gtk_radio_action_new (
				languages[i].action_name,
				languages[i].action_label,
				NULL, NULL, action_index);
		action_index++;
		gtk_action_group_add_action (group, GTK_ACTION (action));
		if (radio_group)
			gtk_radio_action_set_group (action, radio_group);
		else
			g_signal_connect (
				action, "changed",
				G_CALLBACK (reformat), extension);
		radio_group = gtk_radio_action_get_group (action);

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/context/custom-actions-2/format-as-menu/format-as-actions",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		gtk_ui_manager_add_ui (
			shell_ui_manager, shell_merge_id,
			"/mail-preview-popup/mail-preview-popup-actions/format-as-menu/format-as-actions",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	languages = get_additinal_languages (&len);
	for (i = 0; i < len; i++) {
		GtkRadioAction *action;

		action = gtk_radio_action_new (
				languages[i].action_name,
				languages[i].action_label,
				NULL, NULL, action_index);
		action_index++;
		gtk_action_group_add_action (group, GTK_ACTION (action));

		if (radio_group)
			gtk_radio_action_set_group (action, radio_group);
		else
			g_signal_connect (
				action, "changed",
				G_CALLBACK (reformat), extension);
		radio_group = gtk_radio_action_get_group (action);

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/context/custom-actions-2/format-as-menu/format-as-other-menu",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		gtk_ui_manager_add_ui (
			shell_ui_manager, shell_merge_id,
			"/mail-preview-popup/mail-preview-popup-actions/format-as-menu/format-as-other-menu",
			languages[i].action_name, languages[i].action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	return group;
}

static gboolean
emdp_text_highlight_is_enabled (void)
{
	GSettings *settings;
	gboolean enabled;

	settings = e_util_ref_settings ("org.gnome.evolution.text-highlight");
	enabled = g_settings_get_boolean (settings, "enabled");
	g_object_unref (settings);

	return enabled;
}

static void
update_actions (EMailDisplayPopupExtension *extension,
		const gchar *popup_iframe_src,
		const gchar *popup_iframe_id)
{
	EMailDisplayPopupTextHighlight *th_extension;

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (extension);

	if (!th_extension->action_group) {
		th_extension->action_group = create_group (extension);
		if (!th_extension->action_group)
			return;
	}

	set_popup_place (th_extension, popup_iframe_src, popup_iframe_id);

	/* If the part below context menu was made by text-highlight formatter,
	 * then try to check what formatter it's using at the moment and set
	 * it as active in the popup menu */
	if (th_extension->iframe_src && strstr (th_extension->iframe_src, ".text-highlight") != NULL) {
		GUri *guri;
		gtk_action_group_set_visible (
			th_extension->action_group, TRUE);

		guri = g_uri_parse (th_extension->iframe_src, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (guri && g_uri_get_query (guri)) {
			GHashTable *query = soup_form_decode (g_uri_get_query (guri));
			const gchar *highlighter;

			if (!emdp_text_highlight_is_enabled () &&
			    g_strcmp0 (g_hash_table_lookup (query, "__force_highlight"), "true") != 0) {
				highlighter = "txt";
			} else {
				highlighter = g_hash_table_lookup (query, "__formatas");
			}

			if (highlighter && *highlighter) {
				GtkAction *action = gtk_action_group_get_action (
					th_extension->action_group, highlighter);
				if (action) {
					gint value;
					g_atomic_int_add (&th_extension->updating, 1);
					g_object_get (
						G_OBJECT (action), "value",
						&value, NULL);
					gtk_radio_action_set_current_value (
						GTK_RADIO_ACTION (action), value);
					g_atomic_int_add (&th_extension->updating, -1);
				}
			}
			g_hash_table_destroy (query);
		}

		if (guri)
			g_uri_unref (guri);
	} else {
		gtk_action_group_set_visible (th_extension->action_group, FALSE);
	}
}

static void
e_mail_display_popup_text_highlight_finalize (GObject *object)
{
	EMailDisplayPopupTextHighlight *extension;

	extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (object);

	g_clear_object (&extension->action_group);
	g_free (extension->iframe_src);
	g_free (extension->iframe_id);

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_mail_display_popup_text_highlight_parent_class)->finalize (object);
}

void
e_mail_display_popup_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_display_popup_text_highlight_register_type (type_module);
}

static void
e_mail_display_popup_text_highlight_class_init (EMailDisplayPopupTextHighlightClass *klass)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	e_mail_display_popup_text_highlight_parent_class = g_type_class_peek_parent (klass);

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = E_TYPE_MAIL_DISPLAY;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = e_mail_display_popup_text_highlight_finalize;
}

static void
e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface)
{
	iface->update_actions = update_actions;
}

void
e_mail_display_popup_text_highlight_class_finalize (EMailDisplayPopupTextHighlightClass *klass)
{
}

static void
e_mail_display_popup_text_highlight_init (EMailDisplayPopupTextHighlight *extension)
{
	extension->action_group = NULL;
	extension->iframe_src = NULL;
	extension->iframe_id = NULL;
}
