/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-address-popup.h
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

#ifndef __E_ADDRESS_POPUP_H__
#define __E_ADDRESS_POPUP_H__

#include <gtk/gtk.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-card.h>
#include <bonobo/bonobo-event-source.h>

G_BEGIN_DECLS

#define E_TYPE_ADDRESS_POPUP        (e_address_popup_get_type ())
#define E_ADDRESS_POPUP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_ADDRESS_POPUP, EAddressPopup))
#define E_ADDRESS_POPUP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_ADDRESS_POPUP, EAddressPopupClass))
#define E_IS_ADDRESS_POPUP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_ADDRESS_POPUP))
#define E_IS_ADDRESS_POPUP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_ADDRESS_POPUP))

typedef struct _EAddressPopup EAddressPopup;
typedef struct _EAddressPopupClass EAddressPopupClass;

struct _EAddressPopup {
	GtkEventBox parent;

	gchar *name;
	gchar *email;

	GtkWidget *name_widget;
	GtkWidget *email_widget;
	GtkWidget *query_msg;
	
	GtkWidget *main_vbox;
	GtkWidget *generic_view;
	GtkWidget *minicard_view;

	gboolean transitory;

	guint scheduled_refresh;
	EBook *book;
	guint query_tag;
	gboolean multiple_matches;
	ECard *card;

	BonoboEventSource *es;
};

struct _EAddressPopupClass {
	GtkEventBoxClass parent_class;
};

GType e_address_popup_get_type (void);

void e_address_popup_set_name  (EAddressPopup *, const gchar *name);
void e_address_popup_set_email (EAddressPopup *, const gchar *email);

void e_address_popup_construct (EAddressPopup *);
GtkWidget *e_address_popup_new (void);

void e_address_popup_factory_init (void);

G_END_DECLS

#endif /* __E_ADDRESS_POPUP_H__ */

