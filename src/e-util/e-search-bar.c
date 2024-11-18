/*
 * e-search-bar.c
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

#include "evolution-config.h"

#include "e-search-bar.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-dialog-widgets.h"
#include "e-misc-utils.h"

struct _ESearchBarPrivate {
	EWebView *web_view;
	GtkWidget *hide_button;
	GtkWidget *entry;
	GtkWidget *case_sensitive_button;
	GtkWidget *wrapped_next_box;
	GtkWidget *wrapped_prev_box;
	GtkWidget *matches_label;
	GtkWidget *prev_button;
	GtkWidget *next_button;

	WebKitFindController *find_controller;

	gchar *active_search;

	gboolean search_forward;
	gboolean can_hide;
};

enum {
	PROP_0,
	PROP_ACTIVE_SEARCH,
	PROP_CASE_SENSITIVE,
	PROP_CAN_HIDE,
	PROP_TEXT,
	PROP_WEB_VIEW
};

enum {
	CHANGED,
	CLEAR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ESearchBar, e_search_bar, GTK_TYPE_BOX)

static void
search_bar_update_matches (ESearchBar *search_bar,
                           guint matches)
{
	GtkWidget *matches_label;

	matches_label = search_bar->priv->matches_label;

	if (!matches) {
		gtk_label_set_text (GTK_LABEL (matches_label), _("No matches"));
	} else {
		gchar *text;

		/* Translators: The '%u' is the actual number of the matches found */
		text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%u match", "%u matches", matches), matches);
		gtk_label_set_text (GTK_LABEL (matches_label), text);
		g_free (text);
	}

	gtk_widget_show (matches_label);
}

 static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      ESearchBar *search_bar)
{
	GtkWidget *widget;
	WebKitFindOptions options;
	gboolean wrapped = FALSE;

	search_bar_update_matches (search_bar, match_count);

	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search =
		g_strdup (webkit_find_controller_get_search_text (find_controller));

	gtk_widget_set_sensitive (search_bar->priv->next_button, TRUE);
	gtk_widget_set_sensitive (search_bar->priv->prev_button, TRUE);

	g_object_notify (G_OBJECT (search_bar), "active-search");

	options = webkit_find_controller_get_options (find_controller);

	if (options & WEBKIT_FIND_OPTIONS_WRAP_AROUND)
		wrapped = TRUE;

	/* Update wrapped label visibility. */
	widget = search_bar->priv->wrapped_next_box;

	if (wrapped && search_bar->priv->search_forward)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);

	widget = search_bar->priv->wrapped_prev_box;

	if (wrapped && !search_bar->priv->search_forward)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);
}

static void
webkit_find_controller_failed_to_found_text_cb (WebKitFindController *find_controller,
                                                ESearchBar *search_bar)
{
	WebKitFindOptions options;
	GtkWidget *widget;

	options = webkit_find_controller_get_options (find_controller);

	/* If we didn't find anything, try from the beggining with WRAP_AROUND option */
	if (!(options & WEBKIT_FIND_OPTIONS_WRAP_AROUND)) {
		webkit_find_controller_search (
			find_controller,
			webkit_find_controller_get_search_text (find_controller),
			options | WEBKIT_FIND_OPTIONS_WRAP_AROUND,
			G_MAXUINT);
	}

	search_bar_update_matches (search_bar, 0);

	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search =
		g_strdup (webkit_find_controller_get_search_text (find_controller));

	gtk_widget_set_sensitive (search_bar->priv->next_button, FALSE);
	gtk_widget_set_sensitive (search_bar->priv->prev_button, FALSE);

	g_object_notify (G_OBJECT (search_bar), "active-search");

	/* Update wrapped label visibility. */
	widget = search_bar->priv->wrapped_next_box;
	gtk_widget_hide (widget);

	widget = search_bar->priv->wrapped_prev_box;
	gtk_widget_hide (widget);
}

static void
search_bar_update_highlights (ESearchBar *search_bar)
{
	webkit_find_controller_search_finish (search_bar->priv->find_controller);

	e_search_bar_changed (search_bar);
}

static void
search_bar_find (ESearchBar *search_bar,
                 gboolean search_forward)
{
	WebKitFindController *find_controller;
	gboolean case_sensitive;
	gchar *text;

	find_controller = search_bar->priv->find_controller;
	search_bar->priv->search_forward = search_forward;

	case_sensitive = e_search_bar_get_case_sensitive (search_bar);
	text = e_search_bar_get_text (search_bar);

	if (text == NULL || *text == '\0') {
		e_search_bar_clear (search_bar);
		g_free (text);
		return;
	}

	if (g_strcmp0 (webkit_find_controller_get_search_text (find_controller), text) == 0 &&
	    ((webkit_find_controller_get_options (find_controller) & WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE) != 0 ? 1 : 0) ==
	    (case_sensitive ? 1 : 0)) {
		if (search_forward)
			webkit_find_controller_search_next (find_controller);
		else
			webkit_find_controller_search_previous (find_controller);
	} else {
		webkit_find_controller_search_finish (find_controller);
		webkit_find_controller_search (
			find_controller,
			text,
			(case_sensitive ? WEBKIT_FIND_OPTIONS_NONE : WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE) |
			(search_forward ? 0 : WEBKIT_FIND_OPTIONS_BACKWARDS),
			G_MAXUINT);
	}

	g_free (text);
}

static void
search_bar_changed_cb (ESearchBar *search_bar)
{
	gtk_widget_set_sensitive (search_bar->priv->next_button, TRUE);
	gtk_widget_set_sensitive (search_bar->priv->prev_button, TRUE);

	g_object_notify (G_OBJECT (search_bar), "text");
}

static void
search_bar_find_next_cb (ESearchBar *search_bar)
{
	search_bar_find (search_bar, TRUE);
}

static void
search_bar_find_previous_cb (ESearchBar *search_bar)
{
	search_bar_find (search_bar, FALSE);
}

static void
search_bar_icon_release_cb (ESearchBar *search_bar,
                            GtkEntryIconPosition icon_pos,
                            GdkEvent *event)
{
	g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

	e_search_bar_clear (search_bar);
	gtk_widget_grab_focus (search_bar->priv->entry);
}

static void
search_bar_toggled_cb (ESearchBar *search_bar)
{
	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search = NULL;

	g_object_notify (G_OBJECT (search_bar), "active-search");
	g_object_notify (G_OBJECT (search_bar), "case-sensitive");
}

static void
web_view_load_changed_cb (WebKitWebView *webkit_web_view,
                          WebKitLoadEvent load_event,
                          ESearchBar *search_bar)
{
	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	if (gtk_widget_is_visible (GTK_WIDGET (search_bar))) {
		if (search_bar->priv->active_search != NULL) {
			e_web_view_disable_highlights (search_bar->priv->web_view);
			search_bar_find (search_bar, TRUE);
		}
	} else {
		e_web_view_update_highlights (search_bar->priv->web_view);
	}
}

static void
search_bar_set_web_view (ESearchBar *search_bar,
                         EWebView *web_view)
{
	WebKitFindController *find_controller;

	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (search_bar->priv->web_view == NULL);

	search_bar->priv->web_view = g_object_ref (web_view);

	find_controller =
		webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (web_view));

	search_bar->priv->find_controller = find_controller;

	g_signal_connect (
		web_view, "load-changed",
		G_CALLBACK (web_view_load_changed_cb), search_bar);

	g_signal_connect (
		find_controller, "found-text",
		G_CALLBACK (webkit_find_controller_found_text_cb), search_bar);

	g_signal_connect (
		find_controller, "failed-to-find-text",
		G_CALLBACK (webkit_find_controller_failed_to_found_text_cb), search_bar);
}

static void
search_bar_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_HIDE:
			e_search_bar_set_can_hide (
				E_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_CASE_SENSITIVE:
			e_search_bar_set_case_sensitive (
				E_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TEXT:
			e_search_bar_set_text (
				E_SEARCH_BAR (object),
				g_value_get_string (value));
			return;

		case PROP_WEB_VIEW:
			search_bar_set_web_view (
				E_SEARCH_BAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
search_bar_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_SEARCH:
			g_value_set_boolean (
				value, e_search_bar_get_active_search (
				E_SEARCH_BAR (object)));
			return;

		case PROP_CAN_HIDE:
			g_value_set_boolean (
				value, e_search_bar_get_can_hide (
				E_SEARCH_BAR (object)));
			return;

		case PROP_CASE_SENSITIVE:
			g_value_set_boolean (
				value, e_search_bar_get_case_sensitive (
				E_SEARCH_BAR (object)));
			return;

		case PROP_TEXT:
			g_value_take_string (
				value, e_search_bar_get_text (
				E_SEARCH_BAR (object)));
			return;

		case PROP_WEB_VIEW:
			g_value_set_object (
				value, e_search_bar_get_web_view (
				E_SEARCH_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
search_bar_dispose (GObject *object)
{
	ESearchBar *self = E_SEARCH_BAR (object);

	if (self->priv->web_view) {
		g_signal_handlers_disconnect_by_data (self->priv->web_view, object);
		g_clear_object (&self->priv->web_view);
	}

	g_clear_object (&self->priv->hide_button);
	g_clear_object (&self->priv->entry);
	g_clear_object (&self->priv->case_sensitive_button);
	g_clear_object (&self->priv->prev_button);
	g_clear_object (&self->priv->next_button);
	g_clear_object (&self->priv->wrapped_next_box);
	g_clear_object (&self->priv->wrapped_prev_box);
	g_clear_object (&self->priv->matches_label);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_search_bar_parent_class)->dispose (object);
}

static void
search_bar_finalize (GObject *object)
{
	ESearchBar *self = E_SEARCH_BAR (object);

	g_free (self->priv->active_search);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_search_bar_parent_class)->finalize (object);
}

static void
search_bar_constructed (GObject *object)
{
	ESearchBar *self = E_SEARCH_BAR (object);

	e_binding_bind_property (
		object, "case-sensitive",
		self->priv->case_sensitive_button, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_search_bar_parent_class)->constructed (object);
}

static void
search_bar_show (GtkWidget *widget)
{
	ESearchBar *search_bar;

	search_bar = E_SEARCH_BAR (widget);

	/* Chain up to parent's show() method. */
	GTK_WIDGET_CLASS (e_search_bar_parent_class)->show (widget);

	gtk_widget_grab_focus (search_bar->priv->entry);

	webkit_find_controller_search_finish (search_bar->priv->find_controller);

	e_web_view_disable_highlights (search_bar->priv->web_view);
	search_bar_find (search_bar, TRUE);
}

static void
search_bar_hide (GtkWidget *widget)
{
	ESearchBar *search_bar;

	search_bar = E_SEARCH_BAR (widget);

	/* Chain up to parent's hide() method. */
	GTK_WIDGET_CLASS (e_search_bar_parent_class)->hide (widget);

	search_bar_update_highlights (search_bar);

	e_web_view_update_highlights (search_bar->priv->web_view);
}

static gboolean
search_bar_key_press_event (GtkWidget *widget,
                            GdkEventKey *event)
{
	GtkWidgetClass *widget_class;

	if (event->keyval == GDK_KEY_Escape &&
	    e_search_bar_get_can_hide (E_SEARCH_BAR (widget))) {
		gtk_widget_hide (widget);
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (e_search_bar_parent_class);
	return widget_class->key_press_event (widget, event);
}

static void
search_bar_clear (ESearchBar *search_bar)
{
	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search = NULL;

	gtk_entry_set_text (GTK_ENTRY (search_bar->priv->entry), "");

	gtk_widget_hide (search_bar->priv->wrapped_next_box);
	gtk_widget_hide (search_bar->priv->wrapped_prev_box);
	gtk_widget_hide (search_bar->priv->matches_label);

	search_bar_update_highlights (search_bar);

	g_object_notify (G_OBJECT (search_bar), "active-search");
}

static void
e_search_bar_class_init (ESearchBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = search_bar_set_property;
	object_class->get_property = search_bar_get_property;
	object_class->dispose = search_bar_dispose;
	object_class->finalize = search_bar_finalize;
	object_class->constructed = search_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = search_bar_show;
	widget_class->hide = search_bar_hide;
	widget_class->key_press_event = search_bar_key_press_event;

	class->clear = search_bar_clear;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_SEARCH,
		g_param_spec_boolean (
			"active-search",
			"Active Search",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_HIDE,
		g_param_spec_boolean (
			"can-hide",
			"Can Hide",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CASE_SENSITIVE,
		g_param_spec_boolean (
			"case-sensitive",
			"Case Sensitive",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			"Search Text",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEB_VIEW,
		g_param_spec_object (
			"web-view",
			"Web View",
			NULL,
			E_TYPE_WEB_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESearchBarClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CLEAR] = g_signal_new (
		"clear",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESearchBarClass, clear),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_search_bar_init (ESearchBar *search_bar)
{
	GtkWidget *label;
	GtkWidget *widget;
	GtkWidget *container;

	search_bar->priv = e_search_bar_get_instance_private (search_bar);
	search_bar->priv->can_hide = TRUE;

	gtk_box_set_spacing (GTK_BOX (search_bar), 12);
	gtk_container_set_border_width (GTK_CONTAINER (search_bar), 6);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (search_bar), GTK_ORIENTATION_HORIZONTAL);

	container = GTK_WIDGET (search_bar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"window-close", GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (widget, _("Close the find bar"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->hide_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_hide), search_bar);

	widget = gtk_label_new_with_mnemonic (_("Fin_d:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 3);
	gtk_widget_show (widget);

	label = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_icon_from_icon_name (
		GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY,
		"edit-clear");
	gtk_entry_set_icon_tooltip_text (
		GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY,
		_("Clear the search"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_widget_set_size_request (widget, 200, -1);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->entry = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		search_bar, "active-search",
		widget, "secondary-icon-sensitive",
		G_BINDING_SYNC_CREATE);

	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (search_bar_find_next_cb), search_bar);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (search_bar_changed_cb), search_bar);

	g_signal_connect_swapped (
		widget, "icon-release",
		G_CALLBACK (search_bar_icon_release_cb), search_bar);

	widget = e_dialog_button_new_with_icon ("go-previous", _("_Previous"));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the previous occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->prev_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (search_bar_find_previous_cb), search_bar);

	widget = e_dialog_button_new_with_icon ("go-next", _("_Next"));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the next occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->next_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (search_bar_find_next_cb), search_bar);

	widget = gtk_check_button_new_with_mnemonic (_("Mat_ch case"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->case_sensitive_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (search_bar_toggled_cb), search_bar);

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (search_bar_find_next_cb), search_bar);

	container = GTK_WIDGET (search_bar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	search_bar->priv->wrapped_next_box = g_object_ref (widget);
	gtk_widget_hide (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"wrapped", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("Reached bottom of page, continued from top"));
	gtk_label_set_ellipsize (
		GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = GTK_WIDGET (search_bar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	search_bar->priv->wrapped_prev_box = g_object_ref (widget);
	gtk_widget_hide (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"wrapped", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("Reached top of page, continued from bottom"));
	gtk_label_set_ellipsize (
		GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = GTK_WIDGET (search_bar);

	widget = gtk_label_new (NULL);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 12);
	search_bar->priv->matches_label = g_object_ref (widget);
	gtk_widget_show (widget);
}

GtkWidget *
e_search_bar_new (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return g_object_new (
		E_TYPE_SEARCH_BAR, "web-view", web_view, NULL);
}

void
e_search_bar_clear (ESearchBar *search_bar)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	g_signal_emit (search_bar, signals[CLEAR], 0);
}

void
e_search_bar_changed (ESearchBar *search_bar)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	g_signal_emit (search_bar, signals[CHANGED], 0);
}

EWebView *
e_search_bar_get_web_view (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->web_view;
}

gboolean
e_search_bar_get_active_search (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

	return (search_bar->priv->active_search != NULL);
}

gboolean
e_search_bar_get_case_sensitive (ESearchBar *search_bar)
{
	GtkToggleButton *button;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

	button = GTK_TOGGLE_BUTTON (search_bar->priv->case_sensitive_button);

	return gtk_toggle_button_get_active (button);
}

void
e_search_bar_set_case_sensitive (ESearchBar *search_bar,
                                 gboolean case_sensitive)
{
	GtkToggleButton *button;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	button = GTK_TOGGLE_BUTTON (search_bar->priv->case_sensitive_button);

	gtk_toggle_button_set_active (button, case_sensitive);

	g_object_notify (G_OBJECT (search_bar), "case-sensitive");
}

gchar *
e_search_bar_get_text (ESearchBar *search_bar)
{
	GtkEntry *entry;
	const gchar *text;

	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	entry = GTK_ENTRY (search_bar->priv->entry);
	text = gtk_entry_get_text (entry);

	return g_strstrip (g_strdup (text));
}

void
e_search_bar_set_text (ESearchBar *search_bar,
                       const gchar *text)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	entry = GTK_ENTRY (search_bar->priv->entry);

	if (text == NULL)
		text = "";

	/* This will trigger a "notify::text" signal. */
	gtk_entry_set_text (entry, text);
}

gboolean
e_search_bar_get_can_hide (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), FALSE);

	return search_bar->priv->can_hide;
}

void
e_search_bar_set_can_hide (ESearchBar *search_bar,
			   gboolean can_hide)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (!search_bar->priv->can_hide == !can_hide)
		return;

	search_bar->priv->can_hide = can_hide;

	gtk_widget_set_visible (search_bar->priv->hide_button, can_hide);
	if (!can_hide)
		gtk_widget_show (GTK_WIDGET (search_bar));

	g_object_notify (G_OBJECT (search_bar), "can-hide");
}

void
e_search_bar_focus_entry (ESearchBar *search_bar)
{
	g_return_if_fail (E_IS_SEARCH_BAR (search_bar));

	if (!gtk_widget_get_visible (GTK_WIDGET (search_bar)))
		return;

	gtk_widget_grab_focus (search_bar->priv->entry);
}
