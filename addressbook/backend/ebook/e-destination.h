/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-destination.h
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

#ifndef __E_DESTINATION_H__
#define __E_DESTINATION_H__

#include <gtk/gtkobject.h>
#include <addressbook/backend/ebook/e-card.h>

#define E_TYPE_DESTINATION        (e_destination_get_type ())
#define E_DESTINATION(o)          (GTK_CHECK_CAST ((o), E_TYPE_DESTINATION, EDestination))
#define E_DESTINATION_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_TYPE_DESTINATION, EDestinationClass))
#define E_IS_DESTINATION(o)       (GTK_CHECK_TYPE ((o), E_TYPE_DESTINATION))
#define E_IS_DESTINATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TYPE_DESTINATION))

typedef struct _EDestination EDestination;
typedef struct _EDestinationClass EDestinationClass;

struct _EDestinationPrivate;

struct _EDestination {
	GtkObject object;

	struct _EDestinationPrivate *priv;
};

struct _EDestinationClass {
	GtkObjectClass parent_class;
};

GtkType e_destination_get_type (void);

EDestination *e_destination_new (void);
EDestination *e_destination_copy (EDestination *);

gboolean e_destination_is_empty (EDestination *);

void e_destination_set_card   (EDestination *, ECard *card, gint email_num);
void e_destination_set_string (EDestination *, const gchar *string);

ECard       *e_destination_get_card      (const EDestination *);
gint         e_destination_get_email_num (const EDestination *);
const gchar *e_destination_get_string    (const EDestination *);
gint         e_destination_get_strlen    (const EDestination *); /* a convenience function... */

const gchar *e_destination_get_name          (const EDestination *);

const gchar *e_destination_get_email         (const EDestination *);
const gchar *e_destination_get_email_verbose (const EDestination *);



#endif /* __E_DESTINATION_H__ */

