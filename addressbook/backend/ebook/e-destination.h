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
#include <addressbook/backend/ebook/e-book.h>
#include <gnome-xml/tree.h>

#define E_TYPE_DESTINATION        (e_destination_get_type ())
#define E_DESTINATION(o)          (GTK_CHECK_CAST ((o), E_TYPE_DESTINATION, EDestination))
#define E_DESTINATION_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_TYPE_DESTINATION, EDestinationClass))
#define E_IS_DESTINATION(o)       (GTK_CHECK_TYPE ((o), E_TYPE_DESTINATION))
#define E_IS_DESTINATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TYPE_DESTINATION))

typedef struct _EDestination EDestination;
typedef struct _EDestinationClass EDestinationClass;

typedef void (*EDestinationCardCallback) (EDestination *dest, ECard *card, gpointer closure);

struct _EDestinationPrivate;

struct _EDestination {
	GtkObject object;

	struct _EDestinationPrivate *priv;
};

struct _EDestinationClass {
	GtkObjectClass parent_class;
};

GtkType e_destination_get_type (void);


EDestination  *e_destination_new                (void);
EDestination  *e_destination_copy               (const EDestination *);
void           e_destination_clear              (EDestination *);

gboolean       e_destination_is_empty           (EDestination *);

void           e_destination_set_card           (EDestination *, ECard *card, gint email_num);
void           e_destination_set_card_uri       (EDestination *, const gchar *uri, gint email_num);

void           e_destination_set_name           (EDestination *, const gchar *name);
void           e_destination_set_email          (EDestination *, const gchar *email);

void           e_destination_set_string         (EDestination *, const gchar *string);
void           e_destination_set_html_mail_pref (EDestination *, gboolean);

gboolean       e_destination_contains_card      (const EDestination *);
gboolean       e_destination_from_card          (const EDestination *);

void           e_destination_use_card           (EDestination *, EDestinationCardCallback cb, gpointer closure);

ECard         *e_destination_get_card           (const EDestination *);
const gchar   *e_destination_get_card_uri       (const EDestination *);
gint           e_destination_get_email_num      (const EDestination *);

const gchar   *e_destination_get_name           (const EDestination *);  /* "Jane Smith" */
const gchar   *e_destination_get_email          (const EDestination *);  /* "jane@assbarn.com" */
const gchar   *e_destination_get_address        (const EDestination *);  /* "Jane Smith <jane@assbarn.com>" (or a comma-sep set of such for a list) */

const gchar   *e_destination_get_textrep        (const EDestination *);  /* "Jane Smith" or "jane@assbarn.com" */

gboolean       e_destination_is_evolution_list  (const EDestination *);

/* If true, they want HTML mail. */
gboolean       e_destination_get_html_mail_pref (const EDestination *);

gchar         *e_destination_get_address_textv  (EDestination **);

xmlNodePtr     e_destination_xml_encode         (const EDestination *dest);
gboolean       e_destination_xml_decode         (EDestination *dest, xmlNodePtr node);

gchar         *e_destination_export             (const EDestination *);
EDestination  *e_destination_import             (const gchar *str);

gchar         *e_destination_exportv            (EDestination **);
EDestination **e_destination_importv            (const gchar *str);

void           e_destination_touch              (EDestination *);


#endif /* __E_DESTINATION_H__ */

