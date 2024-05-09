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

#include "e-mail-display-popup-prefer-plain.h"
#include "mail/e-mail-display-popup-extension.h"
#include "mail/e-mail-display.h"
#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include "mail/e-mail-browser.h"

#include <libebackend/libebackend.h>

#include <glib/gi18n-lib.h>

#define d(x)

typedef struct _EMailDisplayPopupPreferPlain EMailDisplayPopupPreferPlain;
typedef struct _EMailDisplayPopupPreferPlainClass EMailDisplayPopupPreferPlainClass;

struct _EMailDisplayPopupPreferPlain {
	EExtension parent;

	gchar *text_plain_id;
	gchar *text_html_id;
	gchar *iframe_src;
	gchar *iframe_id;

	GtkActionGroup *action_group;
};

struct _EMailDisplayPopupPreferPlainClass {
	EExtensionClass parent_class;
};

#define E_TYPE_MAIL_DISPLAY_POPUP_PREFER_PLAIN \
	(e_mail_display_popup_prefer_plain_get_type ())
#define E_MAIL_DISPLAY_POPUP_PREFER_PLAIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_DISPLAY_POPUP_PREFER_PLAIN, EMailDisplayPopupPreferPlain))

GType e_mail_display_popup_prefer_plain_get_type (void);
static void e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailDisplayPopupPreferPlain,
	e_mail_display_popup_prefer_plain,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_DISPLAY_POPUP_EXTENSION,
		e_mail_display_popup_extension_interface_init));

static const gchar *ui_webview =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions-2'>"
"      <separator/>"
"      <menuitem action='show-plain-text-part'/>"
"      <menuitem action='show-text-html-part'/>"
"      <separator/>"
"    </placeholder>"
"  </popup>"
"</ui>";

static const gchar *ui_reader =
"<ui>"
"  <popup name='mail-preview-popup'>"
"    <placeholder name='mail-preview-popup-actions'>"
"      <separator/>"
"      <menuitem action='show-plain-text-part'/>"
"      <menuitem action='show-text-html-part'/>"
"      <separator/>"
"    </placeholder>"
"  </popup>"
"</ui>";

static void
toggle_part (GtkAction *action,
             EMailDisplayPopupExtension *extension)
{
	EMailDisplayPopupPreferPlain *pp_extension = (EMailDisplayPopupPreferPlain *) extension;
	GUri *guri;
	GHashTable *query;
	gchar *uri, *query_str;

	if (!pp_extension->iframe_src)
		return;

	guri = g_uri_parse (pp_extension->iframe_src, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

	if (!guri || !g_uri_get_query (guri)) {
		if (guri)
			g_uri_unref (guri);
		return;
	}

	query = soup_form_decode (g_uri_get_query (guri));
	g_hash_table_replace (
		query, g_strdup ("part_id"),
		pp_extension->text_html_id ?
			pp_extension->text_html_id :
			pp_extension->text_plain_id);
	g_hash_table_replace (
		query, g_strdup ("mime_type"),
		pp_extension->text_html_id ?
			(gpointer) "text/html" :
			(gpointer) "text/plain");

	query_str = soup_form_encode_hash (query);
	e_util_change_uri_component (&guri, SOUP_URI_QUERY, query_str);
	g_hash_table_unref (query);
	g_free (query_str);

	uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
	g_uri_unref (guri);

	e_web_view_set_iframe_src (E_WEB_VIEW (e_extension_get_extensible (E_EXTENSION (extension))),
		pp_extension->iframe_id, uri);

	g_free (uri);
}

GtkActionEntry entries[] = {

	{ "show-plain-text-part",
	   NULL,
	   N_("Display plain text version"),
	   NULL,
	   N_("Display plain text version of multipart/alternative message"),
	   NULL
	},

	{ "show-text-html-part",
	  NULL,
	  N_("Display HTML version"),
	  NULL,
	  N_("Display HTML version of multipart/alternative message"),
	  NULL
	}
};

const gint ID_LEN = G_N_ELEMENTS (".alternative-prefer-plain.");

static void
set_text_plain_id (EMailDisplayPopupPreferPlain *extension,
                   const gchar *id)
{
	g_free (extension->text_plain_id);
	extension->text_plain_id = g_strdup (id);
}

static void
set_text_html_id (EMailDisplayPopupPreferPlain *extension,
                  const gchar *id)
{
	g_free (extension->text_html_id);
	extension->text_html_id = g_strdup (id);
}

static void
set_popup_place (EMailDisplayPopupPreferPlain *extension,
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

static GtkActionGroup *
create_group (EMailDisplayPopupExtension *extension)
{
	EExtensible *extensible;
	EWebView *web_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *group;
	GtkAction *action;
	EShell *shell;
	GtkWindow *shell_window;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	web_view = E_WEB_VIEW (extensible);

	shell = e_shell_get_default ();
	shell_window = e_shell_get_active_window (shell);
	if (E_IS_SHELL_WINDOW (shell_window)) {
		ui_manager = e_shell_window_get_ui_manager (E_SHELL_WINDOW (shell_window));
	} else if (E_IS_MAIL_BROWSER (shell_window)) {
		ui_manager = e_mail_browser_get_ui_manager (E_MAIL_BROWSER (shell_window));
	} else {
		return NULL;
	}

	group = gtk_action_group_new ("prefer-plain");
	gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), NULL);

	gtk_ui_manager_insert_action_group (ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui_reader, -1, NULL);

	ui_manager = e_web_view_get_ui_manager (web_view);
	gtk_ui_manager_insert_action_group (ui_manager, group, 0);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui_webview, -1, NULL);

	action = gtk_action_group_get_action (group, "show-plain-text-part");
	g_signal_connect (
		action, "activate",
		G_CALLBACK (toggle_part), extension);

	action = gtk_action_group_get_action (group, "show-text-html-part");
	g_signal_connect (
		action, "activate",
		G_CALLBACK (toggle_part), extension);

	return group;
}

static void
mail_display_popup_prefer_plain_update_actions (EMailDisplayPopupExtension *extension,
						const gchar *popup_iframe_src,
						const gchar *popup_iframe_id)
{
	EMailDisplay *display;
	EMailDisplayPopupPreferPlain *pp_extension;
	GtkAction *action;
	gchar *part_id, *pos, *prefix;
	GUri *guri;
	GHashTable *query;
	EMailPartList *part_list;
	gboolean is_text_plain;
	const gchar *action_name;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;

	display = E_MAIL_DISPLAY (e_extension_get_extensible (
			E_EXTENSION (extension)));

	pp_extension = E_MAIL_DISPLAY_POPUP_PREFER_PLAIN (extension);

	if (!pp_extension->action_group) {
		pp_extension->action_group = create_group (extension);
		if (!pp_extension->action_group)
			return;
	}

	set_popup_place (pp_extension, popup_iframe_src, popup_iframe_id);

	if (pp_extension->iframe_src)
		guri = g_uri_parse (pp_extension->iframe_src, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	else
		guri = NULL;

	if (!guri || !g_uri_get_query (guri)) {
		gtk_action_group_set_visible (pp_extension->action_group, FALSE);
		if (guri)
			g_uri_unref (guri);
		return;
	}

	query = soup_form_decode (g_uri_get_query (guri));
	part_id = g_hash_table_lookup (query, "part_id");
	if (part_id == NULL) {
		gtk_action_group_set_visible (pp_extension->action_group, FALSE);
		goto out;
	}

	pos = strstr (part_id, ".alternative-prefer-plain.");
	if (!pos) {
		gtk_action_group_set_visible (pp_extension->action_group, FALSE);
		goto out;
	}

	/* Don't display the actions on any other than text/plain or text/html parts */
	if (!strstr (pos, "plain_text") && !strstr (pos, "text_html")) {
		gtk_action_group_set_visible (pp_extension->action_group, FALSE);
		goto out;
	}

	/* Check whether the displayed part is text_plain */
	is_text_plain = (strstr (pos + ID_LEN, "plain_text") != NULL);

	/* It is! Hide the menu action */
	if (is_text_plain) {
		action = gtk_action_group_get_action (
			pp_extension->action_group, "show-plain-text-part");
		gtk_action_set_visible (action, FALSE);
	} else {
		action = gtk_action_group_get_action (
			pp_extension->action_group, "show-text-html-part");
		gtk_action_set_visible (action, FALSE);
	}

	/* Now check whether HTML version exists, if it does enable the action */
	prefix = g_strndup (part_id, (pos - part_id) + ID_LEN - 1);

	action_name = NULL;
	part_list = e_mail_display_get_part_list (display);
	e_mail_part_list_queue_parts (part_list, NULL, &queue);
	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *p = link->data;

		if (e_mail_part_id_has_prefix (p, prefix) &&
		    (e_mail_part_id_has_substr (p, "text_html") ||
		     e_mail_part_id_has_substr (p, "plain_text"))) {
			const gchar *p_id;

			p_id = e_mail_part_get_id (p);

			pos = strstr (p_id, ".alternative-prefer-plain.");

			if (is_text_plain) {
				if (strstr (pos + ID_LEN, "text_html") != NULL) {
					action_name = "show-text-html-part";
					set_text_html_id (pp_extension, p_id);
					set_text_plain_id (pp_extension, NULL);
					break;
				}
			} else {
				if (strstr (pos + ID_LEN, "plain_text") != NULL) {
					action_name = "show-plain-text-part";
					set_text_html_id (pp_extension, NULL);
					set_text_plain_id (pp_extension, p_id);
					break;
				}
			}
		}
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	if (action_name) {
		action = gtk_action_group_get_action (
			pp_extension->action_group, action_name);
		gtk_action_group_set_visible (pp_extension->action_group, TRUE);
		gtk_action_set_visible (action, TRUE);
	} else {
		gtk_action_group_set_visible (pp_extension->action_group, FALSE);
	}

	g_free (prefix);
 out:
	g_hash_table_destroy (query);
	g_uri_unref (guri);
}

void
e_mail_display_popup_prefer_plain_type_register (GTypeModule *type_module)
{
	e_mail_display_popup_prefer_plain_register_type (type_module);
}

static void
e_mail_display_popup_prefer_plain_dispose (GObject *object)
{
	EMailDisplayPopupPreferPlain *extension;

	extension = E_MAIL_DISPLAY_POPUP_PREFER_PLAIN (object);
	g_clear_object (&extension->action_group);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_display_popup_prefer_plain_parent_class)->dispose (object);
}

static void
e_mail_display_popup_prefer_plain_finalize (GObject *object)
{
	EMailDisplayPopupPreferPlain *extension;

	extension = E_MAIL_DISPLAY_POPUP_PREFER_PLAIN (object);

	g_free (extension->text_html_id);
	g_free (extension->text_plain_id);
	g_free (extension->iframe_src);
	g_free (extension->iframe_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_display_popup_prefer_plain_parent_class)->finalize (object);
}

static void
e_mail_display_popup_prefer_plain_class_init (EMailDisplayPopupPreferPlainClass *class)
{
	EExtensionClass *extension_class;
	GObjectClass *object_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_DISPLAY;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_mail_display_popup_prefer_plain_dispose;
	object_class->finalize = e_mail_display_popup_prefer_plain_finalize;
}

static void
e_mail_display_popup_extension_interface_init (EMailDisplayPopupExtensionInterface *iface)
{
	iface->update_actions = mail_display_popup_prefer_plain_update_actions;
}

void
e_mail_display_popup_prefer_plain_class_finalize (EMailDisplayPopupPreferPlainClass *class)
{
}

static void
e_mail_display_popup_prefer_plain_init (EMailDisplayPopupPreferPlain *extension)
{
	extension->action_group = NULL;
	extension->text_html_id = NULL;
	extension->text_plain_id = NULL;
	extension->iframe_src = NULL;
	extension->iframe_id = NULL;
}
