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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <libgnome/gnome-defs.h>
#include <addressbook/backend/ebook/e-card.h>

BEGIN_GNOME_DECLS

#define E_ADDRESS_POPUP_TYPE        (e_address_popup_get_type ())
#define E_ADDRESS_POPUP(o)          (GTK_CHECK_CAST ((o), E_ADDRESS_POPUP_TYPE, EAddressPopup))
#define E_ADDRESS_POPUP_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_ADDRESS_POPUP_TYPE, EAddressPopupClass))
#define E_IS_ADDRESS_POPUP(o)       (GTK_CHECK_TYPE ((o), E_ADDRESS_POPUP_TYPE))
#define E_IS_ADDRESS_POPUP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ADDRESS_POPUP_TYPE))

typedef struct _EAddressPopup EAddressPopup;
typedef struct _EAddressPopupClass EAddressPopupClass;

struct _EAddressPopup {
	GtkEventBox parent;

	guint leave_timeout_tag;
	gboolean grabbed;

	gchar *name;
	gchar *email;

	GtkWidget *name_widget;
	GtkWidget *email_widget;
	GtkWidget *query_msg;
	
	GtkWidget *main_vbox;
	GtkWidget *generic_view;
	GtkWidget *minicard_view;

	guint query_tag;
	gboolean have_queried, multiple_matches;
	ECard *card;
};

struct _EAddressPopupClass {
	GtkEventBoxClass parent_class;
};

GtkType e_address_popup_get_type (void);

void e_address_popup_set_name  (EAddressPopup *, const gchar *name);
void e_address_popup_set_email (EAddressPopup *, const gchar *email);

void e_address_popup_construct (EAddressPopup *);
GtkWidget *e_address_popup_new (void);

void e_address_popup_factory_init (void);

END_GNOME_DECLS

#endif /* __E_ADDRESS_POPUP_H__ */

