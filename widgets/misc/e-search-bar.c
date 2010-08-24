/*
 * e-search-bar.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-search-bar.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtkhtml/gtkhtml-search.h>

#include "e-util/e-binding.h"

#define E_SEARCH_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SEARCH_BAR, ESearchBarPrivate))

struct _ESearchBarPrivate {
	EWebView *web_view;
	GtkWidget *entry;
	GtkWidget *case_sensitive_button;
	GtkWidget *wrapped_next_box;
	GtkWidget *wrapped_prev_box;
	GtkWidget *matches_label;

	ESearchingTokenizer *tokenizer;
	gchar *active_search;

	guint rerun_search : 1;
};

enum {
	PROP_0,
	PROP_ACTIVE_SEARCH,
	PROP_CASE_SENSITIVE,
	PROP_TEXT,
	PROP_WEB_VIEW
};

enum {
	CHANGED,
	CLEAR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ESearchBar,
	e_search_bar,
	GTK_TYPE_HBOX)

static void
search_bar_update_matches (ESearchBar *search_bar)
{
	ESearchingTokenizer *tokenizer;
	GtkWidget *matches_label;
	gint matches;
	gchar *text;

	tokenizer = e_search_bar_get_tokenizer (search_bar);
	matches_label = search_bar->priv->matches_label;

	matches = e_searching_tokenizer_match_count (tokenizer);
	text = g_strdup_printf (_("Matches: %d"), matches);

	gtk_label_set_text (GTK_LABEL (matches_label), text);
	gtk_widget_show (matches_label);

	g_free (text);
}

static void
search_bar_update_tokenizer (ESearchBar *search_bar)
{
	ESearchingTokenizer *tokenizer;
	gboolean case_sensitive;
	gchar *active_search;

	tokenizer = e_search_bar_get_tokenizer (search_bar);
	case_sensitive = e_search_bar_get_case_sensitive (search_bar);

	if (gtk_widget_get_visible (GTK_WIDGET (search_bar)))
		active_search = search_bar->priv->active_search;
	else
		active_search = NULL;

	e_searching_tokenizer_set_primary_case_sensitivity (
		tokenizer, case_sensitive);
	e_searching_tokenizer_set_primary_search_string (
		tokenizer, active_search);

	e_search_bar_changed (search_bar);
}

static void
search_bar_find (ESearchBar *search_bar,
                 gboolean search_forward)
{
	EWebView *web_view;
	GtkWidget *widget;
	gboolean case_sensitive;
	gboolean new_search;
	gboolean wrapped = FALSE;
	gchar *text;

	web_view = e_search_bar_get_web_view (search_bar);
	case_sensitive = e_search_bar_get_case_sensitive (search_bar);
	text = e_search_bar_get_text (search_bar);

	if (text == NULL || *text == '\0') {
		e_search_bar_clear (search_bar);
		g_free (text);
		return;
	}

	new_search =
		(search_bar->priv->active_search == NULL) ||
		(g_strcmp0 (text, search_bar->priv->active_search) != 0);

	/* XXX On a new search, the HTMLEngine's search state gets
	 *     destroyed when we redraw the message with highlighted
	 *     matches (EMHTMLStream's write() method triggers this,
	 *     but it's really GtkHtml's fault).  That's why the first
	 *     match isn't selected automatically.  It also causes
	 *     gtk_html_engine_search_next() to return FALSE, which we
	 *     assume to mean the search wrapped.
	 *
	 *     So to avoid mistakenly thinking the search wrapped when
	 *     it hasn't, we have to trap the first button click after a
	 *     search and re-run the search to recreate the HTMLEngine's
	 *     search state, so that gtk_html_engine_search_next() will
	 *     succeed. */
	if (new_search) {
		g_free (search_bar->priv->active_search);
		search_bar->priv->active_search = text;
		search_bar->priv->rerun_search = TRUE;
		search_bar_update_tokenizer (search_bar);
	} else if (search_bar->priv->rerun_search) {
		gtk_html_engine_search (
			GTK_HTML (web_view),
			search_bar->priv->active_search,
			case_sensitive, search_forward, FALSE);
		search_bar->priv->rerun_search = FALSE;
		g_free (text);
	} else {
		gtk_html_engine_search_set_forward (
			GTK_HTML (web_view), search_forward);
		if (!gtk_html_engine_search_next (GTK_HTML (web_view)))
			wrapped = TRUE;
		g_free (text);
	}

	if (new_search || wrapped)
		gtk_html_engine_search (
			GTK_HTML (web_view),
			search_bar->priv->active_search,
			case_sensitive, search_forward, FALSE);

	g_object_notify (G_OBJECT (search_bar), "active-search");

	/* Update wrapped label visibility. */

	widget = search_bar->priv->wrapped_next_box;

	if (wrapped && search_forward)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);

	widget = search_bar->priv->wrapped_prev_box;

	if (wrapped && !search_forward)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);
}

static void
search_bar_changed_cb (ESearchBar *search_bar)
{
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
search_bar_set_web_view (ESearchBar *search_bar,
                         EWebView *web_view)
{
	GtkHTML *html;
	ESearchingTokenizer *tokenizer;

	g_return_if_fail (search_bar->priv->web_view == NULL);

	search_bar->priv->web_view = g_object_ref (web_view);

	html = GTK_HTML (web_view);
	tokenizer = e_search_bar_get_tokenizer (search_bar);
	gtk_html_set_tokenizer (html, HTML_TOKENIZER (tokenizer));
}

static void
search_bar_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
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
	ESearchBarPrivate *priv;

	priv = E_SEARCH_BAR_GET_PRIVATE (object);

	if (priv->web_view != NULL) {
		g_object_unref (priv->web_view);
		priv->web_view = NULL;
	}

	if (priv->entry != NULL) {
		g_object_unref (priv->entry);
		priv->entry = NULL;
	}

	if (priv->case_sensitive_button != NULL) {
		g_object_unref (priv->case_sensitive_button);
		priv->case_sensitive_button = NULL;
	}

	if (priv->wrapped_next_box != NULL) {
		g_object_unref (priv->wrapped_next_box);
		priv->wrapped_next_box = NULL;
	}

	if (priv->wrapped_prev_box != NULL) {
		g_object_unref (priv->wrapped_prev_box);
		priv->wrapped_prev_box = NULL;
	}

	if (priv->matches_label != NULL) {
		g_object_unref (priv->matches_label);
		priv->matches_label = NULL;
	}

	if (priv->tokenizer != NULL) {
		g_object_unref (priv->tokenizer);
		priv->tokenizer = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_search_bar_parent_class)->dispose (object);
}

static void
search_bar_finalize (GObject *object)
{
	ESearchBarPrivate *priv;

	priv = E_SEARCH_BAR_GET_PRIVATE (object);

	g_free (priv->active_search);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_search_bar_parent_class)->finalize (object);
}

static void
search_bar_constructed (GObject *object)
{
	ESearchBarPrivate *priv;

	priv = E_SEARCH_BAR_GET_PRIVATE (object);

	e_mutual_binding_new (
		object, "case-sensitive",
		priv->case_sensitive_button, "active");
}

static void
search_bar_show (GtkWidget *widget)
{
	ESearchBar *search_bar;

	search_bar = E_SEARCH_BAR (widget);

	/* Chain up to parent's show() method. */
	GTK_WIDGET_CLASS (e_search_bar_parent_class)->show (widget);

	gtk_widget_grab_focus (search_bar->priv->entry);

	search_bar_update_tokenizer (search_bar);
}

static void
search_bar_hide (GtkWidget *widget)
{
	ESearchBar *search_bar;

	search_bar = E_SEARCH_BAR (widget);

	/* Chain up to parent's hide() method. */
	GTK_WIDGET_CLASS (e_search_bar_parent_class)->hide (widget);

	search_bar_update_tokenizer (search_bar);
}

static gboolean
search_bar_key_press_event (GtkWidget *widget,
                            GdkEventKey *event)
{
	GtkWidgetClass *widget_class;

	if (event->keyval == GDK_Escape) {
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

	search_bar_update_tokenizer (search_bar);

	g_object_notify (G_OBJECT (search_bar), "active-search");
}

static void
e_search_bar_class_init (ESearchBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (ESearchBarPrivate));

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

	search_bar->priv = E_SEARCH_BAR_GET_PRIVATE (search_bar);
	search_bar->priv->tokenizer = e_searching_tokenizer_new ();

	g_signal_connect_swapped (
		search_bar->priv->tokenizer, "match",
		G_CALLBACK (search_bar_update_matches), search_bar);

	gtk_box_set_spacing (GTK_BOX (search_bar), 12);

	container = GTK_WIDGET (search_bar);

	widget = gtk_hbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (widget, _("Close the find bar"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_hide), search_bar);

	widget = gtk_label_new_with_mnemonic (_("Fin_d:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 3);
	gtk_widget_show (widget);

	label = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_icon_from_stock (
		GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY,
		GTK_STOCK_CLEAR);
	gtk_entry_set_icon_tooltip_text (
		GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY,
		_("Clear the search"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_widget_set_size_request (widget, 200, -1);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->entry = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_new (
		search_bar, "active-search",
		widget, "secondary-icon-sensitive");

	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (search_bar_find_next_cb), search_bar);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (search_bar_changed_cb), search_bar);

	g_signal_connect_swapped (
		widget, "icon-release",
		G_CALLBACK (search_bar_icon_release_cb), search_bar);

	widget = gtk_button_new_with_mnemonic (_("_Previous"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the previous occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_new (search_bar, "active-search", widget, "sensitive");

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (search_bar_find_previous_cb), search_bar);

	widget = gtk_button_new_with_mnemonic (_("_Next"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the next occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_new (search_bar, "active-search", widget, "sensitive");

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

	widget = gtk_hbox_new (FALSE, 6);
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
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = GTK_WIDGET (search_bar);

	widget = gtk_hbox_new (FALSE, 6);
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
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
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

ESearchingTokenizer *
e_search_bar_get_tokenizer (ESearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->tokenizer;
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
