/*
 * e-mail-search-bar.c
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

#include "e-mail-search-bar.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtkhtml/gtkhtml-search.h>

#include "e-util/e-binding.h"

#define E_MAIL_SEARCH_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SEARCH_BAR, EMailSearchBarPrivate))

struct _EMailSearchBarPrivate {
	GtkHTML *html;
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
	PROP_CASE_SENSITIVE,
	PROP_HTML,
	PROP_TEXT
};

enum {
	CHANGED,
	CLEAR,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
mail_search_bar_update_matches (EMailSearchBar *search_bar)
{
	ESearchingTokenizer *tokenizer;
	GtkWidget *matches_label;
	gint matches;
	gchar *text;

	tokenizer = e_mail_search_bar_get_tokenizer (search_bar);
	matches_label = search_bar->priv->matches_label;

	matches = e_searching_tokenizer_match_count (tokenizer);
	text = g_strdup_printf (_("Matches: %d"), matches);

	gtk_label_set_text (GTK_LABEL (matches_label), text);
	gtk_widget_show (matches_label);

	g_free (text);
}

static void
mail_search_bar_update_tokenizer (EMailSearchBar *search_bar)
{
	ESearchingTokenizer *tokenizer;
	gboolean case_sensitive;
	gchar *active_search;

	tokenizer = e_mail_search_bar_get_tokenizer (search_bar);
	case_sensitive = e_mail_search_bar_get_case_sensitive (search_bar);

	if (GTK_WIDGET_VISIBLE (search_bar))
		active_search = search_bar->priv->active_search;
	else
		active_search = NULL;

	e_searching_tokenizer_set_primary_case_sensitivity (
		tokenizer, case_sensitive);
	e_searching_tokenizer_set_primary_search_string (
		tokenizer, active_search);

	e_mail_search_bar_changed (search_bar);
}

static void
mail_search_bar_find (EMailSearchBar *search_bar,
                      gboolean search_forward)
{
	GtkHTML *html;
	GtkWidget *widget;
	gboolean case_sensitive;
	gboolean new_search;
	gboolean wrapped = FALSE;
	gchar *text;

	html = e_mail_search_bar_get_html (search_bar);
	case_sensitive = e_mail_search_bar_get_case_sensitive (search_bar);
	text = e_mail_search_bar_get_text (search_bar);

	if (text == NULL || *text == '\0')
		gtk_widget_hide (search_bar->priv->matches_label);

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
		mail_search_bar_update_tokenizer (search_bar);
	} else if (search_bar->priv->rerun_search) {
		gtk_html_engine_search (
			html, search_bar->priv->active_search,
			case_sensitive, search_forward, FALSE);
		search_bar->priv->rerun_search = FALSE;
		g_free (text);
	} else {
		gtk_html_engine_search_set_forward (html, search_forward);
		if (!gtk_html_engine_search_next (html))
			wrapped = TRUE;
		g_free (text);
	}

	if (new_search || wrapped)
		gtk_html_engine_search (
			html, search_bar->priv->active_search,
			case_sensitive, search_forward, FALSE);

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
mail_search_bar_changed_cb (EMailSearchBar *search_bar)
{
	g_object_notify (G_OBJECT (search_bar), "text");
}

static void
mail_search_bar_find_next_cb (EMailSearchBar *search_bar)
{
	mail_search_bar_find (search_bar, TRUE);
}

static void
mail_search_bar_find_previous_cb (EMailSearchBar *search_bar)
{
	mail_search_bar_find (search_bar, FALSE);
}

static void
mail_search_bar_icon_release_cb (EMailSearchBar *search_bar,
                                 GtkEntryIconPosition icon_pos,
                                 GdkEvent *event)
{
	g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

	e_mail_search_bar_clear (search_bar);
	gtk_widget_grab_focus (search_bar->priv->entry);
}

static void
mail_search_bar_toggled_cb (EMailSearchBar *search_bar)
{
	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search = NULL;

	g_object_notify (G_OBJECT (search_bar), "case-sensitive");
}

static void
mail_search_bar_set_html (EMailSearchBar *search_bar,
                          GtkHTML *html)
{
	ESearchingTokenizer *tokenizer;

	g_return_if_fail (search_bar->priv->html == NULL);

	search_bar->priv->html = g_object_ref (html);

	tokenizer = e_mail_search_bar_get_tokenizer (search_bar);
	gtk_html_set_tokenizer (html, HTML_TOKENIZER (tokenizer));
}

static void
mail_search_bar_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CASE_SENSITIVE:
			e_mail_search_bar_set_case_sensitive (
				E_MAIL_SEARCH_BAR (object),
				g_value_get_boolean (value));
			return;

		case PROP_HTML:
			mail_search_bar_set_html (
				E_MAIL_SEARCH_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_TEXT:
			e_mail_search_bar_set_text (
				E_MAIL_SEARCH_BAR (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_search_bar_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CASE_SENSITIVE:
			g_value_set_boolean (
				value, e_mail_search_bar_get_case_sensitive (
				E_MAIL_SEARCH_BAR (object)));
			return;

		case PROP_HTML:
			g_value_set_object (
				value, e_mail_search_bar_get_html (
				E_MAIL_SEARCH_BAR (object)));
			return;

		case PROP_TEXT:
			g_value_take_string (
				value, e_mail_search_bar_get_text (
				E_MAIL_SEARCH_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_search_bar_dispose (GObject *object)
{
	EMailSearchBarPrivate *priv;

	priv = E_MAIL_SEARCH_BAR_GET_PRIVATE (object);

	if (priv->html != NULL) {
		g_object_unref (priv->html);
		priv->html = NULL;
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
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_search_bar_finalize (GObject *object)
{
	EMailSearchBarPrivate *priv;

	priv = E_MAIL_SEARCH_BAR_GET_PRIVATE (object);

	g_free (priv->active_search);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_search_bar_constructed (GObject *object)
{
	EMailSearchBarPrivate *priv;

	priv = E_MAIL_SEARCH_BAR_GET_PRIVATE (object);

	e_mutual_binding_new (
		G_OBJECT (object), "case-sensitive",
		G_OBJECT (priv->case_sensitive_button), "active");
}

static void
mail_search_bar_show (GtkWidget *widget)
{
	EMailSearchBar *search_bar;

	search_bar = E_MAIL_SEARCH_BAR (widget);

	/* Chain up to parent's show() method. */
	GTK_WIDGET_CLASS (parent_class)->show (widget);

	gtk_widget_grab_focus (search_bar->priv->entry);

	mail_search_bar_update_tokenizer (search_bar);
}

static void
mail_search_bar_hide (GtkWidget *widget)
{
	EMailSearchBar *search_bar;

	search_bar = E_MAIL_SEARCH_BAR (widget);

	/* Chain up to parent's hide() method. */
	GTK_WIDGET_CLASS (parent_class)->hide (widget);

	mail_search_bar_update_tokenizer (search_bar);
}

static gboolean
mail_search_bar_key_press_event (GtkWidget *widget,
                                 GdkEventKey *event)
{
	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (widget);
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		key_press_event (widget, event);
}

static void
mail_search_bar_clear (EMailSearchBar *search_bar)
{
	g_free (search_bar->priv->active_search);
	search_bar->priv->active_search = NULL;

	gtk_widget_hide (search_bar->priv->wrapped_next_box);
	gtk_widget_hide (search_bar->priv->wrapped_prev_box);
	gtk_widget_hide (search_bar->priv->matches_label);

	mail_search_bar_update_tokenizer (search_bar);
}

static void
mail_search_bar_class_init (EMailSearchBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailSearchBarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_search_bar_set_property;
	object_class->get_property = mail_search_bar_get_property;
	object_class->dispose = mail_search_bar_dispose;
	object_class->finalize = mail_search_bar_finalize;
	object_class->constructed = mail_search_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = mail_search_bar_show;
	widget_class->hide = mail_search_bar_hide;
	widget_class->key_press_event = mail_search_bar_key_press_event;

	class->clear = mail_search_bar_clear;

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
		PROP_HTML,
		g_param_spec_object (
			"html",
			"HTML Display",
			NULL,
			GTK_TYPE_HTML,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			"Search Text",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSearchBarClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CLEAR] = g_signal_new (
		"clear",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSearchBarClass, clear),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
mail_search_bar_init (EMailSearchBar *search_bar)
{
	GtkWidget *label;
	GtkWidget *widget;
	GtkWidget *container;

	search_bar->priv = E_MAIL_SEARCH_BAR_GET_PRIVATE (search_bar);
	search_bar->priv->tokenizer = e_searching_tokenizer_new ();

	g_signal_connect_swapped (
		search_bar->priv->tokenizer, "match",
		G_CALLBACK (mail_search_bar_update_matches), search_bar);

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

	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (mail_search_bar_find_next_cb), search_bar);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (mail_search_bar_changed_cb), search_bar);

	g_signal_connect_swapped (
		widget, "icon-release",
		G_CALLBACK (mail_search_bar_icon_release_cb), search_bar);

	widget = gtk_button_new_with_mnemonic (_("_Previous"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the previous occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_search_bar_find_previous_cb), search_bar);

	widget = gtk_button_new_with_mnemonic (_("_Next"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		widget, _("Find the next occurrence of the phrase"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_search_bar_find_next_cb), search_bar);

	widget = gtk_check_button_new_with_mnemonic (_("Mat_ch case"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	search_bar->priv->case_sensitive_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (mail_search_bar_toggled_cb), search_bar);

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (mail_search_bar_find_next_cb), search_bar);

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

GType
e_mail_search_bar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailSearchBarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_search_bar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailSearchBar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_search_bar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_HBOX, "EMailSearchBar", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_mail_search_bar_new (GtkHTML *html)
{
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	return g_object_new (E_TYPE_MAIL_SEARCH_BAR, "html", html, NULL);
}

void
e_mail_search_bar_clear (EMailSearchBar *search_bar)
{
	g_return_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar));

	g_signal_emit (search_bar, signals[CLEAR], 0);
}

void
e_mail_search_bar_changed (EMailSearchBar *search_bar)
{
	g_return_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar));

	g_signal_emit (search_bar, signals[CHANGED], 0);
}

GtkHTML *
e_mail_search_bar_get_html (EMailSearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->html;
}

ESearchingTokenizer *
e_mail_search_bar_get_tokenizer (EMailSearchBar *search_bar)
{
	g_return_val_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar), NULL);

	return search_bar->priv->tokenizer;
}

gboolean
e_mail_search_bar_get_case_sensitive (EMailSearchBar *search_bar)
{
	GtkToggleButton *button;

	g_return_val_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar), FALSE);

	button = GTK_TOGGLE_BUTTON (search_bar->priv->case_sensitive_button);

	return gtk_toggle_button_get_active (button);
}

void
e_mail_search_bar_set_case_sensitive (EMailSearchBar *search_bar,
                                      gboolean case_sensitive)
{
	GtkToggleButton *button;

	g_return_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar));

	button = GTK_TOGGLE_BUTTON (search_bar->priv->case_sensitive_button);

	gtk_toggle_button_set_active (button, case_sensitive);

	g_object_notify (G_OBJECT (search_bar), "case-sensitive");
}

gchar *
e_mail_search_bar_get_text (EMailSearchBar *search_bar)
{
	GtkEntry *entry;
	const gchar *text;

	g_return_val_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar), NULL);

	entry = GTK_ENTRY (search_bar->priv->entry);
	text = gtk_entry_get_text (entry);

	return g_strstrip (g_strdup (text));
}

void
e_mail_search_bar_set_text (EMailSearchBar *search_bar,
                            const gchar *text)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_MAIL_SEARCH_BAR (search_bar));

	entry = GTK_ENTRY (search_bar->priv->entry);

	if (text == NULL)
		text = "";

	/* This will trigger a "notify::text" signal. */
	gtk_entry_set_text (entry, text);
}
