/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-destination.h
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
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

#ifndef __E_DESTINATION_H__
#define __E_DESTINATION_H__

#include <glib.h>
#include <glib-object.h>
#include <ebook/e-contact.h>
#include <ebook/e-book.h>
#include <libxml/tree.h>

#define EAB_TYPE_DESTINATION           (eab_destination_get_type ())
#define EAB_DESTINATION(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), EAB_TYPE_DESTINATION, EABDestination))
#define EAB_DESTINATION_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST ((k), EAB_TYPE_DESTINATION, EABDestinationClass))
#define EAB_IS_DESTINATION(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAB_TYPE_DESTINATION))
#define EAB_IS_DESTINATION_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), EAB_TYPE_DESTINATION))
#define EAB_DESTINATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EAB_TYPE_DESTINATION, EABDestinationClass))

typedef struct _EABDestination EABDestination;
typedef struct _EABDestinationClass EABDestinationClass;

typedef void (*EABDestinationContactCallback) (EABDestination *dest, EContact *contact, gpointer closure);

struct _EABDestinationPrivate;

struct _EABDestination {
	GObject object;

	struct _EABDestinationPrivate *priv;
};

struct _EABDestinationClass {
	GObjectClass parent_class;

	void (*changed) (EABDestination *dest);	
	void (*contact_loaded) (EABDestination *dest);
};

GType eab_destination_get_type (void);


EABDestination  *eab_destination_new                (void);
void             eab_destination_changed            (EABDestination *);
EABDestination  *eab_destination_copy               (const EABDestination *);
void             eab_destination_clear              (EABDestination *);

gboolean         eab_destination_is_empty           (const EABDestination *);
gboolean         eab_destination_equal              (const EABDestination *a, const EABDestination *b);

void             eab_destination_set_contact        (EABDestination *, EContact *contact, gint email_num);

void             eab_destination_set_name           (EABDestination *, const gchar *name);
void             eab_destination_set_email          (EABDestination *, const gchar *email);

void             eab_destination_set_html_mail_pref (EABDestination *, gboolean);

gboolean         eab_destination_contains_contact   (const EABDestination *);

gboolean         eab_destination_is_auto_recipient  (const EABDestination *);
void             eab_destination_set_auto_recipient (EABDestination *, gboolean value);

void             eab_destination_use_contact        (EABDestination *, EABDestinationContactCallback cb, gpointer closure);

EContact        *eab_destination_get_contact        (const EABDestination *);
gint             eab_destination_get_email_num      (const EABDestination *);

const gchar     *eab_destination_get_name           (const EABDestination *);  /* "Jane Smith" */
const gchar     *eab_destination_get_email          (const EABDestination *);  /* "jane@assbarn.com" */
const gchar     *eab_destination_get_address        (const EABDestination *);;  /* "Jane Smith <jane@assbarn.com>" (or a comma-sep set of such for a list) */

void             eab_destination_set_raw            (EABDestination *, const gchar *free_form_string);
const gchar     *eab_destination_get_textrep        (const EABDestination *, gboolean include_email);  /* "Jane Smith" or "jane@assbarn.com" */

gboolean         eab_destination_is_evolution_list   (const EABDestination *);
gboolean         eab_destination_list_show_addresses (const EABDestination *);

/* If true, they want HTML mail. */
gboolean         eab_destination_get_html_mail_pref (const EABDestination *);

void             eab_destination_load_contact         (EABDestination *, EBook *);
void             eab_destination_load_contact_delayed (EABDestination *, EBook *, gint delay); /* delay < 0: "default" */
void             eab_destination_cancel_contact_load  (EABDestination *);
gboolean         eab_destination_unload_contact       (EABDestination *);

gchar           *eab_destination_get_address_textv  (EABDestination **);

gchar           *eab_destination_export             (const EABDestination *);
EABDestination  *eab_destination_import             (const gchar *str);

gchar           *eab_destination_exportv            (EABDestination **);
EABDestination **eab_destination_importv            (const gchar *str);

EABDestination **eab_destination_list_to_vector_sized (GList *, int n);
EABDestination **eab_destination_list_to_vector       (GList *);

void             eab_destination_freev              (EABDestination **);

void             eab_destination_touch              (EABDestination *);
void             eab_destination_touchv             (EABDestination **);


#endif /* __EAB_DESTINATION_H__ */

