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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

	WebKitDOMDocument *document;
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
	  N_("_Format as..."),
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
reformat (GtkAction *old,
          GtkAction *action,
          gpointer user_data)
{
	EMailDisplayPopupTextHighlight *th_extension;
	WebKitDOMDocument *doc;
	WebKitDOMDOMWindow *window;
	WebKitDOMElement *frame_element;
	SoupURI *soup_uri;
	GHashTable *query;
	gchar *uri;

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (user_data);
	doc = th_extension->document;
	if (!doc)
		return;

	uri = webkit_dom_document_get_document_uri (doc);
	soup_uri = soup_uri_new (uri);
	g_free (uri);

	if (!soup_uri)
		goto exit;

	if (!soup_uri->query) {
		soup_uri_free (soup_uri);
		goto exit;
	}

	query = soup_form_decode (soup_uri->query);
	g_hash_table_replace (
		query, g_strdup ("__formatas"), (gpointer) gtk_action_get_name (action));
	g_hash_table_replace (
		query, g_strdup ("mime_type"), (gpointer) "text/plain");

	soup_uri_set_query_from_form (soup_uri, query);
	g_hash_table_destroy (query);

	uri = soup_uri_to_string (soup_uri, FALSE);
	soup_uri_free (soup_uri);

	/* Get frame's window and from the window the actual <iframe> element */
	window = webkit_dom_document_get_default_view (doc);
	frame_element = webkit_dom_dom_window_get_frame_element (window);
	webkit_dom_html_iframe_element_set_src (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (frame_element), uri);

	g_free (uri);

	/* The frame has been reloaded, the document pointer is invalid now */
exit:
	th_extension->document = NULL;
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
		g_signal_connect (
			action, "changed",
			G_CALLBACK (reformat), extension);
		gtk_radio_action_set_group (action, radio_group);
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
		g_signal_connect (
			action, "changed",
			G_CALLBACK (reformat), extension);

		gtk_radio_action_set_group (action, radio_group);
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

static void
update_actions (EMailDisplayPopupExtension *extension,
                WebKitHitTestResult *context)
{
	EMailDisplayPopupTextHighlight *th_extension;
	WebKitDOMNode *node;
	WebKitDOMDocument *document;
	gchar *uri;

	th_extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (extension);

	if (th_extension->action_group == NULL) {
		th_extension->action_group = create_group (extension);
	}

	th_extension->document = NULL;
	g_object_get (G_OBJECT (context), "inner-node", &node, NULL);
	document = webkit_dom_node_get_owner_document (node);
	uri = webkit_dom_document_get_document_uri (document);

	/* If the part below context menu was made by text-highlight formatter,
	 * then try to check what formatter it's using at the moment and set
	 * it as active in the popup menu */
	if (uri && strstr (uri, ".text-highlight") != NULL) {
		SoupURI *soup_uri;
		gtk_action_group_set_visible (
			th_extension->action_group, TRUE);

		soup_uri = soup_uri_new (uri);
		if (soup_uri && soup_uri->query) {
			GHashTable *query = soup_form_decode (soup_uri->query);
			gchar *highlighter;

			highlighter = g_hash_table_lookup (query, "__formatas");
			if (highlighter && *highlighter) {
				GtkAction *action = gtk_action_group_get_action (
					th_extension->action_group, highlighter);
				if (action) {
					gint value;
					g_object_get (
						G_OBJECT (action), "value",
						&value, NULL);
					gtk_radio_action_set_current_value (
						GTK_RADIO_ACTION (action), value);
				}
			}
			g_hash_table_destroy (query);
		}

		if (soup_uri) {
			soup_uri_free (soup_uri);
		}

	} else {
		gtk_action_group_set_visible (
			th_extension->action_group, FALSE);
	}

	/* Set the th_extension->document AFTER changing the active action to
	 * prevent the reformat() from doing some crazy reformatting
	 * (reformat() returns immediatelly when th_extension->document is NULL) */
	th_extension->document = document;

	g_free (uri);
}

void
e_mail_display_popup_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_display_popup_text_highlight_register_type (type_module);
}

static void
e_mail_display_popup_text_highlight_class_init (EMailDisplayPopupTextHighlightClass *klass)
{
	EExtensionClass *extension_class;

	e_mail_display_popup_text_highlight_parent_class = g_type_class_peek_parent (klass);

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = E_TYPE_MAIL_DISPLAY;
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
}
