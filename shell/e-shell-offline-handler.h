/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-offline-handler.h
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

#ifndef _E_SHELL_OFFLINE_HANDLER_H_
#define _E_SHELL_OFFLINE_HANDLER_H_

#include <gtk/gtkobject.h>
#include <gtk/gtkwindow.h>

#include "e-component-registry.h"


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHELL_OFFLINE_HANDLER			(e_shell_offline_handler_get_type ())
#define E_SHELL_OFFLINE_HANDLER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_OFFLINE_HANDLER, EShellOfflineHandler))
#define E_SHELL_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_OFFLINE_HANDLER, EShellOfflineHandlerClass))
#define E_IS_SHELL_OFFLINE_HANDLER(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_OFFLINE_HANDLER))
#define E_IS_SHELL_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_OFFLINE_HANDLER))


typedef struct _EShellOfflineHandler        EShellOfflineHandler;
typedef struct _EShellOfflineHandlerPrivate EShellOfflineHandlerPrivate;
typedef struct _EShellOfflineHandlerClass   EShellOfflineHandlerClass;

struct _EShellOfflineHandler {
	GtkObject parent;

	EShellOfflineHandlerPrivate *priv;
};

struct _EShellOfflineHandlerClass {
	GtkObjectClass parent_class;

	/* This signal is emitted when the offline procedure starts, i.e. the
	   EShellOfflineHanlder starts contacting the components one-by-one
	   telling them to be prepared to go off-line. */
	void (* offline_procedure_started) (EShellOfflineHandler *offline_handler);

	/* This is emitted when the procedure is finished, and all the
	   components are all either off-line (@now_offline is %TRUE) or
	   on-line (@now_offline is %FALSE).  */
	void (* offline_procedure_finished) (EShellOfflineHandler *offline_hanlder,
					     gboolean now_offline);
};


GtkType               e_shell_offline_handler_get_type   (void);
void                  e_shell_offline_handler_construct  (EShellOfflineHandler *offline_handler,
							  EShell               *shell);
EShellOfflineHandler *e_shell_offline_handler_new        (EShell               *shell);

void  e_shell_offline_handler_put_components_offline  (EShellOfflineHandler *offline_handler,
						       GtkWindow            *parent_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_OFFLINE_HANDLER_H_ */
