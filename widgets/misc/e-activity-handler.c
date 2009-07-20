/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-activity-handler.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <misc/e-popup-menu.h>

#define ICON_SIZE 16

struct _ActivityInfo {
	gchar *component_id;
	gint error_type;
	guint id;
	gchar *information;
	gboolean cancellable;
	double progress;
	GtkWidget *menu;
	void (*cancel_func) (gpointer data);
	gpointer data;
	gpointer error;
	time_t	error_time;
};
typedef struct _ActivityInfo ActivityInfo;

struct _EActivityHandlerPrivate {
	guint next_activity_id;
	GList *activity_infos;
	GSList *task_bars;
	ELogger *logger;
	guint error_timer;
	guint error_flush_interval;

};

/* In the status bar, we show only errors and info. Errors are pictured as warnings. */
const gchar *icon_data [] = {"dialog-warning", "dialog-information"};

G_DEFINE_TYPE (EActivityHandler, e_activity_handler, G_TYPE_OBJECT)

/* Utility functions.  */

static void handle_error (ETaskWidget *task);

static guint
get_new_activity_id (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv;

	priv = activity_handler->priv;

	return priv->next_activity_id ++;
}

static GList *
lookup_activity (GList *list,
		 guint activity_id,
		 gint *order_number_return)
{
	GList *p;
	gint i;

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

static gint
task_widget_button_press_event_callback (GtkWidget *widget,
					 GdkEventButton *button_event,
					 gpointer data)
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
activity_info_new (const gchar *component_id,
		   guint id,
		   const gchar *information,
		   gboolean cancellable)
{
	ActivityInfo *info;

	info = g_new (ActivityInfo, 1);
	info->component_id   = g_strdup (component_id);
	info->id             = id;
	info->information    = g_strdup (information);
	info->cancellable    = cancellable;
	info->progress       = -1.0; /* (Unknown) */
	info->menu           = NULL;
	info->error          = NULL;
	info->cancel_func    = NULL;

	return info;
}

static void
activity_info_free (ActivityInfo *info)
{
	g_free (info->component_id);
	g_free (info->information);

	if (info->menu != NULL)
		gtk_widget_destroy (info->menu);

	g_free (info);
}

static ETaskWidget *
task_widget_new_from_activity_info (ActivityInfo *activity_info)
{
	GtkWidget *widget;
	ETaskWidget *etw;

	widget = e_task_widget_new_with_cancel (
		activity_info->component_id,
		activity_info->information,
		activity_info->cancel_func,
		activity_info->data);
	etw = (ETaskWidget *) widget;
	etw->id = activity_info->id;
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
		ActivityInfo *info = p->data;
		ETaskWidget *task_widget = task_widget_new_from_activity_info (info);
		task_widget->id = info->id;
		e_task_bar_prepend_task (task_bar, task_widget);
		if (info->error) {
			/* Prepare to handle existing errors*/
			GtkWidget *tool;
			const gchar *stock;

			stock = info->error_type ? icon_data [1] : icon_data[0];
			tool = e_task_widget_update_image (task_widget, (gchar *)stock, info->information);
			g_object_set_data ((GObject *) task_widget, "tool", tool);
			g_object_set_data ((GObject *) task_widget, "error", info->error);
			g_object_set_data ((GObject *) task_widget, "activity-handler", activity_handler);
			g_object_set_data ((GObject *) task_widget, "activity", GINT_TO_POINTER(info->id));
			g_object_set_data ((GObject *) task_widget, "error-type", GINT_TO_POINTER(info->error_type));
			g_signal_connect_swapped (tool, "clicked", G_CALLBACK(handle_error), task_widget);
		}
	}
}

static void
task_bar_destroy_notify (gpointer data,
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
	priv->logger	       = NULL;
	priv->error_timer      = 0;
	priv->error_flush_interval = 0;
	activity_handler->priv = priv;
}

EActivityHandler *
e_activity_handler_new (void)
{
	return g_object_new (e_activity_handler_get_type (), NULL);
}

void
e_activity_handler_set_error_flush_time (EActivityHandler *handler, gint time)
{
	handler->priv->error_flush_interval = time;
}
void
e_activity_handler_set_logger (EActivityHandler *handler, ELogger *logger)
{
	handler->priv->logger = logger;
}

void
e_activity_handler_set_message (EActivityHandler *activity_handler,
				const gchar       *message)
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

struct _cancel_wdata {
	EActivityHandler *handler;
	ActivityInfo *info;
	guint id;
	void (*cancel)(gpointer);
	gpointer data;
};

static void
cancel_wrapper (gpointer pdata)
{
	struct _cancel_wdata *data = (struct _cancel_wdata *) pdata;
	/* This can be invoked in two scenario. Either to cancel or to hide error */
	if (data->info->error) {
		/* Hide the error */
		EActivityHandler *handler = data->handler;
		ActivityInfo *info;
		gint order, len;
		GSList *sp;
		GList *p = lookup_activity (handler->priv->activity_infos, data->id, &order);
		e_logger_log (handler->priv->logger, E_LOG_ERROR, g_object_get_data (data->info->error, "primary"),
						    g_object_get_data (data->info->error, "secondary"));
		gtk_widget_destroy (data->info->error);
		data->info->error = NULL;
		info = data->info;
		for (sp = handler->priv->task_bars; sp != NULL; sp = sp->next) {
			ETaskBar *task_bar;

			task_bar = E_TASK_BAR (sp->data);
			e_task_bar_remove_task_from_id (task_bar, info->id);
		}
		activity_info_free (info);
		len = g_list_length (handler->priv->activity_infos);
		handler->priv->activity_infos = g_list_remove_link (handler->priv->activity_infos, p);
		if (len == 1)
			handler->priv->activity_infos = NULL;
	} else {
		/* Cancel the operation */
		data->cancel (data->data);
	}
	/* No need to free the data. It will be freed as part of the task widget destroy */
}

/* CORBA methods.  */
guint  e_activity_handler_cancelable_operation_started  (EActivityHandler *activity_handler,
						      const gchar       *component_id,
						      const gchar       *information,
						      gboolean          cancellable,
						      void (*cancel_func)(gpointer),
						      gpointer user_data)
{
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	guint activity_id;
	GSList *p;
	struct _cancel_wdata *data;
	gboolean bfree = FALSE;
	priv = activity_handler->priv;

	activity_id = get_new_activity_id (activity_handler);
	activity_info = activity_info_new (component_id, activity_id, information, cancellable);

	data = g_new(struct _cancel_wdata, 1);
	data->handler = activity_handler;
	data->id = activity_id;
	data->info = activity_info;
	data->cancel = cancel_func;
	data->data = user_data;

	activity_info->cancel_func = cancel_wrapper;
	activity_info->data = data;
	for (p = priv->task_bars; p != NULL; p = p->next) {
		ETaskWidget *tw = task_widget_new_from_activity_info (activity_info);
		tw->id = activity_id;
		if (!bfree) {
			/* The data will be freed part of the widget destroy */
			g_object_set_data_full ((GObject *) tw, "free-data", data, g_free);
			bfree = TRUE;
		}
		e_task_bar_prepend_task (E_TASK_BAR (p->data), tw);
	}

	priv->activity_infos = g_list_prepend (priv->activity_infos, activity_info);

	return activity_id;

}

guint
e_activity_handler_operation_started (EActivityHandler *activity_handler,
				      const gchar *component_id,
				      const gchar *information,
				      gboolean cancellable)
{
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	guint activity_id;
	GSList *p;

	priv = activity_handler->priv;

	activity_id = get_new_activity_id (activity_handler);

	activity_info = activity_info_new (component_id, activity_id, information, cancellable);

	for (p = priv->task_bars; p != NULL; p = p->next) {
		ETaskWidget *tw = task_widget_new_from_activity_info (activity_info);
		tw->id = activity_id;
		e_task_bar_prepend_task (E_TASK_BAR (p->data), tw);
	}

	priv->activity_infos = g_list_prepend (priv->activity_infos, activity_info);

	return activity_id;
}

static void
handle_error (ETaskWidget *task)
{
	GtkWidget *tool, *error;
	EActivityHandler *activity_handler;
	guint id;
	gint error_type  = GPOINTER_TO_INT((g_object_get_data ((GObject *) task, "error-type")));
	tool = g_object_get_data ((GObject *) task, "tool");
	error = g_object_get_data ((GObject *) task, "error");
	activity_handler = g_object_get_data ((GObject *) task, "activity-handler");
	id = GPOINTER_TO_UINT (g_object_get_data ((GObject *) task, "activity"));
	e_activity_handler_operation_finished (activity_handler, id);
	gtk_widget_show (error);
	e_logger_log (activity_handler->priv->logger, error_type,
		      g_object_get_data ((GObject *) error, "primary"),
				    g_object_get_data ((GObject *) error, "secondary"));
}

static gboolean
error_cleanup (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv = activity_handler->priv;
	GList *p, *node;
	GSList *sp;
	gint i;
	time_t now = time (NULL);
	gboolean berror = FALSE;

	for (p = priv->activity_infos, i = 0; p != NULL; i++) {
		ActivityInfo *info;

		info = (ActivityInfo *) p->data;
		if (info->error)
			berror = TRUE;
		if (info->error && info->error_time && (now - info->error_time) > 5 ) {
			/* Error older than wanted time. So cleanup */
			e_logger_log (priv->logger, info->error_type, g_object_get_data (info->error, "primary"),
						    g_object_get_data (info->error, "secondary"));

			if (GTK_IS_DIALOG (info->error))
				gtk_dialog_response (GTK_DIALOG (info->error), GTK_RESPONSE_CLOSE);
			else
				gtk_widget_destroy (info->error);

			node = p;
			p = p->next;

			for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
				ETaskBar *task_bar;

				task_bar = E_TASK_BAR (sp->data);
				e_task_bar_remove_task_from_id (task_bar, info->id);
			}
			activity_info_free (info);
			priv->activity_infos = g_list_remove_link (priv->activity_infos, node);

		} else
			p = p->next;
	}
	if (!berror)
		priv->error_timer = 0;
	return berror;
}

static gboolean
show_intrusive_errors (void)
{
	const gchar *intrusive = NULL;

	intrusive = g_getenv ("EVO-SHOW-INTRUSIVE-ERRORS");

	if (intrusive && g_str_equal (intrusive, "1"))
		return TRUE;
	else
		return FALSE;
}

guint
e_activity_handler_make_error (EActivityHandler *activity_handler,
				      const gchar *component_id,
				      gint error_type,
				      GtkWidget  *error)
{
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	guint activity_id;
	GSList *p;
	gchar *information = g_object_get_data((GObject *) error, "primary");
	const gchar *img;

	priv = activity_handler->priv;
	activity_id = get_new_activity_id (activity_handler);

	if (show_intrusive_errors ()) {
		gtk_widget_show (error);
		return activity_id;
	}

	activity_info = activity_info_new (component_id, activity_id, information, TRUE);
	activity_info->error = error;
	activity_info->error_time = time (NULL);
	activity_info->error_type = error_type;

	img = error_type ? icon_data[1] : icon_data[0];
	for (p = priv->task_bars; p != NULL; p = p->next) {
		ETaskBar *task_bar;
		ETaskWidget *task_widget;
		GtkWidget *tool;

		task_bar = E_TASK_BAR (p->data);
		task_widget = task_widget_new_from_activity_info (activity_info);
		task_widget->id = activity_id;
		e_task_bar_prepend_task (E_TASK_BAR (p->data), task_widget);

		tool = e_task_widget_update_image (task_widget, (gchar *)img, information);
		g_object_set_data ((GObject *) task_widget, "tool", tool);
		g_object_set_data ((GObject *) task_widget, "error", error);
		g_object_set_data ((GObject *) task_widget, "activity-handler", activity_handler);
		g_object_set_data ((GObject *) task_widget, "activity", GINT_TO_POINTER(activity_id));
		g_object_set_data ((GObject *) task_widget, "error-type", GINT_TO_POINTER(error_type));
		g_signal_connect_swapped (tool, "clicked", G_CALLBACK(handle_error), task_widget);
	}

	priv->activity_infos = g_list_prepend (priv->activity_infos, activity_info);

	if (!activity_handler->priv->error_timer)
		activity_handler->priv->error_timer = g_timeout_add (activity_handler->priv->error_flush_interval, (GSourceFunc)error_cleanup, activity_handler);

	return activity_id;
}

void
e_activity_handler_operation_set_error(EActivityHandler *activity_handler,
					  guint activity_id,
					  GtkWidget *error)
{
	EActivityHandlerPrivate *priv = activity_handler->priv;
	ActivityInfo *activity_info;
	GList *p;
	GSList *sp;
	gint order_number;

	p = lookup_activity (priv->activity_infos, activity_id, &order_number);
	if (p == NULL) {
		g_warning ("EActivityHandler: unknown operation %d", activity_id);
		return;
	}

	activity_info = (ActivityInfo *) p->data;
	activity_info->error = error;
	activity_info->error_time = time (NULL);
	activity_info->error_type = E_LOG_ERROR;
	g_free (activity_info->information);
	activity_info->information = g_strdup (g_object_get_data ((GObject *) error, "primary"));
	for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
		ETaskBar *task_bar;
		ETaskWidget *task_widget;
		GtkWidget *tool;

		task_bar = E_TASK_BAR (sp->data);
		task_widget = e_task_bar_get_task_widget_from_id (task_bar, activity_info->id);
		if (!task_widget)
			continue;

		tool = e_task_widget_update_image (task_widget, (gchar *)icon_data[0], g_object_get_data ((GObject *) error, "primary"));
		g_object_set_data ((GObject *) task_widget, "tool", tool);
		g_object_set_data ((GObject *) task_widget, "error", error);
		g_object_set_data ((GObject *) task_widget, "activity-handler", activity_handler);
		g_object_set_data ((GObject *) task_widget, "activity", GINT_TO_POINTER(activity_id));
		g_object_set_data ((GObject *) task_widget, "error-type", GINT_TO_POINTER(E_LOG_ERROR));
		g_signal_connect_swapped (tool, "clicked", G_CALLBACK(handle_error), task_widget);
	}

	if (!activity_handler->priv->error_timer)
		activity_handler->priv->error_timer = g_timeout_add (activity_handler->priv->error_flush_interval, (GSourceFunc) error_cleanup, activity_handler);
}

void
e_activity_handler_operation_progressing (EActivityHandler *activity_handler,
					  guint activity_id,
					  const gchar *information,
					  double progress)
{
	EActivityHandlerPrivate *priv = activity_handler->priv;
	ActivityInfo *activity_info;
	GList *p;
	GSList *sp;
	gint order_number;

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
		task_widget = e_task_bar_get_task_widget_from_id (task_bar, activity_info->id);
		if (!task_widget)
			continue;

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
	gint order_number;

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
		e_task_bar_remove_task_from_id (task_bar, activity_id);
	}
}

