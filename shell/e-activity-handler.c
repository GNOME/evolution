/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-activity-handler.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#include "e-shell-corba-icon-utils.h"

#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-popup-menu.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-popup-menu.h>

#include <bonobo/bonobo-exception.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;


#define ICON_SIZE 16


struct _ActivityInfo {
	char *component_id;
	GdkPixbuf *icon_pixbuf;
	GNOME_Evolution_Activity_ActivityId id;
	CORBA_char *information;
	CORBA_boolean cancellable;
	Bonobo_Listener event_listener;
	CORBA_float progress;
	GtkWidget *menu;
};
typedef struct _ActivityInfo ActivityInfo;

struct _EActivityHandlerPrivate {
	GNOME_Evolution_Activity_ActivityId next_activity_id;
	GList *activity_infos;
	GSList *task_bars;
};


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
		 GNOME_Evolution_Activity_ActivityId activity_id,
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

#if 0
static const CORBA_any *
get_corba_null_value (void)
{
	static CORBA_any *null_value = NULL;

	if (null_value == NULL) {
		null_value = CORBA_any__alloc ();
		null_value->_type = TC_null;
	}

	return null_value;
}

static void
report_task_event (ActivityInfo *activity_info,
		   const char *event_name)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_Listener_event (activity_info->event_listener, event_name, get_corba_null_value (), &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("EActivityHandler: Cannot event `%s' -- %s", event_name, ev._repo_id);

	CORBA_exception_free (&ev);
}
#endif


/* ETaskWidget actions.  */

#if 0
static void
task_widget_cancel_callback (GtkWidget *widget,
			     void *data)
{
	ActivityInfo *activity_info;

	activity_info = (ActivityInfo *) data;
	report_task_event (activity_info, "Cancel");
}

static void
task_widget_show_details_callback (GtkWidget *widget,
				   void *data)
{
	ActivityInfo *activity_info;

	activity_info = (ActivityInfo *) data;
	report_task_event (activity_info, "ShowDetails");
}

static void
show_cancellation_popup (ActivityInfo *activity_info,
			 GtkWidget *task_widget,
			 GdkEventButton *button_event)
{
	GtkMenu *popup;
	EPopupMenu items[] = {
		E_POPUP_MENU (N_("Show Details"), task_widget_show_details_callback, 0),
		E_POPUP_SEPARATOR,
		E_POPUP_MENU (N_("Cancel Operation"), task_widget_cancel_callback, 0),
		E_POPUP_TERMINATOR
	};

	/* FIXME: We should gray out things properly here.  */
	popup = e_popup_menu_create (items, 0, 0, activity_info);

	g_assert (activity_info->menu == NULL);
	activity_info->menu = GTK_WIDGET (popup);

	gnome_popup_menu_do_popup_modal (GTK_WIDGET (popup), NULL, NULL, button_event, activity_info);

	activity_info->menu = NULL;
}
#endif

static int
task_widget_button_press_event_callback (GtkWidget *widget,
					 GdkEventButton *button_event,
					 void *data)
{
	CORBA_Environment ev;
	ActivityInfo *activity_info;
	CORBA_any *null_value;

	activity_info = (ActivityInfo *) data;

	if (button_event->button == 3) {
		if (! activity_info->cancellable) {
			return FALSE;
		} else {
			/* show_cancellation_popup (activity_info, widget, button_event); */
			/* return TRUE; */
			return TRUE;
		}
	}

	if (button_event->button != 1)
		return FALSE;

	CORBA_exception_init (&ev);

	null_value = CORBA_any__alloc ();
	null_value->_type = TC_null;

	Bonobo_Listener_event (activity_info->event_listener, "Clicked", null_value, &ev);
	if (BONOBO_EX (&ev) != CORBA_NO_EXCEPTION)
		g_warning ("EActivityHandler: Cannot report `Clicked' event -- %s",
			   BONOBO_EX_REPOID (&ev));

	CORBA_free (null_value);
 
	CORBA_exception_free (&ev);

	return TRUE;
}


/* Creating and destroying ActivityInfos.  */

static ActivityInfo *
activity_info_new (const char *component_id,
		   GNOME_Evolution_Activity_ActivityId id,
		   GdkPixbuf *icon,
		   const CORBA_char *information,
		   CORBA_boolean cancellable,
		   const Bonobo_Listener event_listener)
{
	ActivityInfo *info;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	info = g_new (ActivityInfo, 1);
	info->component_id   = g_strdup (component_id);
	info->id             = id;
	info->icon_pixbuf    = g_object_ref (icon);
	info->information    = CORBA_string_dup (information);
	info->cancellable    = cancellable;
	info->event_listener = CORBA_Object_duplicate (event_listener, &ev);
	info->progress       = -1.0; /* (Unknown) */
	info->menu           = NULL;

	CORBA_exception_free (&ev);

	return info;
}

static void
activity_info_free (ActivityInfo *info)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	g_free (info->component_id);

	g_object_unref (info->icon_pixbuf);
	CORBA_free (info->information);
	CORBA_Object_release (info->event_listener, &ev);

	if (info->menu != NULL)
		gtk_widget_destroy (info->menu);

	g_free (info);

	CORBA_exception_free (&ev);
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
		g_object_weak_unref (G_OBJECT (sp->data), task_bar_destroy_notify, sp->data);
	priv->task_bars = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EActivityHandler *handler;
	EActivityHandlerPrivate *priv;

	handler = E_ACTIVITY_HANDLER (object);
	priv = handler->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* CORBA methods.  */

static void
impl_operationStarted (PortableServer_Servant servant,
		       const CORBA_char *component_id,
		       const GNOME_Evolution_AnimatedIcon *icon,
		       const CORBA_char *information,
		       const CORBA_boolean cancellable,
		       const Bonobo_Listener event_listener,
		       GNOME_Evolution_Activity_ActivityId *activity_id_return,
		       CORBA_boolean *suggest_display_return,
		       CORBA_Environment *ev)
{
	EActivityHandler *activity_handler;
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	GdkPixbuf *icon_pixbuf;
	unsigned int activity_id;
	GSList *p;

	activity_handler = E_ACTIVITY_HANDLER (bonobo_object_from_servant (servant));

	priv = activity_handler->priv;

	if (icon->_length == 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Activity_InvalidIcon, NULL);
		return;
	}

	if (icon->_length > 1)
		g_warning ("Animated icons are not supported for activities (yet).");

	icon_pixbuf = e_new_gdk_pixbuf_from_corba_icon (icon->_buffer, ICON_SIZE, ICON_SIZE);

	activity_id = get_new_activity_id (activity_handler);

	activity_info = activity_info_new (component_id, activity_id, icon_pixbuf, information,
					   cancellable, event_listener);

	for (p = priv->task_bars; p != NULL; p = p->next)
		e_task_bar_prepend_task (E_TASK_BAR (p->data),
					 task_widget_new_from_activity_info (activity_info));

	g_object_unref (icon_pixbuf);

	priv->activity_infos = g_list_prepend (priv->activity_infos, activity_info);

	*activity_id_return = activity_id;
}

static void
impl_operationProgressing (PortableServer_Servant servant,
			   const GNOME_Evolution_Activity_ActivityId activity_id,
			   const CORBA_char *information,
			   const CORBA_float progress,
			   CORBA_Environment *ev)
{
	EActivityHandler *activity_handler;
	EActivityHandlerPrivate *priv;
	ActivityInfo *activity_info;
	GList *p;
	GSList *sp;
	int order_number;

	/* FIXME?  The complexity in this function sucks.  */

	activity_handler = E_ACTIVITY_HANDLER (bonobo_object_from_servant (servant));

	priv = activity_handler->priv;

	p = lookup_activity (priv->activity_infos, activity_id, &order_number);
	if (p == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Activity_IdNotFound, NULL);
		return;
	}

	activity_info = (ActivityInfo *) p->data;

	CORBA_free (activity_info->information);
	activity_info->information = CORBA_string_dup (information);

	activity_info->progress = progress;

	for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
		ETaskBar *task_bar;
		ETaskWidget *task_widget;

		task_bar = E_TASK_BAR (sp->data);
		task_widget = e_task_bar_get_task_widget (task_bar, order_number);

		e_task_widget_update (task_widget, information, progress);
	}
}

static void
impl_operationFinished (PortableServer_Servant servant,
			const GNOME_Evolution_Activity_ActivityId activity_id,
			CORBA_Environment *ev)
{
	EActivityHandler *activity_handler;
	EActivityHandlerPrivate *priv;
	GList *p;
	GSList *sp;
	int order_number;

	activity_handler = E_ACTIVITY_HANDLER (bonobo_object_from_servant (servant));

	priv = activity_handler->priv;

	p = lookup_activity (priv->activity_infos, activity_id, &order_number);

	activity_info_free ((ActivityInfo *) p->data);
	priv->activity_infos = g_list_remove_link (priv->activity_infos, p);

	for (sp = priv->task_bars; sp != NULL; sp = sp->next) {
		ETaskBar *task_bar;

		task_bar = E_TASK_BAR (sp->data);
		e_task_bar_remove_task (task_bar, order_number);
	}
}

static GNOME_Evolution_Activity_DialogAction
impl_requestDialog (PortableServer_Servant servant,
		    const GNOME_Evolution_Activity_ActivityId activity_id,
		    const GNOME_Evolution_Activity_DialogType dialog_type,
		    CORBA_Environment *ev)
{
	EActivityHandler *activity_handler;

	activity_handler = E_ACTIVITY_HANDLER (bonobo_object_from_servant (servant));

	/* FIXME implement.  */
	g_warning ("Evolution::Activity::requestDialog not implemented");

	return GNOME_Evolution_Activity_DIALOG_ACTION_DISPLAY;
}


/* GTK+ type stuff.  */

static void
e_activity_handler_class_init (GObjectClass *object_class)
{
	EActivityHandlerClass *handler_class;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	handler_class = E_ACTIVITY_HANDLER_CLASS (object_class);
	handler_class->epv.operationStarted     = impl_operationStarted;
	handler_class->epv.operationProgressing = impl_operationProgressing;
	handler_class->epv.operationFinished    = impl_operationFinished;
	handler_class->epv.requestDialog        = impl_requestDialog;
}

static void
e_activity_handler_init (EActivityHandler *activity_handler)
{
	EActivityHandlerPrivate *priv;

	priv = g_new (EActivityHandlerPrivate, 1);
	priv->next_activity_id = 0;
	priv->activity_infos   = NULL;
	priv->task_bars        = NULL;

	activity_handler->priv = priv;
}


void
e_activity_handler_construct (EActivityHandler *activity_handler)
{
	g_return_if_fail (activity_handler != NULL);
	g_return_if_fail (E_IS_ACTIVITY_HANDLER (activity_handler));

	/* Nothing to do here.  */
}

EActivityHandler *
e_activity_handler_new (void)
{
	EActivityHandler *activity_handler;

	activity_handler = g_object_new (e_activity_handler_get_type (), 0);
	e_activity_handler_construct (activity_handler);

	return activity_handler;
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


BONOBO_TYPE_FUNC_FULL (EActivityHandler,
		       GNOME_Evolution_Activity,
		       PARENT_TYPE,
		       e_activity_handler)
