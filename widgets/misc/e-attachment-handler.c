/*
 * e-attachment-handler.c
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

#include "e-attachment-handler.h"

#define E_ATTACHMENT_HANDLER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_HANDLER, EAttachmentHandlerPrivate))

struct _EAttachmentHandlerPrivate {
	gpointer view;  /* weak pointer */
};

enum {
	PROP_0,
	PROP_VIEW
};

G_DEFINE_TYPE (
	EAttachmentHandler,
	e_attachment_handler,
	G_TYPE_OBJECT)

static void
attachment_handler_set_view (EAttachmentHandler *handler,
                             EAttachmentView *view)
{
	g_return_if_fail (handler->priv->view == NULL);

	handler->priv->view = view;

	g_object_add_weak_pointer (
		G_OBJECT (view), &handler->priv->view);
}

static void
attachment_handler_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VIEW:
			attachment_handler_set_view (
				E_ATTACHMENT_HANDLER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_handler_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VIEW:
			g_value_set_object (
				value, e_attachment_handler_get_view (
				E_ATTACHMENT_HANDLER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_handler_constructed (GObject *object)
{
	/* This allows subclasses to chain up safely since GObject
	 * does not implement this method, and we might want to do
	 * something here in the future. */
}

static void
attachment_handler_dispose (GObject *object)
{
	EAttachmentHandlerPrivate *priv;

	priv = E_ATTACHMENT_HANDLER_GET_PRIVATE (object);

	if (priv->view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->view), &priv->view);
		priv->view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_handler_parent_class)->dispose (object);
}

static void
e_attachment_handler_class_init (EAttachmentHandlerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAttachmentHandlerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_handler_set_property;
	object_class->get_property = attachment_handler_get_property;
	object_class->constructed = attachment_handler_constructed;
	object_class->dispose = attachment_handler_dispose;

	g_object_class_install_property (
		object_class,
		PROP_VIEW,
		g_param_spec_object (
			"view",
			"View",
			NULL,
			E_TYPE_ATTACHMENT_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_attachment_handler_init (EAttachmentHandler *handler)
{
	handler->priv = E_ATTACHMENT_HANDLER_GET_PRIVATE (handler);
}

EAttachmentView *
e_attachment_handler_get_view (EAttachmentHandler *handler)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_HANDLER (handler), NULL);

	return E_ATTACHMENT_VIEW (handler->priv->view);
}

GdkDragAction
e_attachment_handler_get_drag_actions (EAttachmentHandler *handler)
{
	EAttachmentHandlerClass *class;

	g_return_val_if_fail (E_IS_ATTACHMENT_HANDLER (handler), 0);

	class = E_ATTACHMENT_HANDLER_GET_CLASS (handler);

	if (class->get_drag_actions != NULL)
		return class->get_drag_actions (handler);

	return 0;
}

const GtkTargetEntry *
e_attachment_handler_get_target_table (EAttachmentHandler *handler,
                                       guint *n_targets)
{
	EAttachmentHandlerClass *class;

	g_return_val_if_fail (E_IS_ATTACHMENT_HANDLER (handler), NULL);

	class = E_ATTACHMENT_HANDLER_GET_CLASS (handler);

	if (class->get_target_table != NULL)
		return class->get_target_table (handler, n_targets);

	if (n_targets != NULL)
		*n_targets = 0;

	return NULL;
}
