/*
 * e-shell-searchbar.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-searchbar
 * @short_description: quick search interface
 * @include: shell/e-shell-searchbar.h
 **/

#include "evolution-config.h"

#include "e-shell-searchbar.h"

#include <glib/gi18n-lib.h>
#include <libebackend/libebackend.h>

#include "e-shell-window-actions.h"

#define SEARCH_OPTION_ADVANCED		(-1)

/* Default "state key file" group: [Search Bar] */
#define STATE_GROUP_DEFAULT		"Search Bar"

#define STATE_KEY_SEARCH_FILTER		"SearchFilter"
#define STATE_KEY_SEARCH_OPTION		"SearchOption"
#define STATE_KEY_SEARCH_SCOPE		"SearchScope"
#define STATE_KEY_SEARCH_TEXT		"SearchText"

struct _EShellSearchbarPrivate {

	gpointer shell_view;  /* weak pointer */

	EUIAction *search_option;
	EFilterRule *search_rule;
	GtkCssProvider *css_provider;

	/* Child Widgets (not referenced) */
	GtkWidget *filter_combo_box;
	GtkWidget *search_entry;
	GtkWidget *scope_combo_box;

	/* State Key File */
	gchar *state_group;

	gchar *active_search_text;

	gboolean filter_visible;
	gboolean scope_visible;
	gboolean state_dirty;
};

enum {
	PROP_0,
	PROP_FILTER_COMBO_BOX,
	PROP_FILTER_VISIBLE,
	PROP_SEARCH_HINT,
	PROP_SEARCH_OPTION,
	PROP_SEARCH_TEXT,
	PROP_SCOPE_COMBO_BOX,
	PROP_SCOPE_VISIBLE,
	PROP_SHELL_VIEW,
	PROP_STATE_GROUP
};

G_DEFINE_TYPE_WITH_CODE (EShellSearchbar, e_shell_searchbar, GTK_TYPE_BOX,
	G_ADD_PRIVATE (EShellSearchbar)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static EUIAction *
shell_searchbar_radio_action_get_current_action (EUIAction *group_action)
{
	GPtrArray *radio_group;
	guint ii;

	radio_group = e_ui_action_get_radio_group (group_action);

	if (!radio_group)
		return group_action;

	for (ii = 0; ii < radio_group->len; ii++) {
		EUIAction *action = g_ptr_array_index (radio_group, ii);

		if (e_ui_action_get_active (action))
			return action;
	}

	return group_action;
}

static void
shell_searchbar_save_current_action (EShellSearchbar *searchbar,
				     const gchar *key,
				     EUIAction *action)
{
	EShellView *shell_view;
	GKeyFile *key_file;
	const gchar *action_name;
	const gchar *state_group;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	state_group = e_shell_searchbar_get_state_group (searchbar);
	g_return_if_fail (state_group != NULL);

	key_file = e_shell_view_get_state_key_file (shell_view);

	if (action != NULL)
		action = shell_searchbar_radio_action_get_current_action (action);

	if (action) {
		action_name = g_action_get_name (G_ACTION (action));
		g_key_file_set_string (key_file, state_group, key, action_name);
	} else {
		g_key_file_remove_key (key_file, state_group, key, NULL);
	}

	e_shell_view_set_state_dirty (shell_view);
}

static void
shell_searchbar_save_search_filter (EShellSearchbar *searchbar)
{
	EActionComboBox *action_combo_box;
	EUIAction *action;

	action_combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	action = e_action_combo_box_get_action (action_combo_box);

	shell_searchbar_save_current_action (searchbar, STATE_KEY_SEARCH_FILTER, action);
}

static void
shell_searchbar_save_search_option (EShellSearchbar *searchbar)
{
	EUIAction *action;

	action = e_shell_searchbar_get_search_option (searchbar);

	shell_searchbar_save_current_action (searchbar, STATE_KEY_SEARCH_OPTION, action);
}

static void
shell_searchbar_save_search_text (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	GKeyFile *key_file;
	const gchar *search_text;
	const gchar *state_group;
	const gchar *key;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	state_group = e_shell_searchbar_get_state_group (searchbar);
	g_return_if_fail (state_group != NULL);

	key = STATE_KEY_SEARCH_TEXT;
	key_file = e_shell_view_get_state_key_file (shell_view);

	search_text = e_shell_searchbar_get_search_text (searchbar);

	if (search_text != NULL && *search_text != '\0')
		g_key_file_set_string (key_file, state_group, key, search_text);
	else
		g_key_file_remove_key (key_file, state_group, key, NULL);

	e_shell_view_set_state_dirty (shell_view);
}

static void
shell_searchbar_save_search_scope (EShellSearchbar *searchbar)
{
	EActionComboBox *action_combo_box;
	EUIAction *action;

	action_combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	action = e_action_combo_box_get_action (action_combo_box);

	shell_searchbar_save_current_action (searchbar, STATE_KEY_SEARCH_SCOPE, action);
}

static void
shell_searchbar_update_search_widgets (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	GtkWidget *widget;
	const gchar *search_text;
	gboolean sensitive;

	/* EShellView subclasses are responsible for actually
	 * executing the search.  This is all cosmetic stuff. */

	widget = searchbar->priv->search_entry;
	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	search_text = e_shell_searchbar_get_search_text (searchbar);

	sensitive =
		(searchbar->priv->active_search_text && *searchbar->priv->active_search_text) ||
		(search_text != NULL && *search_text != '\0') ||
		(e_shell_view_get_search_rule (shell_view) != NULL);

	if (sensitive) {
		GdkRGBA bg, fg;
		gchar *css;

		e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &bg);
		e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg);

		css = g_strdup_printf (
			"#searchbar_searchentry_active { "
			"   background:none; "
			"   background-color:#%06x; "
			"   color:#%06x; "
			"}",
			e_rgba_to_value (&bg),
			e_rgba_to_value (&fg));
		gtk_css_provider_load_from_data (
			searchbar->priv->css_provider, css, -1, NULL);
		g_free (css);

		gtk_widget_set_name (widget, "searchbar_searchentry_active");
	} else {
		gtk_widget_set_name (widget, "searchbar_searchentry");
	}

	if (e_shell_view_is_active (shell_view)) {
		EUIAction *action;

		action = e_shell_view_get_action (shell_view, "search-clear");
		e_ui_action_set_sensitive (action, sensitive);

		action = e_shell_view_get_action (shell_view, "search-save");
		e_ui_action_set_visible (action, sensitive && e_shell_view_get_search_rule (shell_view) != NULL);
	}
}

static void
shell_searchbar_clear_search_cb (EShellView *shell_view,
                                 EShellSearchbar *searchbar)
{
	EUIAction *search_option;
	GVariant *state;
	gint current_value;

	e_shell_searchbar_set_search_text (searchbar, NULL);

	search_option = e_shell_searchbar_get_search_option (searchbar);
	if (search_option == NULL)
		return;

	state = g_action_get_state (G_ACTION (search_option));
	current_value = state ? g_variant_get_int32 (state) : -1;
	g_clear_pointer (&state, g_variant_unref);

	/* Reset the search option if it's set to advanced search. */
	if (current_value == SEARCH_OPTION_ADVANCED)
		e_ui_action_set_state (search_option, g_variant_new_int32 (0));
}

static void
shell_searchbar_custom_search_cb (EShellView *shell_view,
                                  EFilterRule *custom_rule,
                                  EShellSearchbar *searchbar)
{
	EUIAction *search_option;
	gint value = SEARCH_OPTION_ADVANCED;

	e_shell_searchbar_set_search_text (searchbar, NULL);

	search_option = e_shell_searchbar_get_search_option (searchbar);
	if (search_option != NULL)
		e_ui_action_set_state (search_option, g_variant_new_int32 (value));
}

static void
shell_searchbar_execute_search_cb (EShellView *shell_view,
                                   EShellSearchbar *searchbar)
{
	EShellContent *shell_content;
	const gchar *search_text;

	search_text = e_shell_searchbar_get_search_text (searchbar);

	g_clear_pointer (&searchbar->priv->active_search_text, g_free);
	if (search_text && *search_text)
		searchbar->priv->active_search_text = g_strdup (search_text);

	shell_searchbar_update_search_widgets (searchbar);

	e_shell_searchbar_save_state (searchbar);

	if (!e_shell_view_is_active (shell_view))
		return;

	/* Direct the focus away from the search entry, so that a
	 * focus-in event is required before the text can be changed.
	 * This will reset the entry to the appropriate visual state. */
	if (gtk_widget_is_focus (searchbar->priv->search_entry)) {
		shell_content = e_shell_view_get_shell_content (shell_view);
		e_shell_content_focus_search_results (shell_content);
	}
}

static void
shell_searchbar_filter_changed_cb (GtkComboBox *filter_combo_box,
                                   EShellSearchbar *searchbar)
{
	EShellView *shell_view;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	e_shell_view_execute_search (shell_view);
}

static void
shell_searchbar_entry_activate_cb (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	EUIAction *action;
	const gchar *search_text;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	search_text = e_shell_searchbar_get_search_text (searchbar);
	if (search_text != NULL && *search_text != '\0')
		action = e_shell_view_get_action (shell_view, "search-quick");
	else
		action = e_shell_view_get_action (shell_view, "search-clear");

	g_action_activate (G_ACTION (action), NULL);
}

static void
shell_searchbar_entry_changed_cb (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	const gchar *search_text;
	gboolean sensitive;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	search_text = e_shell_searchbar_get_search_text (searchbar);
	sensitive = (search_text != NULL && *search_text != '\0');

	if (e_shell_view_is_active (shell_view)) {
		EUIAction *action;

		action = e_shell_view_get_action (shell_view, "search-quick");
		e_ui_action_set_sensitive (action, sensitive);

		action = e_shell_view_get_action (shell_view, "search-clear");
		e_ui_action_set_sensitive (action, sensitive ||
			(searchbar->priv->active_search_text && *searchbar->priv->active_search_text) ||
			e_shell_view_get_search_rule (shell_view) != NULL);
	}
}

static void
e_shell_searchbar_scope_changed_cb (EShellSearchbar *searchbar)
{
	if (gtk_widget_is_visible (searchbar->priv->scope_combo_box)) {
		EShellView *shell_view;

		shell_view = e_shell_searchbar_get_shell_view (searchbar);

		e_shell_view_execute_search (shell_view);
	}
}

static void
shell_searchbar_entry_icon_press_cb (EShellSearchbar *searchbar,
                                     GtkEntryIconPosition icon_pos,
                                     GdkEvent *event)
{
	EShellView *shell_view;
	EUIAction *action;

	/* Show the search options menu when the icon is pressed. */

	if (icon_pos != GTK_ENTRY_ICON_PRIMARY)
		return;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	action = e_shell_view_get_action (shell_view, "search-options");
	g_action_activate (G_ACTION (action), NULL);
}

static void
shell_searchbar_entry_icon_release_cb (EShellSearchbar *searchbar,
                                       GtkEntryIconPosition icon_pos,
                                       GdkEvent *event)
{
	EShellView *shell_view;
	EUIAction *action;

	/* Clear the search when the icon is pressed and released. */

	if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
		return;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	action = e_shell_view_get_action (shell_view, "search-clear");
	g_action_activate (G_ACTION (action), NULL);
}

static gboolean
shell_searchbar_entry_key_press_cb (EShellSearchbar *searchbar,
                                    GdkEventKey *key_event,
                                    GtkWindow *entry)
{
	EShellView *shell_view;
	EUIAction *action;
	guint mask;

	mask = gtk_accelerator_get_default_mod_mask ();
	if ((key_event->state & mask) != GDK_MOD1_MASK)
		return FALSE;

	if (key_event->keyval != GDK_KEY_Down)
		return FALSE;

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	action = e_shell_view_get_action (shell_view, "search-options");
	g_action_activate (G_ACTION (action), NULL);

	return TRUE;
}

static void
shell_searchbar_option_notify_state_cb (EUIAction *action,
					GParamSpec *param,
					EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	EUIAction *current;
	const gchar *search_text;
	const gchar *label;
	GVariant *state;
	gint current_value;

	state = g_action_get_state (G_ACTION (action));
	current_value = state ? g_variant_get_int32 (state) : -1;
	g_clear_pointer (&state, g_variant_unref);

	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	current = shell_searchbar_radio_action_get_current_action (action);

	label = e_ui_action_get_label (current);
	e_shell_searchbar_set_search_hint (searchbar, label);

	search_text = e_shell_searchbar_get_search_text (searchbar);

	if (current_value != SEARCH_OPTION_ADVANCED) {
		e_shell_view_set_search_rule (shell_view, NULL);
		e_shell_searchbar_set_search_text (searchbar, search_text);

		if (search_text != NULL && *search_text != '\0')
			e_shell_view_execute_search (shell_view);

		shell_searchbar_save_search_option (searchbar);
	} else if (search_text != NULL)
		e_shell_searchbar_set_search_text (searchbar, NULL);
}

static gboolean
shell_searchbar_entry_focus_in_cb (GtkWidget *entry,
                                   GdkEvent *event,
                                   EShellSearchbar *searchbar)
{
	/* to not change background when user changes search entry content */
	gtk_widget_set_name (entry, "searchbar_searchentry");

	return FALSE;
}

static gboolean
shell_searchbar_entry_focus_out_cb (GtkWidget *entry,
                                    GdkEvent *event,
                                    EShellSearchbar *searchbar)
{
	if (e_util_strcmp0 (searchbar->priv->active_search_text, gtk_entry_get_text (GTK_ENTRY (searchbar->priv->search_entry))) != 0) {
		gtk_entry_set_text (GTK_ENTRY (searchbar->priv->search_entry), searchbar->priv->active_search_text ?
			searchbar->priv->active_search_text : "");
	}

	shell_searchbar_update_search_widgets (searchbar);

	return FALSE;
}

static void
shell_searchbar_set_shell_view (EShellSearchbar *searchbar,
                                EShellView *shell_view)
{
	g_return_if_fail (searchbar->priv->shell_view == NULL);

	searchbar->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&searchbar->priv->shell_view);
}

static void
shell_searchbar_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_VISIBLE:
			e_shell_searchbar_set_filter_visible (
				E_SHELL_SEARCHBAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SEARCH_HINT:
			e_shell_searchbar_set_search_hint (
				E_SHELL_SEARCHBAR (object),
				g_value_get_string (value));
			return;

		case PROP_SEARCH_OPTION:
			e_shell_searchbar_set_search_option (
				E_SHELL_SEARCHBAR (object),
				g_value_get_object (value));
			return;

		case PROP_SEARCH_TEXT:
			e_shell_searchbar_set_search_text (
				E_SHELL_SEARCHBAR (object),
				g_value_get_string (value));
			return;

		case PROP_SCOPE_VISIBLE:
			e_shell_searchbar_set_scope_visible (
				E_SHELL_SEARCHBAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHELL_VIEW:
			shell_searchbar_set_shell_view (
				E_SHELL_SEARCHBAR (object),
				g_value_get_object (value));
			return;

		case PROP_STATE_GROUP:
			e_shell_searchbar_set_state_group (
				E_SHELL_SEARCHBAR (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_searchbar_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_COMBO_BOX:
			g_value_set_object (
				value, e_shell_searchbar_get_filter_combo_box (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_FILTER_VISIBLE:
			g_value_set_boolean (
				value, e_shell_searchbar_get_filter_visible (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SEARCH_HINT:
			g_value_set_string (
				value, e_shell_searchbar_get_search_hint (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SEARCH_OPTION:
			g_value_set_object (
				value, e_shell_searchbar_get_search_option (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SEARCH_TEXT:
			g_value_set_string (
				value, e_shell_searchbar_get_search_text (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SCOPE_COMBO_BOX:
			g_value_set_object (
				value, e_shell_searchbar_get_scope_combo_box (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SCOPE_VISIBLE:
			g_value_set_boolean (
				value, e_shell_searchbar_get_scope_visible (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_shell_searchbar_get_shell_view (
				E_SHELL_SEARCHBAR (object)));
			return;

		case PROP_STATE_GROUP:
			g_value_set_string (
				value, e_shell_searchbar_get_state_group (
				E_SHELL_SEARCHBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_searchbar_dispose (GObject *object)
{
	EShellSearchbar *self = E_SHELL_SEARCHBAR (object);

	if (self->priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->shell_view), &self->priv->shell_view);
		self->priv->shell_view = NULL;
	}

	if (self->priv->search_option != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->search_option, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->search_option);
	}

	g_clear_object (&self->priv->css_provider);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_searchbar_parent_class)->dispose (object);
}

static void
shell_searchbar_finalize (GObject *object)
{
	EShellSearchbar *self = E_SHELL_SEARCHBAR (object);

	g_free (self->priv->state_group);
	g_free (self->priv->active_search_text);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_searchbar_parent_class)->finalize (object);
}

static void
shell_searchbar_constructed (GObject *object)
{
	EShellView *shell_view;
	EShellSearchbar *searchbar;
	GtkSizeGroup *size_group;
	GtkWidget *widget;

	searchbar = E_SHELL_SEARCHBAR (object);
	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	size_group = e_shell_view_get_size_group (shell_view);

	g_signal_connect (
		shell_view, "clear-search",
		G_CALLBACK (shell_searchbar_clear_search_cb), searchbar);

	g_signal_connect (
		shell_view, "custom-search",
		G_CALLBACK (shell_searchbar_custom_search_cb), searchbar);

	g_signal_connect (
		shell_view, "execute-search",
		G_CALLBACK (shell_searchbar_execute_search_cb), searchbar);

	widget = searchbar->priv->filter_combo_box;

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_shell_searchbar_set_state_dirty), searchbar);

	/* Use G_CONNECT_AFTER here so the EActionComboBox has a
	 * chance to update its radio actions before we go sifting
	 * through the radio group for the current action. */
	g_signal_connect_after (
		widget, "changed",
		G_CALLBACK (shell_searchbar_filter_changed_cb), searchbar);

	searchbar->priv->css_provider = gtk_css_provider_new ();
	widget = searchbar->priv->search_entry;
	gtk_style_context_add_provider (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (searchbar->priv->css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	widget = GTK_WIDGET (searchbar);
	gtk_size_group_add_widget (size_group, widget);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_searchbar_parent_class)->constructed (object);
}

static void
shell_searchbar_map (GtkWidget *widget)
{
	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_shell_searchbar_parent_class)->map (widget);

	/* Load state after constructed() so we don't derail
	 * subclass initialization.  We wait until map() so we
	 * have usable style colors for the entry box. */
	e_shell_searchbar_load_state (E_SHELL_SEARCHBAR (widget));
}

static void
e_shell_searchbar_class_init (EShellSearchbarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_searchbar_set_property;
	object_class->get_property = shell_searchbar_get_property;
	object_class->dispose = shell_searchbar_dispose;
	object_class->finalize = shell_searchbar_finalize;
	object_class->constructed = shell_searchbar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = shell_searchbar_map;

	g_object_class_install_property (
		object_class,
		PROP_FILTER_COMBO_BOX,
		g_param_spec_object (
			"filter-combo-box",
			NULL,
			NULL,
			E_TYPE_ACTION_COMBO_BOX,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_VISIBLE,
		g_param_spec_boolean (
			"filter-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_HINT,
		g_param_spec_string (
			"search-hint",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_OPTION,
		g_param_spec_object (
			"search-option",
			NULL,
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_TEXT,
		g_param_spec_string (
			"search-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_COMBO_BOX,
		g_param_spec_object (
			"scope-combo-box",
			NULL,
			NULL,
			E_TYPE_ACTION_COMBO_BOX,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_VISIBLE,
		g_param_spec_boolean (
			"scope-visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSearchbar:shell-view
	 *
	 * The #EShellView to which the searchbar widget belongs.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			NULL,
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSearchbar:state-group
	 *
	 * Key file group name to read and write search bar state.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_STATE_GROUP,
		g_param_spec_string (
			"state-group",
			NULL,
			NULL,
			STATE_GROUP_DEFAULT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_shell_searchbar_init (EShellSearchbar *searchbar)
{
	GtkBox *box;
	GtkLabel *label;
	GtkWidget *widget;

	searchbar->priv = e_shell_searchbar_get_instance_private (searchbar);

	gtk_box_set_spacing (GTK_BOX (searchbar), 6);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (searchbar)),
				     "header-box");

	/* Filter Combo Widgets */

	box = GTK_BOX (searchbar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		searchbar, "filter-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	box = GTK_BOX (widget);

	/* Translators: The "Show:" label precedes a combo box that
	 * allows the user to filter the current view.  Examples of
	 * items that appear in the combo box are "Unread Messages",
	 * "Important Messages", or "Active Appointments". */
	widget = gtk_label_new_with_mnemonic (_("Sho_w:"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	e_action_combo_box_set_ellipsize_enabled (E_ACTION_COMBO_BOX (widget), TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	searchbar->priv->filter_combo_box = widget;
	gtk_widget_show (widget);

	/* Search Entry Widgets */

	box = GTK_BOX (searchbar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("Sear_ch:"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	searchbar->priv->search_entry = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (shell_searchbar_entry_activate_cb),
		searchbar);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (shell_searchbar_entry_changed_cb),
		searchbar);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_shell_searchbar_set_state_dirty),
		searchbar);

	g_signal_connect_swapped (
		widget, "icon-press",
		G_CALLBACK (shell_searchbar_entry_icon_press_cb),
		searchbar);

	g_signal_connect_swapped (
		widget, "icon-release",
		G_CALLBACK (shell_searchbar_entry_icon_release_cb),
		searchbar);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (shell_searchbar_entry_key_press_cb),
		searchbar);

	g_signal_connect (
		widget, "focus-in-event",
		G_CALLBACK (shell_searchbar_entry_focus_in_cb),
		searchbar);

	g_signal_connect (
		widget, "focus-out-event",
		G_CALLBACK (shell_searchbar_entry_focus_out_cb),
		searchbar);

	/* Scope Combo Widgets */

	box = GTK_BOX (searchbar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		searchbar, "scope-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	box = GTK_BOX (widget);

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("i_n"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	e_action_combo_box_set_ellipsize_enabled (E_ACTION_COMBO_BOX (widget), TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	searchbar->priv->scope_combo_box = widget;
	gtk_widget_show (widget);

	g_signal_connect_object (
		widget, "changed",
		G_CALLBACK (e_shell_searchbar_scope_changed_cb),
		searchbar, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

/**
 * e_shell_searchbar_new:
 * @shell_view: an #EShellView
 *
 * Creates a new #EShellSearchbar instance.
 *
 * Returns: a new #EShellSearchbar instance
 **/
GtkWidget *
e_shell_searchbar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_SHELL_SEARCHBAR,
		"shell-view", shell_view,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		NULL);
}

/**
 * e_shell_searchbar_get_shell_view:
 * @searchbar: an #EShellSearchbar
 *
 * Returns the #EShellView that was passed to e_shell_searchbar_new().
 *
 * Returns: the #EShellView to which @searchbar belongs
 **/
EShellView *
e_shell_searchbar_get_shell_view (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	return E_SHELL_VIEW (searchbar->priv->shell_view);
}

void
e_shell_searchbar_init_ui_data (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	EUIAction *action;

	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	shell_view = e_shell_searchbar_get_shell_view (searchbar);

	action = e_shell_view_get_action (shell_view, "search-clear");

	e_binding_bind_property (
		action, "sensitive",
		searchbar->priv->search_entry, "secondary-icon-sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		action, "icon-name",
		searchbar->priv->search_entry, "secondary-icon-name",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		action, "tooltip",
		searchbar->priv->search_entry, "secondary-icon-tooltip-text",
		G_BINDING_SYNC_CREATE);

	action = e_shell_view_get_action (shell_view, "search-options");

	e_binding_bind_property (
		action, "sensitive",
		searchbar->priv->search_entry, "primary-icon-sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		action, "icon-name",
		searchbar->priv->search_entry, "primary-icon-name",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		action, "tooltip",
		searchbar->priv->search_entry, "primary-icon-tooltip-text",
		G_BINDING_SYNC_CREATE);
}

EActionComboBox *
e_shell_searchbar_get_filter_combo_box (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	return E_ACTION_COMBO_BOX (searchbar->priv->filter_combo_box);
}

gboolean
e_shell_searchbar_get_filter_visible (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), FALSE);

	return searchbar->priv->filter_visible;
}

void
e_shell_searchbar_set_filter_visible (EShellSearchbar *searchbar,
                                      gboolean filter_visible)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	if (searchbar->priv->filter_visible == filter_visible)
		return;

	searchbar->priv->filter_visible = filter_visible;

	/* If we're hiding the filter combo box, reset it to its
	 * first item so that no content gets permanently hidden. */
	if (!filter_visible) {
		EActionComboBox *combo_box;

		combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
	}

	g_object_notify (G_OBJECT (searchbar), "filter-visible");
}

const gchar *
e_shell_searchbar_get_search_hint (EShellSearchbar *searchbar)
{
	GtkEntry *entry;

	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	entry = GTK_ENTRY (searchbar->priv->search_entry);

	return gtk_entry_get_placeholder_text (entry);
}

void
e_shell_searchbar_set_search_hint (EShellSearchbar *searchbar,
                                   const gchar *search_hint)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	entry = GTK_ENTRY (searchbar->priv->search_entry);

	if (g_strcmp0 (gtk_entry_get_placeholder_text (entry), search_hint) == 0)
		return;

	gtk_entry_set_placeholder_text (entry, search_hint);

	g_object_notify (G_OBJECT (searchbar), "search-hint");
}

EUIAction *
e_shell_searchbar_get_search_option (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	return searchbar->priv->search_option;
}

void
e_shell_searchbar_set_search_option (EShellSearchbar *searchbar,
				     EUIAction *search_option)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	if (searchbar->priv->search_option == search_option)
		return;

	if (search_option != NULL) {
		g_return_if_fail (E_IS_UI_ACTION (search_option));
		g_object_ref (search_option);
	}

	if (searchbar->priv->search_option != NULL) {
		g_signal_handlers_disconnect_matched (
			searchbar->priv->search_option,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			searchbar);
		g_object_unref (searchbar->priv->search_option);
	}

	searchbar->priv->search_option = search_option;

	if (search_option != NULL) {
		g_signal_connect (
			search_option, "notify::state",
			G_CALLBACK (shell_searchbar_option_notify_state_cb),
			searchbar);

		shell_searchbar_option_notify_state_cb (search_option, NULL, searchbar);
	}

	g_object_notify (G_OBJECT (searchbar), "search-option");
}

const gchar *
e_shell_searchbar_get_search_text (EShellSearchbar *searchbar)
{
	GtkEntry *entry;

	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	entry = GTK_ENTRY (searchbar->priv->search_entry);

	return gtk_entry_get_text (entry);
}

void
e_shell_searchbar_set_search_text (EShellSearchbar *searchbar,
                                   const gchar *search_text)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	entry = GTK_ENTRY (searchbar->priv->search_entry);

	/* XXX Really wish gtk_entry_set_text()
	 *     would just learn to accept NULL. */
	if (search_text == NULL)
		search_text = "";

	if (g_strcmp0 (gtk_entry_get_text (entry), search_text) == 0)
		return;

	gtk_entry_set_text (entry, search_text);

	shell_searchbar_update_search_widgets (searchbar);

	g_object_notify (G_OBJECT (searchbar), "search-text");
}

GtkWidget *
e_shell_searchbar_get_search_box (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (searchbar != NULL, NULL);
	g_return_val_if_fail (searchbar->priv != NULL, NULL);
	g_return_val_if_fail (searchbar->priv->search_entry != NULL, NULL);

	return gtk_widget_get_parent (searchbar->priv->search_entry);
}

EActionComboBox *
e_shell_searchbar_get_scope_combo_box (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	return E_ACTION_COMBO_BOX (searchbar->priv->scope_combo_box);
}

gboolean
e_shell_searchbar_get_scope_visible (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), FALSE);

	return searchbar->priv->scope_visible;
}

void
e_shell_searchbar_set_scope_visible (EShellSearchbar *searchbar,
                                     gboolean scope_visible)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	if (searchbar->priv->scope_visible == scope_visible)
		return;

	searchbar->priv->scope_visible = scope_visible;

	if (searchbar->priv->scope_visible) {
		/* Use G_CONNECT_AFTER here so the EActionComboBox has a
		 * chance to update its radio actions before we go sifting
		 * through the radio group for the current action. */
		g_signal_connect_data (
			searchbar->priv->scope_combo_box, "changed",
			G_CALLBACK (shell_searchbar_save_search_scope),
			searchbar, (GClosureNotify) NULL,
			G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	} else {
		g_signal_handlers_disconnect_by_func (searchbar->priv->scope_combo_box,
			G_CALLBACK (shell_searchbar_save_search_scope), searchbar);
	}

	g_object_notify (G_OBJECT (searchbar), "scope-visible");
}

void
e_shell_searchbar_set_state_dirty (EShellSearchbar *searchbar)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	searchbar->priv->state_dirty = TRUE;
}

const gchar *
e_shell_searchbar_get_state_group (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), NULL);

	return searchbar->priv->state_group;
}

void
e_shell_searchbar_set_state_group (EShellSearchbar *searchbar,
                                   const gchar *state_group)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	if (state_group == NULL)
		state_group = STATE_GROUP_DEFAULT;

	if (g_strcmp0 (searchbar->priv->state_group, state_group) == 0)
		return;

	g_free (searchbar->priv->state_group);
	searchbar->priv->state_group = g_strdup (state_group);

	g_object_notify (G_OBJECT (searchbar), "state-group");
}

static gboolean
idle_execute_search (gpointer shell_view)
{
	e_shell_view_execute_search (shell_view);
	g_object_unref (shell_view);
	return FALSE;
}

void
e_shell_searchbar_load_state (EShellSearchbar *searchbar)
{
	EShellView *shell_view;
	GKeyFile *key_file;
	EUIAction *action;
	GtkWidget *widget;
	const gchar *search_text;
	const gchar *state_group;
	const gchar *key;
	gchar *string;
	gint value;

	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	shell_view = e_shell_searchbar_get_shell_view (searchbar);
	state_group = e_shell_searchbar_get_state_group (searchbar);
	g_return_if_fail (state_group != NULL);

	key_file = e_shell_view_get_state_key_file (shell_view);

	/* Changing the combo boxes triggers searches, so block
	 * the search action until the state is fully restored. */
	e_shell_view_block_execute_search (shell_view);

	e_shell_view_set_search_rule (shell_view, NULL);

	key = STATE_KEY_SEARCH_FILTER;
	string = g_key_file_get_string (key_file, state_group, key, NULL);
	if (string != NULL && *string != '\0')
		action = e_shell_view_get_action (shell_view, string);
	else
		action = NULL;
	if (action) {
		e_ui_action_set_active (action, TRUE);
	} else {
		/* Pick the first combo box item. */
		widget = searchbar->priv->filter_combo_box;
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	g_free (string);

	/* Avoid restoring to the "Advanced Search" option, since we
	 * don't currently save the search rule (TODO but we should). */
	key = STATE_KEY_SEARCH_OPTION;
	string = g_key_file_get_string (key_file, state_group, key, NULL);
	if (string != NULL && *string != '\0')
		action = e_shell_view_get_action (shell_view, string);
	else
		action = NULL;
	value = SEARCH_OPTION_ADVANCED;
	if (action) {
		GVariant *target;

		target = e_ui_action_ref_target (action);
		if (target)
			value = g_variant_get_int32 (target);
		g_clear_pointer (&target, g_variant_unref);
	} else {
		value = SEARCH_OPTION_ADVANCED;
	}
	if (value != SEARCH_OPTION_ADVANCED)
		e_ui_action_set_active (action, TRUE);
	else if (searchbar->priv->search_option != NULL)
		e_ui_action_set_state (searchbar->priv->search_option, g_variant_new_int32 (0));
	g_free (string);

	key = STATE_KEY_SEARCH_TEXT;
	string = g_key_file_get_string (key_file, state_group, key, NULL);
	search_text = e_shell_searchbar_get_search_text (searchbar);
	if (search_text != NULL && *search_text == '\0')
		search_text = NULL;
	if (g_strcmp0 (string, search_text) != 0)
		e_shell_searchbar_set_search_text (searchbar, string);
	g_free (string);

	key = STATE_KEY_SEARCH_SCOPE;
	string = g_key_file_get_string (key_file, state_group, key, NULL);
	if (string != NULL && *string != '\0')
		action = e_shell_view_get_action (shell_view, string);
	else
		action = NULL;
	if (action) {
		e_ui_action_set_active (action, TRUE);
	} else {
		/* Pick the first combo box item. */
		widget = searchbar->priv->scope_combo_box;
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	g_free (string);

	e_shell_view_unblock_execute_search (shell_view);

	/* Execute the search when we have time. */

	searchbar->priv->state_dirty = FALSE;

	if (!e_shell_view_is_execute_search_blocked (shell_view)) {
		/* Prioritize ahead of GTK+ redraws. */
		g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			idle_execute_search, g_object_ref (shell_view), NULL);
	}
}

void
e_shell_searchbar_save_state (EShellSearchbar *searchbar)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));

	/* Skip saving state if it hasn't changed since it was loaded. */
	if (!searchbar->priv->state_dirty)
		return;

	shell_searchbar_save_search_filter (searchbar);

	shell_searchbar_save_search_option (searchbar);

	shell_searchbar_save_search_text (searchbar);

	shell_searchbar_save_search_scope (searchbar);

	searchbar->priv->state_dirty = FALSE;
}

void
e_shell_searchbar_search_entry_grab_focus (EShellSearchbar *searchbar)
{
	g_return_if_fail (E_IS_SHELL_SEARCHBAR (searchbar));
	g_return_if_fail (searchbar->priv->search_entry);

	gtk_widget_grab_focus (searchbar->priv->search_entry);
}

gboolean
e_shell_searchbar_search_entry_has_focus (EShellSearchbar *searchbar)
{
	g_return_val_if_fail (E_IS_SHELL_SEARCHBAR (searchbar), FALSE);
	g_return_val_if_fail (searchbar->priv->search_entry, FALSE);

	return gtk_widget_has_focus (searchbar->priv->search_entry);
}
