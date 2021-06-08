/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_BASE_SHELL_CONTENT_H
#define E_CAL_BASE_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-cal-data-model.h>
#include <calendar/gui/e-cal-model.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BASE_SHELL_CONTENT \
	(e_cal_base_shell_content_get_type ())
#define E_CAL_BASE_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BASE_SHELL_CONTENT, ECalBaseShellContent))
#define E_CAL_BASE_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BASE_SHELL_CONTENT, ECalBaseShellContentClass))
#define E_IS_CAL_BASE_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BASE_SHELL_CONTENT))
#define E_IS_CAL_BASE_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BASE_SHELL_CONTENT))
#define E_CAL_BASE_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BASE_SHELL_CONTENT, ECalBaseShellContentClass))

G_BEGIN_DECLS

typedef struct _ECalBaseShellContent ECalBaseShellContent;
typedef struct _ECalBaseShellContentClass ECalBaseShellContentClass;
typedef struct _ECalBaseShellContentPrivate ECalBaseShellContentPrivate;

enum {
	E_CAL_BASE_SHELL_CONTENT_SELECTION_SINGLE	  = 1 << 0,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_MULTIPLE	  = 1 << 1,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_EDITABLE	  = 1 << 2,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_INSTANCE	  = 1 << 3,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_MEETING	  = 1 << 4,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_ORGANIZER	  = 1 << 5,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_RECURRING	  = 1 << 6,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_CAN_DELEGATE	  = 1 << 7,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_CAN_ASSIGN	  = 1 << 8,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_COMPLETE	  = 1 << 9,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_INCOMPLETE = 1 << 10,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_URL	  = 1 << 11,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_ATTENDEE	  = 1 << 12,
	E_CAL_BASE_SHELL_CONTENT_SELECTION_THIS_AND_FUTURE_SUPPORTED = 1 << 13
};

struct _ECalBaseShellContent {
	EShellContent parent;
	ECalBaseShellContentPrivate *priv;
};

struct _ECalBaseShellContentClass {
	EShellContentClass parent_class;

	/* Virtual methods */

	/* To create correct ECalModel instance for the content */
	ECalModel *	(*new_cal_model)	(ECalDataModel *cal_data_model,
						 ESourceRegistry *registry,
						 EShell *shell);

	/* This is called when the shell view is fully created;
	   it can be used for further initialization of the object, when it needs other
	   sub-parts of the shell view. */
	void		(* view_created)	(ECalBaseShellContent *cal_base_shell_content);

	/* This is called when a user initiated a quit; implementors
	   should stop everything or pile on top of the activity */
	void		(* prepare_for_quit)	(ECalBaseShellContent *cal_base_shell_content,
						 EActivity *activity);
};

GType		e_cal_base_shell_content_get_type	(void);

ECalDataModel *	e_cal_base_shell_content_get_data_model	(ECalBaseShellContent *cal_base_shell_content);
ECalModel *	e_cal_base_shell_content_get_model	(ECalBaseShellContent *cal_base_shell_content);
void		e_cal_base_shell_content_prepare_for_quit
							(ECalBaseShellContent *cal_base_shell_content,
							 EActivity *activity);

ECalDataModel *	e_cal_base_shell_content_create_new_data_model
							(ECalBaseShellContent *cal_base_shell_content);

G_END_DECLS

#endif /* E_CAL_BASE_SHELL_CONTENT_H */
