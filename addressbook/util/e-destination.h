/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-destination.h
 *
 * Copyright (C) 2001-2004 Ximian, Inc.
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
#include <libebook/e-contact.h>
#include <libebook/e-book.h>
#include <libxml/tree.h>

#define E_TYPE_DESTINATION           (e_destination_get_type ())
#define E_DESTINATION(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_DESTINATION, EDestination))
#define E_DESTINATION_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_DESTINATION, EDestinationClass))
#define E_IS_DESTINATION(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_DESTINATION))
#define E_IS_DESTINATION_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_DESTINATION))
#define E_DESTINATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_DESTINATION, EDestinationClass))

typedef struct _EDestination EDestination;
typedef struct _EDestinationClass EDestinationClass;

struct _EDestinationPrivate;

struct _EDestination {
	GObject object;

	struct _EDestinationPrivate *priv;
};

struct _EDestinationClass {
	GObjectClass parent_class;

	/* Padding for future expansion */
	void (*_ebook_reserved0) (void);
	void (*_ebook_reserved1) (void);
	void (*_ebook_reserved2) (void);
	void (*_ebook_reserved3) (void);
	void (*_ebook_reserved4) (void);
};

GType e_destination_get_type (void);


EDestination  *e_destination_new                (void);
EDestination  *e_destination_copy               (const EDestination *);

gboolean       e_destination_empty              (const EDestination *);
gboolean       e_destination_equal              (const EDestination *a, const EDestination *b);

/* for use with EDestinations that wrap a particular contact */
void           e_destination_set_contact        (EDestination *, EContact *contact, int email_num);
void           e_destination_set_contact_uid    (EDestination *dest, const char *uid, gint email_num);
void           e_destination_set_book           (EDestination *, EBook *book);
EContact      *e_destination_get_contact        (const EDestination *);
const char    *e_destination_get_source_uid     (const EDestination *);
const char    *e_destination_get_contact_uid    (const EDestination *);
int            e_destination_get_email_num      (const EDestination *);

/* for use with EDestinations built up from strings (not corresponding to contacts in a user's address books) */
void           e_destination_set_name           (EDestination *, const char *name);
void           e_destination_set_email          (EDestination *, const char *email);
const char    *e_destination_get_name           (const EDestination *);  /* "Jane Smith" */
const char    *e_destination_get_email          (const EDestination *);  /* "jane@assbarn.com" */
const char    *e_destination_get_address        (const EDestination *);  /* "Jane Smith <jane@assbarn.com>" (or a comma-sep set of such for a list) */

gboolean       e_destination_is_evolution_list   (const EDestination *);
gboolean       e_destination_list_show_addresses (const EDestination *);

/* If true, they want HTML mail. */
void           e_destination_set_html_mail_pref (EDestination *dest, gboolean flag);
gboolean       e_destination_get_html_mail_pref (const EDestination *);

/* used by the evolution composer to manage automatic recipients

   XXX should probably be implemented using a more neutral/extensible
   approach instead of a hardcoded evolution-only flag. */
gboolean       e_destination_is_auto_recipient  (const EDestination *);
void           e_destination_set_auto_recipient (EDestination *, gboolean value);

/* parse out an EDestination (name/email, not contact) from a free form string. */
void           e_destination_set_raw            (EDestination *, const char *free_form_string);

/* generate a plain-text representation of an EDestination* or EDestination** */
const char    *e_destination_get_textrep        (const EDestination *, gboolean include_email);  /* "Jane Smith" or "jane@assbarn.com" */
char          *e_destination_get_textrepv       (EDestination **);

/* XML export/import routines. */
char          *e_destination_export             (const EDestination *);
char          *e_destination_exportv            (EDestination **);
EDestination  *e_destination_import             (const char *str);
EDestination **e_destination_importv            (const char *str);

/* EVCard "export" routines */
void          e_destination_export_to_vcard_attribute   (EDestination *dest, EVCardAttribute *attr);

void           e_destination_freev              (EDestination **);

#endif /* __E_DESTINATION_H__ */

