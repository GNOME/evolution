/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-url-button.h
 *
 * Copyright (C) 2002  JP Rosevear
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: JP Rosevear
 */

#ifndef _E_URL_BUTTON_H_
#define _E_URL_BUTTON_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_URL_BUTTON			(e_url_button_get_type ())
#define E_URL_BUTTON(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_URL_BUTTON, EUrlButton))
#define E_URL_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_URL_BUTTON, EUrlButtonClass))
#define E_IS_URL_BUTTON(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_URL_BUTTON))
#define E_IS_URL_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_URL_BUTTON))


typedef struct _EUrlButton        EUrlButton;
typedef struct _EUrlButtonPrivate EUrlButtonPrivate;
typedef struct _EUrlButtonClass   EUrlButtonClass;

struct _EUrlButton {
	GtkButton parent;

	EUrlButtonPrivate *priv;
};

struct _EUrlButtonClass {
	GtkButtonClass parent_class;
};



GtkType    e_url_button_get_type  (void);
GtkWidget *e_url_button_new       (void);
void       e_url_button_set_entry (EUrlButton *url_button,
				   GtkWidget  *entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_URL_BUTTON_H_ */
