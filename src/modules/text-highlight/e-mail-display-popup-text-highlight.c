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
#include "mail/e-mail-reader.h"

#include <libebackend/libebackend.h>

#include <glib/gi18n-lib.h>

#include "languages.h"

#define d(x)

typedef struct _EMailDisplayPopupTextHighlight {
	EExtension parent;

	EUIAction *menu_action_webview;
	EUIAction *item_action_webview;
	EUIAction *menu_action_reader;
	EUIAction *item_action_reader;
	GMenu *menu;

	gint updating;
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
text_hightlight_format_as_menu_item_set_state_cb (EUIAction *action,
						  GVariant *parameter,
						  gpointer user_data)
{
	EMailDisplayPopupTextHighlight *self = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (user_data);
	GUri *guri;
	GHashTable *query;
	gchar *uri, *query_str;

	e_ui_action_set_state (action, parameter);

	if (g_atomic_int_get (&self->updating))
		return;

	if (self->iframe_src)
		guri = g_uri_parse (self->iframe_src, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	else
		guri = NULL;

	if (!guri)
		return;

	if (!g_uri_get_query (guri)) {
		g_uri_unref (guri);
		return;
	}

	query = soup_form_decode (g_uri_get_query (guri));
	g_hash_table_replace (query, g_strdup ("__formatas"), (gpointer) g_variant_get_string (parameter, NULL));
	g_hash_table_replace (query, g_strdup ("mime_type"), (gpointer) "text/plain");
	g_hash_table_replace (query, g_strdup ("__force_highlight"), (gpointer) "true");

	#ifdef HAVE_MARKDOWN
	if (g_strcmp0 (g_variant_get_string (parameter, NULL), "markdown") == 0) {
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

	e_web_view_set_iframe_src (E_WEB_VIEW (e_extension_get_extensible (E_EXTENSION (self))),
		self->iframe_id, uri);

	g_free (uri);
}

static gboolean
text_highlight_ui_manager_create_item_cb (EUIManager *ui_manager,
					  EUIElement *elem,
					  EUIAction *action,
					  EUIElementKind for_kind,
					  GObject **out_item,
					  gpointer user_data)
{
	GMenuModel *format_as_menu = user_data;
	const gchar *name;

	g_return_val_if_fail (G_IS_MENU (format_as_menu), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EPluginTextHighlight::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (is_action ("EPluginTextHighlight::format-as-menu")) {
		*out_item = e_ui_manager_create_item_from_menu_model (ui_manager, elem, action, for_kind, format_as_menu);
	} else if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
create_actions (EMailDisplayPopupExtension *extension)
{
	static const gchar *eui_webview =
		"<eui>"
		  "<menu id='context'>"
		    "<placeholder id='custom-actions-2'>"
		      "<separator/>"
		      "<item action='EPluginTextHighlight::format-as-menu'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "format-as-menu-item",
		   NULL,
		   N_("_Format as…"),
		   NULL,
		   NULL,
		   NULL, "s", "'txt'", text_hightlight_format_as_menu_item_set_state_cb },

		{ "EPluginTextHighlight::format-as-menu", NULL, N_("_Format as…"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	EMailDisplayPopupTextHighlight *self = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (extension);
	EExtensible *extensible;
	EUIManager *ui_manager;
	EUIActionGroup *group;
	EWebView *web_view;
	EMailReader *mail_reader;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	web_view = E_WEB_VIEW (extensible);
	ui_manager = e_web_view_get_ui_manager (web_view);
	g_return_if_fail (ui_manager != NULL);

	g_signal_connect_data (ui_manager, "create-item",
		G_CALLBACK (text_highlight_ui_manager_create_item_cb), g_object_ref (self->menu),
		(GClosureNotify) g_object_unref, 0);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "EPluginTextHighlight", NULL,
		entries, G_N_ELEMENTS (entries), extension, eui_webview);

	group = e_ui_manager_get_action_group (ui_manager, "EPluginTextHighlight");
	self->menu_action_webview = g_object_ref (e_ui_action_group_get_action (group, "EPluginTextHighlight::format-as-menu"));
	self->item_action_webview = g_object_ref (e_ui_action_group_get_action (group, "format-as-menu-item"));

	mail_reader = e_mail_display_ref_mail_reader (E_MAIL_DISPLAY (web_view));
	if (mail_reader) {
		static const gchar *eui_reader =
			"<eui>"
			  "<menu id='mail-preview-popup'>"
			    "<placeholder id='mail-previewXXX-popup-actions'>"
			      "<separator/>"
			      "<item action='EPluginTextHighlight::format-as-menu'/>"
			    "</placeholder>"
			  "</menu>"
			"</eui>";

		ui_manager = e_mail_reader_get_ui_manager (mail_reader);

		g_signal_connect_data (ui_manager, "create-item",
			G_CALLBACK (text_highlight_ui_manager_create_item_cb), g_object_ref (self->menu),
			(GClosureNotify) g_object_unref, 0);

		e_ui_manager_add_actions_with_eui_data (ui_manager, "EPluginTextHighlight", NULL,
			entries, G_N_ELEMENTS (entries), extension, eui_reader);

		group = e_ui_manager_get_action_group (ui_manager, "EPluginTextHighlight");
		self->menu_action_reader = g_object_ref (e_ui_action_group_get_action (group, "EPluginTextHighlight::format-as-menu"));
		self->item_action_reader = g_object_ref (e_ui_action_group_get_action (group, "format-as-menu-item"));

		g_clear_object (&mail_reader);
	}
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

	if (!th_extension->menu_action_webview) {
		create_actions (extension);
		if (!th_extension->menu_action_webview)
			return;
	}

	set_popup_place (th_extension, popup_iframe_src, popup_iframe_id);

	/* If the part below context menu was made by text-highlight formatter,
	 * then try to check what formatter it's using at the moment and set
	 * it as active in the popup menu */
	if (th_extension->iframe_src && strstr (th_extension->iframe_src, ".text-highlight") != NULL) {
		GVariant *state = NULL;
		GUri *guri;

		e_ui_action_set_visible (th_extension->menu_action_webview, TRUE);
		if (th_extension->menu_action_reader)
			e_ui_action_set_visible (th_extension->menu_action_reader, TRUE);

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

			if (highlighter && *highlighter)
				state = g_variant_new_string (highlighter);

			g_hash_table_destroy (query);
		}

		if (guri)
			g_uri_unref (guri);

		if (!state)
			state = g_variant_new_string ("txt");

		g_variant_ref_sink (state);
		g_atomic_int_add (&th_extension->updating, 1);
		e_ui_action_set_state (th_extension->item_action_webview, state);
		if (th_extension->item_action_reader)
			e_ui_action_set_state (th_extension->item_action_reader, state);
		g_atomic_int_add (&th_extension->updating, -1);
		g_variant_unref (state);
	} else {
		e_ui_action_set_visible (th_extension->menu_action_webview, FALSE);
		if (th_extension->menu_action_reader)
			e_ui_action_set_visible (th_extension->menu_action_reader, FALSE);
	}
}

static void
e_mail_display_popup_text_highlight_finalize (GObject *object)
{
	EMailDisplayPopupTextHighlight *extension;

	extension = E_MAIL_DISPLAY_POPUP_TEXT_HIGHLIGHT (object);

	g_clear_object (&extension->menu_action_webview);
	g_clear_object (&extension->menu_action_reader);
	g_clear_object (&extension->item_action_webview);
	g_clear_object (&extension->item_action_reader);
	g_clear_object (&extension->menu);
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
	Language *languages;
	GMenu *others_menu;
	gsize ii, len;

	extension->menu = g_menu_new ();

	languages = get_default_langauges (&len);
	for (ii = 0; ii < len; ii++) {
		gchar *detailed_action;

		detailed_action = g_strdup_printf ("EPluginTextHighlight.format-as-menu-item('%s')", languages[ii].action_name);

		g_menu_append (extension->menu, languages[ii].action_label, detailed_action);

		g_free (detailed_action);
	}

	others_menu = g_menu_new ();

	languages = get_additinal_languages (&len);
	for (ii = 0; ii < len; ii++) {
		gchar *detailed_action;

		detailed_action = g_strdup_printf ("EPluginTextHighlight.format-as-menu-item('%s')", languages[ii].action_name);

		g_menu_append (others_menu, languages[ii].action_label, detailed_action);

		g_free (detailed_action);
	}

	g_menu_append_submenu (extension->menu, N_("_Other languages"), G_MENU_MODEL (others_menu));

	g_clear_object (&others_menu);
}
