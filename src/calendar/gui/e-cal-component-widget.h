/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_COMPONENT_WIDGET_H
#define E_CAL_COMPONENT_WIDGET_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>

G_BEGIN_DECLS

#define E_TYPE_CAL_COMPONENT_WIDGET e_cal_component_widget_get_type ()

G_DECLARE_FINAL_TYPE (ECalComponentWidget, e_cal_component_widget, E, CAL_COMPONENT_WIDGET, GtkBox)

GtkWidget *	e_cal_component_widget_new	(ECalClient *client,
						 ECalComponent *component,
						 ESourceRegistry *registry);
ECalClient *	e_cal_component_widget_get_client
						(ECalComponentWidget *self);
ECalComponent *	e_cal_component_widget_get_component
						(ECalComponentWidget *self);
void		e_cal_component_widget_update_component
						(ECalComponentWidget *self,
						 ECalClient *client,
						 ECalComponent *component);
ESourceRegistry *
		e_cal_component_widget_get_registry
						(ECalComponentWidget *self);
gboolean	e_cal_component_widget_get_time_visible
						(ECalComponentWidget *self);
void		e_cal_component_widget_set_time_visible
						(ECalComponentWidget *self,
						 gboolean value);
gboolean	e_cal_component_widget_get_with_transparency
						(ECalComponentWidget *self);
void		e_cal_component_widget_set_with_transparency
						(ECalComponentWidget *self,
						 gboolean value);
void		e_cal_component_widget_update	(ECalComponentWidget *self);

G_END_DECLS

#endif /* E_CAL_COMPONENT_WIDGET_H */
