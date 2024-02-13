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
 *
 *
 * Authors:
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-select-names-editable.h"
#include "e-select-names-renderer.h"

struct _ESelectNamesRendererPrivate {
	EClientCache *client_cache;
	ESelectNamesEditable *editable;
	gchar *path;

	gchar *name;
	gchar *email;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_NAME,
	PROP_EMAIL
};

enum {
	CELL_EDITED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ESelectNamesRenderer, e_select_names_renderer, 	GTK_TYPE_CELL_RENDERER_TEXT)

static void
e_select_names_renderer_editing_done (GtkCellEditable *editable,
                                      ESelectNamesRenderer *renderer)
{
	GList *addresses = NULL, *names = NULL, *a, *n;
	gboolean editing_canceled;

	/* We don't need to listen for the focus out event any more */
	g_signal_handlers_disconnect_matched (
		editable, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, renderer);

	g_object_get (editable, "editing-canceled", &editing_canceled, NULL);
	if (editing_canceled) {
		gtk_cell_renderer_stop_editing (
			GTK_CELL_RENDERER (renderer), TRUE);
		goto cleanup;
	}

	addresses = e_select_names_editable_get_emails (
		E_SELECT_NAMES_EDITABLE (editable));
	names = e_select_names_editable_get_names (
		E_SELECT_NAMES_EDITABLE (editable));

	/* remove empty addresses */
	for (a = addresses, n = names; a && n;) {
		gchar *addr = a->data, *nm = n->data;

		if ((!addr || !*addr) && (!nm || !*nm)) {
			g_free (addr);
			g_free (nm);
			addresses = g_list_remove_link (addresses, a);
			names = g_list_remove_link (names, n);
			a = addresses;
			n = names;
		} else {
			a = a->next;
			n = n->next;
		}
	}

	g_signal_emit (
		renderer, signals[CELL_EDITED], 0,
		renderer->priv->path, addresses, names);

	g_list_free_full (addresses, (GDestroyNotify) g_free);
	g_list_free_full (names, (GDestroyNotify) g_free);

cleanup:
	g_free (renderer->priv->path);
	renderer->priv->path = NULL;
	renderer->priv->editable = NULL;
}

static void
select_names_renderer_set_client_cache (ESelectNamesRenderer *renderer,
                                        EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (renderer->priv->client_cache == NULL);

	renderer->priv->client_cache = g_object_ref (client_cache);
}

static void
select_names_renderer_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			select_names_renderer_set_client_cache (
				E_SELECT_NAMES_RENDERER (object),
				g_value_get_object (value));
			return;

		case PROP_NAME:
			e_select_names_renderer_set_name (
				E_SELECT_NAMES_RENDERER (object),
				g_value_get_string (value));
			return;

		case PROP_EMAIL:
			e_select_names_renderer_set_email (
				E_SELECT_NAMES_RENDERER (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
select_names_renderer_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_select_names_renderer_ref_client_cache (
				E_SELECT_NAMES_RENDERER (object)));
			return;

		case PROP_NAME:
			g_value_set_string (
				value,
				e_select_names_renderer_get_name (
				E_SELECT_NAMES_RENDERER (object)));
			return;

		case PROP_EMAIL:
			g_value_set_string (
				value,
				e_select_names_renderer_get_email (
				E_SELECT_NAMES_RENDERER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
select_names_renderer_dispose (GObject *object)
{
	ESelectNamesRenderer *self = E_SELECT_NAMES_RENDERER (object);

	g_clear_object (&self->priv->client_cache);
	g_clear_object (&self->priv->editable);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_select_names_renderer_parent_class)->
		dispose (object);
}

static void
select_names_renderer_finalize (GObject *object)
{
	ESelectNamesRenderer *self = E_SELECT_NAMES_RENDERER (object);

	g_free (self->priv->path);
	g_free (self->priv->name);
	g_free (self->priv->email);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_select_names_renderer_parent_class)->
		finalize (object);
}

static GtkCellEditable *
select_names_renderer_start_editing (GtkCellRenderer *cell,
                                     GdkEvent *event,
                                     GtkWidget *widget,
                                     const gchar *path,
                                     const GdkRectangle *bg_area,
                                     const GdkRectangle *cell_area,
                                     GtkCellRendererState flags)
{
	ESelectNamesRenderer *sn_cell = E_SELECT_NAMES_RENDERER (cell);
	GtkCellRendererText *text_cell = GTK_CELL_RENDERER_TEXT (cell);
	EClientCache *client_cache;
	GtkWidget *editable;
	gboolean is_editable;
	gfloat xalign;

	g_object_get (
		text_cell,
		"editable", &is_editable,
		"xalign", &xalign, NULL);

	if (!is_editable)
		return NULL;

	client_cache = e_select_names_renderer_ref_client_cache (sn_cell);

	editable = e_select_names_editable_new (client_cache);
	gtk_entry_set_has_frame (GTK_ENTRY (editable), FALSE);
	gtk_entry_set_alignment (GTK_ENTRY (editable), xalign);
	if (sn_cell->priv->email != NULL && *sn_cell->priv->email != '\0')
		e_select_names_editable_set_address (
			E_SELECT_NAMES_EDITABLE (editable),
			sn_cell->priv->name,
			sn_cell->priv->email);
	gtk_widget_show (editable);

	g_signal_connect (
		editable, "editing_done",
		G_CALLBACK (e_select_names_renderer_editing_done), sn_cell);

	sn_cell->priv->editable = E_SELECT_NAMES_EDITABLE (g_object_ref (editable));
	sn_cell->priv->path = g_strdup (path);

	g_object_unref (client_cache);

	return GTK_CELL_EDITABLE (editable);
}

static void
e_select_names_renderer_class_init (ESelectNamesRendererClass *class)
{
	GObjectClass *object_class;
	GtkCellRendererClass *renderer_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = select_names_renderer_get_property;
	object_class->set_property = select_names_renderer_set_property;
	object_class->dispose = select_names_renderer_dispose;
	object_class->finalize = select_names_renderer_finalize;

	renderer_class = GTK_CELL_RENDERER_CLASS (class);
	renderer_class->start_editing = select_names_renderer_start_editing;

	/**
	 * ESelectNamesRenderer:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_NAME,
		g_param_spec_string (
			"name",
			"Name",
			"Email name.",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EMAIL,
		g_param_spec_string (
			"email",
			"Email",
			"Email address.",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[CELL_EDITED] = g_signal_new (
		"cell_edited",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectNamesRendererClass, cell_edited),
		NULL, NULL,
		e_marshal_VOID__STRING_POINTER_POINTER,
		G_TYPE_NONE, 3,
		G_TYPE_STRING,
		G_TYPE_POINTER,
		G_TYPE_POINTER);
}

static void
e_select_names_renderer_init (ESelectNamesRenderer *renderer)
{
	renderer->priv = e_select_names_renderer_get_instance_private (renderer);
}

GtkCellRenderer *
e_select_names_renderer_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_SELECT_NAMES_RENDERER,
		"client-cache", client_cache, NULL);
}

EClientCache *
e_select_names_renderer_ref_client_cache (ESelectNamesRenderer *renderer)
{
	g_return_val_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer), NULL);

	return g_object_ref (renderer->priv->client_cache);
}

EDestination *
e_select_names_renderer_get_destination (ESelectNamesRenderer *renderer)
{
	g_return_val_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer), NULL);

	if (!renderer->priv->editable)
		return NULL;

	return e_select_names_editable_get_destination (renderer->priv->editable);
}

const gchar *
e_select_names_renderer_get_name (ESelectNamesRenderer *renderer)
{
	g_return_val_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer), NULL);

	return renderer->priv->name;
}

void
e_select_names_renderer_set_name (ESelectNamesRenderer *renderer,
                                  const gchar *name)
{
	g_return_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer));

	g_free (renderer->priv->name);
	renderer->priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (renderer), "name");
}

const gchar *
e_select_names_renderer_get_email (ESelectNamesRenderer *renderer)
{
	g_return_val_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer), NULL);

	return renderer->priv->email;
}

void
e_select_names_renderer_set_email (ESelectNamesRenderer *renderer,
                                   const gchar *email)
{
	g_return_if_fail (E_IS_SELECT_NAMES_RENDERER (renderer));

	g_free (renderer->priv->email);
	renderer->priv->email = g_strdup (email);

	g_object_notify (G_OBJECT (renderer), "email");
}

