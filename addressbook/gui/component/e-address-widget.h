/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-address-widget.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
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

#ifndef __E_ADDRESS_WIDGET_H__
#define __E_ADDRESS_WIDGET_H__

#include <gtk/gtkeventbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/backend/ebook/e-card.h>

G_BEGIN_DECLS

#define E_TYPE_ADDRESS_WIDGET        (e_address_widget_get_type ())
#define E_ADDRESS_WIDGET(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_ADDRESS_WIDGET, EAddressWidget))
#define E_ADDRESS_WIDGET_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_ADDRESS_WIDGET, EAddressWidgetClass))
#define E_IS_ADDRESS_WIDGET(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_ADDRESS_WIDGET))
#define E_IS_ADDRESS_WIDGET_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_ADDRESS_WIDGET))

typedef struct _EAddressWidget EAddressWidget;
typedef struct _EAddressWidgetClass EAddressWidgetClass;

struct _EAddressWidget {
	GtkEventBox parent;
	
	gchar *name;
	gchar *email;
	
	GtkWidget *name_widget;
	GtkWidget *email_widget;
	GtkWidget *spacer;

	guint query_idle_tag;
	guint query_tag;

	ECard *card;
	gboolean known_email;
};

struct _EAddressWidgetClass {
	GtkEventBoxClass parent_class;
};

GType e_address_widget_get_type (void);

void e_address_widget_set_name  (EAddressWidget *, const gchar *name);
void e_address_widget_set_email (EAddressWidget *, const gchar *email);
void e_address_widget_set_text  (EAddressWidget *, const gchar *text);

void e_address_widget_construct (EAddressWidget *);
GtkWidget *e_address_widget_new (void);

BonoboControl *e_address_widget_new_control (void);

G_END_DECLS

#endif /* __E_ADDRESS_WIDGET_H__ */
