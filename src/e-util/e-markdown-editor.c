/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#ifdef HAVE_MARKDOWN
#include <cmark.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserverui/libedataserverui.h>

#include "e-misc-utils.h"
#include "e-spell-text-view.h"
#include "e-web-view.h"

#include "e-markdown-editor.h"

struct _EMarkdownEditorPrivate {
	GtkTextView *text_view;
	EWebView *web_view;
	GtkToolbar *action_toolbar;
	gboolean is_dark_theme;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMarkdownEditor, e_markdown_editor, GTK_TYPE_BOX)

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
e_markdown_editor_get_selection (EMarkdownEditor *self,
				 GtkTextIter *out_start,
				 GtkTextIter *out_end,
				 gchar **out_selected_text)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (self->priv->text_view);

	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end) && out_selected_text) {
		*out_selected_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	} else if (out_selected_text) {
		*out_selected_text = NULL;
	}

	if (out_start)
		*out_start = start;

	if (out_end)
		*out_end = end;
}

static void
e_markdown_editor_surround_selection (EMarkdownEditor *self,
				      gboolean whole_lines,
				      const gchar *prefix,
				      const gchar *suffix)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer;

	e_markdown_editor_get_selection (self, &start, &end, NULL);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);

	gtk_text_buffer_begin_user_action (buffer);

	if (whole_lines) {
		gint to_line, ii;

		to_line = gtk_text_iter_get_line (&end);

		for (ii = gtk_text_iter_get_line (&start); ii <= to_line; ii++) {
			GtkTextIter iter;

			gtk_text_buffer_get_iter_at_line (buffer, &iter, ii);

			if (prefix && *prefix)
				gtk_text_buffer_insert (buffer, &iter, prefix, -1);

			if (suffix && *suffix) {
				gtk_text_iter_forward_to_line_end (&iter);
				gtk_text_buffer_insert (buffer, &iter, suffix, -1);
			}
		}
	} else {
		gint end_offset = gtk_text_iter_get_offset (&end);

		if (prefix && *prefix) {
			gtk_text_buffer_insert (buffer, &start, prefix, -1);
			/* Keep the cursor where it is, move it only when the suffix is used */
			end_offset += strlen (prefix);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);
		}

		if (suffix && *suffix) {
			gtk_text_buffer_insert (buffer, &end, suffix, -1);
			/* Place the cursor before the suffix */
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);
			gtk_text_buffer_select_range (buffer, &end, &end);
		}
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
e_markdown_editor_add_bold_text_cb (GtkToolButton *button,
				    gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, FALSE, "**", "**");
}

static void
e_markdown_editor_add_italic_text_cb (GtkToolButton *button,
				      gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, FALSE, "_", "_");
}

static void
e_markdown_editor_insert_quote_cb (GtkToolButton *button,
				   gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "> ", NULL);
}

static void
e_markdown_editor_insert_code_cb (GtkToolButton *button,
				  gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	GtkTextIter start, end;
	gchar *selection = NULL;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_get_selection (self, &start, &end, &selection);

	if (selection && strchr (selection, '\n')) {
		GtkTextBuffer *buffer;
		GtkTextIter iter;
		gint start_line, end_line;

		buffer = gtk_text_view_get_buffer (self->priv->text_view);

		gtk_text_buffer_begin_user_action (buffer);

		start_line = gtk_text_iter_get_line (&start);
		end_line = gtk_text_iter_get_line (&end);

		gtk_text_buffer_get_iter_at_line (buffer, &iter, start_line);
		gtk_text_buffer_insert (buffer, &iter, "```\n", -1);

		/* One line added above + 1 for the end line itself */
		end_line = end_line + 2;
		gtk_text_buffer_get_iter_at_line (buffer, &iter, end_line);
		if (gtk_text_iter_is_end (&iter) && gtk_text_iter_get_line_offset (&iter) > 0) {
			gtk_text_buffer_insert (buffer, &iter, "\n```\n", -1);
		} else {
			if (gtk_text_iter_is_end (&iter))
				end_line--;
			gtk_text_buffer_insert (buffer, &iter, "```\n", -1);
		}

		/* Place the cursor before the suffix */
		gtk_text_buffer_get_iter_at_line (buffer, &iter, end_line);
		gtk_text_buffer_select_range (buffer, &iter, &iter);

		gtk_text_buffer_end_user_action (buffer);
	} else {
		e_markdown_editor_surround_selection (self, FALSE, "`", "`");
	}

	g_free (selection);
}

static void
e_markdown_editor_add_link_cb (GtkToolButton *button,
			       gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *selection = NULL;
	gint offset;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_get_selection (self, &start, &end, &selection);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	offset = gtk_text_iter_get_offset (&start);

	gtk_text_buffer_begin_user_action (buffer);

	if (selection && *selection) {
		gint end_offset = gtk_text_iter_get_offset (&end);

		if (g_ascii_strncasecmp (selection, "http:", 5) == 0 ||
		    g_ascii_strncasecmp (selection, "https:", 6) == 0) {
			gtk_text_buffer_insert (buffer, &start, "[](", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 3);
			gtk_text_buffer_insert (buffer, &end, ")", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &start, offset + 1);
			end = start;
		} else {
			gtk_text_buffer_insert (buffer, &start, "[", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 1);
			gtk_text_buffer_insert (buffer, &end, "](https://)", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &start, end_offset + 1 + 2);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 1 + 10);
		}

		gtk_text_buffer_select_range (buffer, &start, &end);
	} else {
		gtk_text_buffer_insert (buffer, &start, "[](https://)", -1);

		/* skip "[](" */
		offset += 3;

		gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);

		/* after the "https://" text */
		gtk_text_buffer_get_iter_at_offset (buffer, &end, offset + 8);

		gtk_text_buffer_select_range (buffer, &start, &end);
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
e_markdown_editor_add_bullet_list_cb (GtkToolButton *button,
				      gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "- ", NULL);
}

static void
e_markdown_editor_add_numbered_list_cb (GtkToolButton *button,
					gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "1. ", NULL);
}

static void
e_markdown_editor_add_header_cb (GtkToolButton *button,
				 gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "# ", NULL);
}

static void
e_markdown_editor_markdown_syntax_cb (GtkToolButton *button,
				      gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	GtkWidget *toplevel;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	e_show_uri (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, "https://commonmark.org/help/");
}

#ifdef HAVE_MARKDOWN
static void
e_markdown_editor_switch_page_cb (GtkNotebook *notebook,
				  GtkWidget *page,
				  guint page_num,
				  gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	gchar *converted;
	gchar *html;
	gint n_items, ii;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

	for (ii = 0; ii < n_items; ii++) {
		GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);

		if (item) {
			GtkWidget *widget = GTK_WIDGET (item);

			/* Keep only the help button and hide any other */
			if (g_strcmp0 (gtk_widget_get_name (widget), "markdown-help") != 0)
				gtk_widget_set_visible (widget, page_num != 1);
		}
	}

	/* Not the Preview page */
	if (page_num != 1)
		return;

	converted = e_markdown_editor_dup_html (self);

	html = g_strconcat ("<div class=\"-e-web-view-background-color -e-web-view-text-color\" style=\"border: none; padding: 0px; margin: 0;\">",
		converted ? converted : "",
		"</div>",
		NULL);

	e_web_view_load_string (self->priv->web_view, html);

	g_free (converted);
	g_free (html);
}
#endif /* HAVE_MARKDOWN */

static gboolean
e_markdown_editor_is_dark_theme (EMarkdownEditor *self)
{
	GtkStyleContext *style_context;
	GdkRGBA rgba;
	gdouble brightness;

	g_return_val_if_fail (self->priv->action_toolbar != NULL, FALSE);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (self->priv->action_toolbar));
	gtk_style_context_get_color (style_context, gtk_style_context_get_state (style_context), &rgba);

	brightness =
		(0.2109 * 255.0 * rgba.red) +
		(0.5870 * 255.0 * rgba.green) +
		(0.1021 * 255.0 * rgba.blue);

	return brightness > 140;
}

struct _toolbar_items {
	const gchar *label;
	const gchar *icon_name;
	const gchar *icon_name_dark;
	GCallback callback;
};

static struct _toolbar_items toolbar_items[] = {
	#define ITEM(lbl, icn, cbk) { lbl, icn, icn "-dark", G_CALLBACK (cbk) }
	ITEM (N_("Add bold text"), "markdown-bold", e_markdown_editor_add_bold_text_cb),
	ITEM (N_("Add italic text"), "markdown-italic", e_markdown_editor_add_italic_text_cb),
	ITEM (N_("Insert a quote"), "markdown-quote", e_markdown_editor_insert_quote_cb),
	ITEM (N_("Insert code"), "markdown-code", e_markdown_editor_insert_code_cb),
	ITEM (N_("Add a link"), "markdown-link", e_markdown_editor_add_link_cb),
	ITEM (N_("Add a bullet list"), "markdown-bullets", e_markdown_editor_add_bullet_list_cb),
	ITEM (N_("Add a numbered list"), "markdown-numbers", e_markdown_editor_add_numbered_list_cb),
	ITEM (N_("Add a header"), "markdown-header", e_markdown_editor_add_header_cb),
	ITEM (NULL, "", NULL),
	ITEM (N_("Open online common mark documentation"), "markdown-help", G_CALLBACK (e_markdown_editor_markdown_syntax_cb))
	#undef ITEM
};

static void
e_markdown_editor_style_updated_cb (GtkWidget *widget,
				    gpointer user_data)
{
	EMarkdownEditor *self;
	gboolean is_dark_theme;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (widget));

	self = E_MARKDOWN_EDITOR (widget);
	is_dark_theme = e_markdown_editor_is_dark_theme (self);

	if (self->priv->is_dark_theme != is_dark_theme) {
		gint n_items, ii, jj, idx = 0;

		self->priv->is_dark_theme = is_dark_theme;

		n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

		for (ii = 0; ii < n_items; ii++) {
			GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);
			const gchar *name;

			if (!item || !GTK_IS_TOOL_BUTTON (item))
				continue;

			name = gtk_widget_get_name (GTK_WIDGET (item));

			if (!name || !*name)
				continue;

			for (jj = 0; jj < G_N_ELEMENTS (toolbar_items); jj++) {
				gint index = (jj + idx) % G_N_ELEMENTS (toolbar_items);

				if (g_strcmp0 (name, toolbar_items[index].icon_name) == 0) {
					const gchar *icon_name = is_dark_theme ? toolbar_items[index].icon_name_dark : toolbar_items[index].icon_name;

					if (icon_name) {
						GtkWidget *icon_widget = gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON (item));

						if (icon_widget)
							gtk_image_set_from_icon_name (GTK_IMAGE (icon_widget), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
						else
							gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_name);
					}

					idx = jj + 1;
					break;
				}
			}
		}
	}
}

static void
e_markdown_editor_notify_editable_cb (GObject *object,
				      GParamSpec *param,
				      gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	gboolean sensitive = FALSE;
	gint n_items, ii;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	g_object_get (object, "editable", &sensitive, NULL);

	n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

	for (ii = 0; ii < n_items; ii++) {
		GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);

		if (item) {
			GtkWidget *widget = GTK_WIDGET (item);

			/* Keep only the help button and hide any other */
			if (g_strcmp0 (gtk_widget_get_name (widget), "markdown-help") != 0)
				gtk_widget_set_sensitive (widget, sensitive);
		}
	}
}

static void
e_markdown_editor_text_view_changed_cb (GtkTextView *text_view,
					gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}

static void
e_markdown_editor_constructed (GObject *object)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (object);
	GtkWidget *widget;
	GtkNotebook *notebook;
	GtkScrolledWindow *scrolled_window;
	guint ii;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_markdown_editor_parent_class)->constructed (object);

	widget = gtk_notebook_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"show-border", TRUE,
		"show-tabs", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (self), widget, TRUE, TRUE, 0);

	notebook = GTK_NOTEBOOK (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_notebook_append_page (notebook, widget, gtk_label_new_with_mnemonic (_("_Write")));

	scrolled_window = GTK_SCROLLED_WINDOW (widget);

	widget = gtk_text_view_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"margin", 4,
		"monospace", TRUE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->priv->text_view = GTK_TEXT_VIEW (widget);

	e_buffer_tagger_connect (self->priv->text_view);
	e_spell_text_view_attach (self->priv->text_view);

	#ifdef HAVE_MARKDOWN
	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_notebook_append_page (notebook, widget, gtk_label_new_with_mnemonic (_("_Preview")));

	scrolled_window = GTK_SCROLLED_WINDOW (widget);

	widget = e_web_view_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"margin", 4,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->priv->web_view = E_WEB_VIEW (widget);
	#endif /* HAVE_MARKDOWN */

	widget = gtk_toolbar_new ();
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (widget), GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (widget);
	gtk_notebook_set_action_widget (notebook, widget, GTK_PACK_END);

	self->priv->action_toolbar = GTK_TOOLBAR (widget);
	self->priv->is_dark_theme = e_markdown_editor_is_dark_theme (self);

	for (ii = 0; ii < G_N_ELEMENTS (toolbar_items); ii++) {
		GtkToolItem *item;

		if (toolbar_items[ii].callback) {
			GtkWidget *icon;
			const gchar *icon_name;

			icon_name = self->priv->is_dark_theme ? toolbar_items[ii].icon_name_dark : toolbar_items[ii].icon_name;
			icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
			gtk_widget_show (GTK_WIDGET (icon));
			item = gtk_tool_button_new (icon, _(toolbar_items[ii].label));
			gtk_widget_set_name (GTK_WIDGET (item), toolbar_items[ii].icon_name);
			gtk_tool_item_set_tooltip_text (item, _(toolbar_items[ii].label));
			g_signal_connect_object (item, "clicked", toolbar_items[ii].callback, self, 0);
		} else {
			item = gtk_separator_tool_item_new ();
		}

		gtk_widget_show (GTK_WIDGET (item));
		gtk_toolbar_insert (self->priv->action_toolbar, item, -1);
	}

	#ifdef HAVE_MARKDOWN
	g_signal_connect_object (notebook, "switch-page", G_CALLBACK (e_markdown_editor_switch_page_cb), self, 0);
	#endif

	g_signal_connect (self, "style-updated", G_CALLBACK (e_markdown_editor_style_updated_cb), NULL);
	g_signal_connect_object (gtk_text_view_get_buffer (self->priv->text_view), "changed", G_CALLBACK (e_markdown_editor_text_view_changed_cb), self, 0);
	e_signal_connect_notify_object (self->priv->text_view, "notify::editable", G_CALLBACK (e_markdown_editor_notify_editable_cb), self, 0);
}

static void
e_markdown_editor_class_init (EMarkdownEditorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_markdown_editor_constructed;

	/**
	 * EMarkdownEditor::changed:
	 * @self: an #EMarkdownEditor, which sent the signal
	 *
	 * This signal is emitted the content of the @self changes.
	 *
	 * Since: 3.44
	 **/
	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

static void
e_markdown_editor_init (EMarkdownEditor *self)
{
	self->priv = e_markdown_editor_get_instance_private (self);
}

/**
 * e_markdown_editor_new:
 *
 * Creates a new #EMarkdownEditor
 *
 * Returns: (transfer full): a new #EMarkdownEditor
 *
 * Since: 3.44
 */
GtkWidget *
e_markdown_editor_new (void)
{
	return g_object_new (E_TYPE_MARKDOWN_EDITOR, NULL);
}

/**
 * e_markdown_editor_get_text_view:
 * @self: an #EMarkdownEditor
 *
 * Returns: (transfer none): a #GtkTextView of the @self
 *
 * Since: 3.44
 **/
GtkTextView *
e_markdown_editor_get_text_view (EMarkdownEditor *self)
{
	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	return self->priv->text_view;
}

/**
 * e_markdown_editor_get_action_toolbar:
 * @self: an #EMarkdownEditor
 *
 * Returns: (transfer none): a #GtkToolbar of the @self, where the caller
 *    can add its own action buttons.
 *
 * Since: 3.44
 **/
GtkToolbar *
e_markdown_editor_get_action_toolbar (EMarkdownEditor *self)
{
	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	return self->priv->action_toolbar;
}

/**
 * e_markdown_editor_dup_text:
 * @self: an #EMarkdownEditor
 *
 * Get the markdown text entered in the @self. To get
 * the HTML version of it use e_markdown_editor_dup_html().
 * Free the returned string with g_free(), when no longer needed.
 *
 * Returns: (transfer full): the markdown text
 *
 * Since: 3.44
 **/
gchar *
e_markdown_editor_dup_text (EMarkdownEditor *self)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * e_markdown_editor_dup_html:
 * @self: an #EMarkdownEditor
 *
 * Get the HTML version of the markdown text entered in the @self.
 * To get the markdown text use e_markdown_editor_dup_text().
 * Free the returned string with g_free(), when no longer needed.
 *
 * Note: The function can return %NULL when was not built
 *    with the markdown support.
 *
 * Returns: (transfer full) (nullable): the markdown text converted
 *    into HTML, or %NULL, when was not built with the markdown support
 *
 * Since: 3.44
 **/
gchar *
e_markdown_editor_dup_html (EMarkdownEditor *self)
{
	#ifdef HAVE_MARKDOWN
	gchar *text, *html;
	#endif

	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	#ifdef HAVE_MARKDOWN
	text = e_markdown_editor_dup_text (self);
	html = e_markdown_util_text_to_html (text, -1);

	g_free (text);

	return html;
	#else
	return NULL;
	#endif
}

/**
 * e_markdown_util_text_to_html:
 * @plain_text: plain text with markdown to convert to HTML
 * @length: length of the @plain_text, or -1 when it's nul-terminated
 *
 * Convert @plain_text, possibly with markdown, into the HTML.
 *
 * Note: The function can return %NULL when was not built
 *    with the markdown support.
 *
 * Returns: (transfer full) (nullable): text converted into HTML,
 *    or %NULL, when was not built with the markdown support.
 *    Free the string with g_free(), when no longer needed.
 *
 * Since: 3.44
 **/
gchar *
e_markdown_util_text_to_html (const gchar *plain_text,
			      gssize length)
{
	#ifdef HAVE_MARKDOWN
	GString *html;
	gchar *converted;

	if (length == -1)
		length = plain_text ? strlen (plain_text) : 0;

	converted = cmark_markdown_to_html (plain_text ? plain_text : "", length,
		CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE);

	html = e_str_replace_string (converted, "<blockquote>", "<blockquote type=\"cite\">");

	g_free (converted);

	return g_string_free (html, FALSE);
	#else
	return NULL;
	#endif
}
