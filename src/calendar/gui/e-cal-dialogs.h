/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_CAL_DIALOGS_H
#define E_CAL_DIALOGS_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-calendar-view.h>

gboolean	e_cal_dialogs_delete_with_comment
						(GtkWindow *parent,
						 ECalClient *cal_client,
						 ECalComponent *comp,
						 gboolean organizer_is_user,
						 gboolean attendee_is_user,
						 gboolean *out_can_send_notice);
gboolean	e_cal_dialogs_cancel_component	(GtkWindow *parent,
						 ECalClient *cal_client,
						 ECalComponent *comp,
						 gboolean can_set_cancel_comment,
						 gboolean organizer_is_user);
void		e_cal_dialogs_copy_source	(GtkWindow *parent,
						 ECalModel *model,
						 ESource *from_source);
gboolean	e_cal_dialogs_delete_component	(ECalComponent *comp,
						 gboolean consider_as_untitled,
						 gint n_comps,
						 ECalComponentVType vtype,
						 GtkWidget *widget);
gboolean	e_cal_dialogs_prompt_retract	(GtkWidget *parent,
						 ECalComponent *comp,
						 gchar **retract_text,
						 gboolean *retract);
gboolean	e_cal_dialogs_goto_run		(GtkWindow *parent,
						 ECalDataModel *data_model,
						 const GDate *from_date,
						 ECalendarViewMoveType *out_move_type,
						 time_t *out_exact_date);
gboolean	e_cal_dialogs_recur_component	(ECalClient *client,
						 ECalComponent *comp,
						 ECalObjModType *mod,
						 GtkWindow *parent,
						 gboolean delegated);
gboolean	e_cal_dialogs_recur_icalcomp	(ECalClient *client,
						 ICalComponent *icomp,
						 ECalObjModType *mod,
						 GtkWindow *parent,
						 gboolean delegated);
ESource *	e_cal_dialogs_select_source	(GtkWindow *parent,
						 ESourceRegistry *registry,
						 ECalClientSourceType type,
						 ESource *except_source);
gboolean	e_cal_dialogs_send_component	(GtkWindow *parent,
						 ECalClient *client,
						 ECalComponent *comp,
						 gboolean new,
						 gboolean *strip_alarms,
						 gboolean *only_new_attendees);
GtkResponseType	e_cal_dialogs_send_dragged_or_resized_component
						(GtkWindow *parent,
						 ECalClient *client,
						 ECalComponent *comp,
						 gboolean *strip_alarms,
						 gboolean *only_new_attendees);
gboolean	e_cal_dialogs_send_component_prompt_subject
						(GtkWindow *parent,
						 ICalComponent *icomp);
gboolean	e_cal_dialogs_detach_and_copy	(GtkWindow *parent,
						 ICalComponent *icomp);

#endif /* E_CAL_DIALOGS_H */
