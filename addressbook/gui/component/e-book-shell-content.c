/*
 * e-book-shell-content.c
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

#include "e-book-shell-content.h"

#include <glib/gi18n.h>
#include <e-util/gconf-bridge.h>

#define E_BOOK_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_CONTENT, EBookShellContentPrivate))

struct _EBookShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *notebook;
	GtkWidget *preview;
};

enum {
	PROP_0,
	PROP_CURRENT_VIEW,
	PROP_PREVIEW_CONTACT,
	PROP_PREVIEW_VISIBLE
};

static gpointer parent_class;

static void
book_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			e_book_shell_content_set_current_view (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_PREVIEW_CONTACT:
			e_book_shell_content_set_preview_contact (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_PREVIEW_VISIBLE:
			e_book_shell_content_set_preview_visible (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			g_value_set_object (
				value, e_book_shell_content_get_current_view (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_CONTACT:
			g_value_set_object (
				value, e_book_shell_content_get_preview_contact (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value, e_book_shell_content_get_preview_visible (
				E_BOOK_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_content_dispose (GObject *object)
{
	EBookShellContentPrivate *priv;

	priv = E_BOOK_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->notebook != NULL) {
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
	}

	if (priv->preview != NULL) {
		g_object_unref (priv->preview);
		priv->preview = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
book_shell_content_constructed (GObject *object)
{
	EBookShellContentPrivate *priv;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *key;

	priv = E_BOOK_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	container = GTK_WIDGET (object);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add2 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = eab_contact_display_new ();
	eab_contact_display_set_mode (
		EAB_CONTACT_DISPLAY (widget),
		EAB_CONTACT_DISPLAY_RENDER_NORMAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->preview = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/addressbook/display/vpane_position";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");
}

static void
book_shell_content_class_init (EBookShellContentClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EBookShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = book_shell_content_set_property;
	object_class->get_property = book_shell_content_get_property;
	object_class->dispose = book_shell_content_dispose;
	object_class->constructed = book_shell_content_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VIEW,
		g_param_spec_object (
			"current-view",
			_("Current View"),
			_("The currently selected address book view"),
			E_TYPE_ADDRESSBOOK_VIEW,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_CONTACT,
		g_param_spec_object (
			"preview-contact",
			_("Previewed Contact"),
			_("The contact being shown in the preview pane"),
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			_("Preview is Visible"),
			_("Whether the preview pane is visible"),
			TRUE,
			G_PARAM_READWRITE));
}

static void
book_shell_content_init (EBookShellContent *book_shell_content)
{
	book_shell_content->priv =
		E_BOOK_SHELL_CONTENT_GET_PRIVATE (book_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_book_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EBookShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) book_shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EBookShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) book_shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "EBookShellContent",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_book_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_BOOK_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

void
e_book_shell_content_insert_view (EBookShellContent *book_shell_content,
                                  EAddressbookView *addressbook_view)
{
	GtkNotebook *notebook;
	GtkWidget *child;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	gtk_notebook_append_page (notebook, child, NULL);
}

void
e_book_shell_content_remove_view (EBookShellContent *book_shell_content,
                                  EAddressbookView *addressbook_view)
{
	GtkNotebook *notebook;
	GtkWidget *child;
	gint page_num;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	page_num = gtk_notebook_page_num (notebook, child);
	g_return_if_fail (page_num >= 0);

	gtk_notebook_remove_page (notebook, page_num);
}

EAddressbookView *
e_book_shell_content_get_current_view (EBookShellContent *book_shell_content)
{
	GtkNotebook *notebook;
	GtkWidget *widget;
	gint page_num;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	page_num = gtk_notebook_get_current_page (notebook);
	widget = gtk_notebook_get_nth_page (notebook, page_num);
	g_return_val_if_fail (widget != NULL, NULL);

	return E_ADDRESSBOOK_VIEW (widget);
}

void
e_book_shell_content_set_current_view (EBookShellContent *book_shell_content,
                                       EAddressbookView *addressbook_view)
{
	GtkNotebook *notebook;
	GtkWidget *child;
	gint page_num;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	page_num = gtk_notebook_page_num (notebook, child);
	g_return_if_fail (page_num >= 0);

	gtk_notebook_set_current_page (notebook, page_num);
	g_object_notify (G_OBJECT (book_shell_content), "current-view");
}

EContact *
e_book_shell_content_get_preview_contact (EBookShellContent *book_shell_content)
{
	EABContactDisplay *display;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	display = EAB_CONTACT_DISPLAY (book_shell_content->priv->preview);

	return eab_contact_display_get_contact (display);
}

void
e_book_shell_content_set_preview_contact (EBookShellContent *book_shell_content,
                                          EContact *preview_contact)
{
	EABContactDisplay *display;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	display = EAB_CONTACT_DISPLAY (book_shell_content->priv->preview);

	eab_contact_display_set_contact (display, preview_contact);
	g_object_notify (G_OBJECT (book_shell_content), "preview-contact");
}

gboolean
e_book_shell_content_get_preview_visible (EBookShellContent *book_shell_content)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), FALSE);

	paned = GTK_PANED (book_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	return GTK_WIDGET_VISIBLE (child);
}

void
e_book_shell_content_set_preview_visible (EBookShellContent *book_shell_content,
                                          gboolean preview_visible)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	paned = GTK_PANED (book_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	if (preview_visible)
		gtk_widget_show (child);
	else
		gtk_widget_hide (child);

	g_object_notify (G_OBJECT (book_shell_content), "preview-visible");
}

void
e_book_shell_content_clipboard_copy (EBookShellContent *book_shell_content)
{
	EAddressbookView *addressbook_view;
	GtkHTML *html;
	gchar *selection;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	html = GTK_HTML (book_shell_content->priv->preview);
	addressbook_view =
		e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (addressbook_view != NULL);

	if (!GTK_WIDGET_HAS_FOCUS (html)) {
		e_addressbook_view_copy (addressbook_view);
		return;
	}

	selection = gtk_html_get_selection_html (html, NULL);
	if (selection != NULL)
		gtk_html_copy (html);
	g_free (selection);
}
