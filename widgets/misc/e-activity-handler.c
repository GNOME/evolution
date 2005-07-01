/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-activity-handler.c
 *
 * Copyright (C) 2001, 2002, 2003 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-activity-handler.h"

#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-popup-menu.h>

#include <gal/widgets/e-popup-menu.h>

#define ICON_SIZE 16


struct _ActivityInfo {
	char *component_id;
	GdkPixbuf *icon_pixbuf;
	guint id;
	char *information;
	gboolean cancellable;
	double progress;
	GtkWidget *menu;
};
typedef struct _ActivityInfo ActivityInfo;

struct _EActivityHandlerPrivate {
	guint next_activity_id;
	GList *activity_infos;
	GSList *task_bars;
};

G_DEFINE_TYPE (EActivityHandler, e_activity_handler, G_TYPE_OBJECT)

/* Utility functions.  */

static unsigned int
get_new_activity_id (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv;

	priv = activity_handler->priv;

	return priv->next_activity_id ++;
}

static GList *
lookup_activity (GList *list,
		 guint activity_id,
		 int *order_number_return)
{
	GList *p;
	int i;

	for (p = list, i = 0; p != NULL; p = p->next, i ++) {
		ActivityInfo *activity_info;

		activity_info = (ActivityInfo *) p->data;
		if (activity_info->id == activity_id) {
			*order_number_return = i;
			return p;
		}
	}

	*order_number_return = -1;
	return NULL;
}


/* ETaskWidget actions.  */

static int
task_widget_button_press_event_callback (GtkWidget *widget,
					 GdkEventButton *button_event,
					 void *data)
{
	ActivityInfo *activity_info;

	activity_info = (ActivityInfo *) data;

	if (button_event->button == 3)
		return activity_info->cancellable;

	if (button_event->button != 1)
		return FALSE;

	return TRUE;
}


/* Creating and destroying ActivityInfos.  */

static ActivityInfo *
activity_info_new (const char *component_id,
		   guint id,
		   GdkPixbuf *icon,
		   const char *information,
		   gboolean cancellable)
{
	ActivityInfo *info;

	info = g_new (ActivityInfo, 1);
	info->component_id   = g_strdup (component_id);
	info->id             = id;
	info->icon_pixbuf    = g_object_ref (icon);
	info->information    = g_strdup (information);
	info->cancellable    = cancellable;
	info->progress       = -1.0; /* (Unknown) */
	info->menu           = NULL;

	return info;
}

static void
activity_info_free (ActivityInfo *info)
{
	g_free (info->component_id);

	g_object_unref (info->icon_pixbuf);
	g_free (info->information);

	if (info->menu != NULL)
		gtk_widget_destroy (info->menu);

	g_free (info);
}

static ETaskWidget *
task_widget_new_from_activity_info (ActivityInfo *activity_info)
{
	GtkWidget *widget;

	widget = e_task_widget_new (activity_info->icon_pixbuf,
				    activity_info->component_id,
				    activity_info->information);
	gtk_widget_show (widget);

	g_signal_connect (widget, "button_press_event",
			  G_CALLBACK (task_widget_button_press_event_callback),
			  activity_info);

	return E_TASK_WIDGET (widget);
}


/* Task Bar handling.  */

static void
setup_task_bar (EActivityHandler *activity_handler,
		ETaskBar *task_bar)
{
	EActivityHandlerPrivate *priv;
	GList *p;

	priv = activity_handler->priv;

	for (p = g_list_last (priv->activity_infos); p != NULL; p = p->prev) {
		e_task_bar_prepend_task (task_bar,
					 task_widget_new_from_activity_info ((ActivityInfo *) p->data));
	}
}

static void
task_bar_destroy_notify (void *data,
			 GObject *task_bar_instance)
{
	EActivityHandler *activity_handler;
	EActivityHandlerPrivate *priv;

	activity_handler = E_ACTIVITY_HANDLER (data);
	priv = activity_handler->priv;

	priv->task_bars = g_slist_remove (priv->task_bars, task_bar_instance);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EActivityHandler *handler;
	EActivityHandlerPrivate *priv;
	GList *p;
	GSList *sp;

	handler = E_ACTIVITY_HANDLER (object);
	priv = handler->priv;

	for (p = priv->activity_infos; p != NULL; p = p->next) {
		ActivityInfo *info;

		info = (ActivityInfo *) p->data;
		activity_info_free (info);
	}

	g_list_free (priv->activity_infos);
	priv->activity_infos = NULL;

	for (sp = priv->task_bars; sp != NULL; sp = sp->next)
		g_object_weak_unref (G_OBJECT (sp->data), task_bar_destroy_notify, handler);
	priv->task_bars = NULL;

	(* G_OBJECT_CLASS (e_activity_handler_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EActivityHandler *handler;
	EActivityHandlerPrivate *priv;

	handler = E_ACTIVITY_HANDLER (object);
	priv = handler->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_activity_handler_parent_class)->finalize) (object);
}

static void
e_activity_handler_class_init (EActivityHandlerClass *activity_handler_class)
{
	GObjectClass *object_class = (GObjectClass *) activity_handler_class;
	
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
e_activity_handler_init (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv;

	priv = g_new (EActivityHandlerPrivate, 1);
	priv->next_activity_id = 1;
	priv->activity_infos   = NULL;
	priv->task_bars        = NULL;

	activity_handler->priv = priv;
}


EActivityHandler *
e_activity_handler_new (void)
{
	return g_object_new (e_activity_handler_get_type (), 0);
}

void
e_activity_handler_set_message (EActivityHandler *activity_handler,
				const char       *message)
{
	EActivityHandlerPrivate *priv;
	GSList *i;

	priv = activity_handler->priv;

	for (i = priv->task_bars; i; i = i->next)
		e_task_bar_set_message (E_TASK_BAR (i->data), message);
}

void
e_activity_handler_unset_message (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv;
	GSList *i;

	priv = activity_handler->priv;

	for (i = priv->task_bars; i; i = i->next)
		e_task_bar_unset_message (E_TASK_BAR (i->data));
}

void
e_activity_handler_attach_task_bar (EActivityHandler *activity_handler,
				    ETaskBar *task_bar)
{
	EActivityHandlerPrivate *priv;

	g_return_if_fail (activity_handler != NULL);
	g_return_if_fail (E_IS_ACTIVITY_HANDLER (activity_handler));
	g_return_if_fail (task_bar != NULL);
	g_return_if_fail (E_IS_TASK_BAR (task_bar));

	priv = activity_handler->priv;

	g_object_weak_ref (G_OBJECT (task_bar), task_bar_destroy_notify, activity_handler);

	priv->task_bars = g_slist_prepend (priv->task_bars, task_bar);

	setup_task_bar (activity_handler, task_bar);
}

/* CORBA methods.  */

guint
e_activity_handler_operation_started (EActivityHandler *activity_handler,
				      const char *component_id,
				      GdkPixbuf *icon_pixbuf,
				      const char *information,
				      gboolean cancellable)
{
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	unsigned int activity_id;
	GSList *p;

	priv = activity_handler->priv;

	activity_id = get_new_activity_id (activity_handler);

	activity_info = activity_info_new (component_id, activity_id, icon_pixbuf, information, cancellable);

	for (p = priv->task_bars; p != NULL; p = p->next) {
		e_task_bar_prepend_task (E_TASK_BAR (p->data),
					 task_widget_new_from_activity_info (activity_info));
	}

	priv->activity_infos = g_list_prepend (priv->activity_infos, activity_info);

	return activity_id;
}

void
e_activity_handler_operation_progressing (EActivityHandler *activity_handler,
					  guint activity_id,
					  const char *information,
					  double progress)
{
	EActivityHandlerPrivate *priv = activity_handler->priv;
	ActivityInfo *activity_info;
	GList *p;
	GSList *sp;
	int order_number;

	p = lookup_activity (priv->activity_infos, activity_id, &order_number);
	if (p == NULL) {
		g_warning ("EActivityHandler: unknown operation %d", activity_id);
		return;
	}

	activity_info = (ActivityInfo *) p->data;

	g_free (activity_info->information);
	activity_info->information = g_strdup (information);

	activity_info->progress = progress;

	for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
		ETaskBar *task_bar;
		ETaskWidget *task_widget;

		task_bar = E_TASK_BAR (sp->data);
		task_widget = e_task_bar_get_task_widget (task_bar, order_number);

		e_task_widget_update (task_widget, information, progress);
	}
}

void
e_activity_handler_operation_finished (EActivityHandler *activity_handler,
				       guint activity_id)
{
	EActivityHandlerPrivate *priv = activity_handler->priv;
	GList *p;
	GSList *sp;
	int order_number;

	p = lookup_activity (priv->activity_infos, activity_id, &order_number);
	if (p == NULL) {
		g_warning ("e_activity_handler_operation_finished: Unknown activity %d\n", activity_id);
		return;
	}

	activity_info_free ((ActivityInfo *) p->data);
	priv->activity_infos = g_list_remove_link (priv->activity_infos, p);

	for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
		ETaskBar *task_bar;

		task_bar = E_TASK_BAR (sp->data);
		e_task_bar_remove_task (task_bar, order_number);
	}
}

