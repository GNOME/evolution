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

#ifndef __E_DESTINATION_H__
#define __E_DESTINATION_H__

#include <glib.h>
#include <glib-object.h>
#include <ebook/e-card.h>
#include <ebook/e-book.h>
#include <libxml/tree.h>

#define E_TYPE_DESTINATION           (e_destination_get_type ())
#define E_DESTINATION(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_DESTINATION, EDestination))
#define E_DESTINATION_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_DESTINATION, EDestinationClass))
#define E_IS_DESTINATION(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_DESTINATION))
#define E_IS_DESTINATION_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_DESTINATION))
#define E_DESTINATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_DESTINATION, EDestinationClass))

typedef struct _EDestination EDestination;
typedef struct _EDestinationClass EDestinationClass;

typedef void (*EDestinationCardCallback) (EDestination *dest, ECard *card, gpointer closure);

struct _EDestinationPrivate;

struct _EDestination {
	GObject object;

	struct _EDestinationPrivate *priv;
};

struct _EDestinationClass {
	GObjectClass parent_class;

	void (*changed) (EDestination *dest);	
	void (*cardified) (EDestination *dest);
};

GType e_destination_get_type (void);


EDestination  *e_destination_new                (void);
void           e_destination_changed            (EDestination *);
EDestination  *e_destination_copy               (const EDestination *);
void           e_destination_clear              (EDestination *);

gboolean       e_destination_is_empty           (const EDestination *);
gboolean       e_destination_is_valid           (const EDestination *);
gboolean       e_destination_equal              (const EDestination *a, const EDestination *b);

void           e_destination_set_card           (EDestination *, ECard *card, gint email_num);
void           e_destination_set_book_uri       (EDestination *, const gchar *uri); 
void           e_destination_set_card_uid       (EDestination *, const gchar *uid, gint email_num); 

void           e_destination_set_name           (EDestination *, const gchar *name);
void           e_destination_set_email          (EDestination *, const gchar *email);

void           e_destination_set_html_mail_pref (EDestination *, gboolean);

gboolean       e_destination_contains_card      (const EDestination *);
gboolean       e_destination_from_card          (const EDestination *);

gboolean       e_destination_is_auto_recipient  (const EDestination *);
void           e_destination_set_auto_recipient (EDestination *, gboolean value);

void           e_destination_use_card           (EDestination *, EDestinationCardCallback cb, gpointer closure);

ECard         *e_destination_get_card           (const EDestination *);
const gchar   *e_destination_get_book_uri       (const EDestination *);
const gchar   *e_destination_get_card_uid       (const EDestination *);
gint           e_destination_get_email_num      (const EDestination *);

const gchar   *e_destination_get_name           (const EDestination *);  /* "Jane Smith" */
const gchar   *e_destination_get_email          (const EDestination *);  /* "jane@assbarn.com" */
const gchar   *e_destination_get_address        (const EDestination *);;  /* "Jane Smith <jane@assbarn.com>" (or a comma-sep set of such for a list) */

void           e_destination_set_raw            (EDestination *, const gchar *free_form_string);
const gchar   *e_destination_get_textrep        (const EDestination *, gboolean include_email);  /* "Jane Smith" or "jane@assbarn.com" */

gboolean       e_destination_is_evolution_list   (const EDestination *);
gboolean       e_destination_list_show_addresses (const EDestination *);

/* If true, they want HTML mail. */
gboolean       e_destination_get_html_mail_pref (const EDestination *);

gboolean       e_destination_allow_cardification     (const EDestination *);
void           e_destination_set_allow_cardification (EDestination *, gboolean);
void           e_destination_cardify                 (EDestination *, EBook *);
void           e_destination_cardify_delayed         (EDestination *, EBook *, gint delay); /* delay < 0: "default" */
void           e_destination_cancel_cardify          (EDestination *);
gboolean       e_destination_uncardify               (EDestination *);

gboolean       e_destination_revert                  (EDestination *);

gchar         *e_destination_get_address_textv  (EDestination **);

xmlNodePtr     e_destination_xml_encode         (const EDestination *dest);
gboolean       e_destination_xml_decode         (EDestination *dest, xmlNodePtr node);

gchar         *e_destination_export             (const EDestination *);
EDestination  *e_destination_import             (const gchar *str);

gchar         *e_destination_exportv            (EDestination **);
EDestination **e_destination_importv            (const gchar *str);

EDestination **e_destination_list_to_vector_sized (GList *, int n);
EDestination **e_destination_list_to_vector       (GList *);

void           e_destination_freev              (EDestination **);

void           e_destination_touch              (EDestination *);
void           e_destination_touchv             (EDestination **);


#endif /* __E_DESTINATION_H__ */

