/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-popup-control.h
 *
 * Copyright (C) 2001-2003, Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifndef __EAB_POPUP_CONTROL_H__
#define __EAB_POPUP_CONTROL_H__

#include <bonobo/bonobo-event-source.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <gtk/gtkeventbox.h>

G_BEGIN_DECLS

#define EAB_TYPE_POPUP_CONTROL        (eab_popup_control_get_type ())
#define EAB_POPUP_CONTROL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EAB_TYPE_POPUP_CONTROL, EABPopupControl))
#define EAB_POPUP_CONTROL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EAB_TYPE_POPUP_CONTROL, EABPopupControlClass))
#define EAB_IS_POPUP_CONTROL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAB_TYPE_POPUP_CONTROL))
#define EAB_IS_POPUP_CONTROL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EAB_TYPE_POPUP_CONTROL))

typedef struct _EABPopupControl EABPopupControl;
typedef struct _EABPopupControlClass EABPopupControlClass;

struct _EABPopupControl {
	GtkEventBox parent;

	gchar *name;
	gchar *email;

	GtkWidget *name_widget;
	GtkWidget *email_widget;
	GtkWidget *query_msg;
	
	GtkWidget *main_vbox;
	GtkWidget *generic_view;
	GtkWidget *contact_display;

	gboolean transitory;

	guint scheduled_refresh;
	EBook *book;
	guint query_tag;
	gboolean multiple_matches;
	EContact *contact;

	BonoboEventSource *es;
};

struct _EABPopupControlClass {
	GtkEventBoxClass parent_class;
};

GType eab_popup_control_get_type (void);

void eab_popup_control_construct (EABPopupControl *);

BonoboControl *eab_popup_control_new (void);

G_END_DECLS

#endif /* __EAB_POPUP_CONTROL_H__ */

