/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-vcard.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Author: Chris Toshok (toshok@ximian.com)
 */

#ifndef _EVCARD_H
#define _EVCARD_H

#include <glib.h>
#include <glib-object.h>

#define EVC_ADR             "ADR"
#define EVC_BDAY            "BDAY"
#define EVC_CALURI          "CALURI"
#define EVC_CATEGORIES      "CATEGORIES"
#define EVC_EMAIL           "EMAIL"
#define EVC_ENCODING        "ENCODING"
#define EVC_FBURL           "FBURL"
#define EVC_FN              "FN"
#define EVC_ICSCALENDAR     "ICSCALENDAR" /* XXX should this be X-EVOLUTION-ICSCALENDAR? */
#define EVC_LABEL           "LABEL"
#define EVC_LOGO            "LOGO"
#define EVC_MAILER          "MAILER"
#define EVC_NICKNAME        "NICKNAME"
#define EVC_N               "N"
#define EVC_NOTE            "NOTE"
#define EVC_ORG             "ORG"
#define EVC_PHOTO           "PHOTO"
#define EVC_PRODID          "PRODID"
#define EVC_QUOTEDPRINTABLE "QUOTED-PRINTABLE"
#define EVC_REV             "REV"
#define EVC_ROLE            "ROLE"
#define EVC_TEL             "TEL"
#define EVC_TITLE           "TITLE"
#define EVC_TYPE            "TYPE"
#define EVC_UID             "UID"
#define EVC_URL             "URL"
#define EVC_VALUE           "VALUE"
#define EVC_VERSION         "VERSION"

#define EVC_X_AIM           "X-AIM"
#define EVC_X_ANNIVERSARY   "X-EVOLUTION-ANNIVERSARY"
#define EVC_X_ASSISTANT     "X-EVOLUTION-ASSISTANT"
#define EVC_X_BIRTHDAY      "X-EVOLUTION-BIRTHDAY"
#define EVC_X_BLOG_URL      "X-EVOLUTION-BLOG-URL"
#define EVC_X_FILE_AS       "X-EVOLUTION-FILE-AS"
#define EVC_X_ICQ           "X-ICQ"
#define EVC_X_JABBER        "X-JABBER"
#define EVC_X_LIST_SHOW_ADDRESSES "X-EVOLUTION-LIST-SHOW_ADDRESSES"
#define EVC_X_LIST          "X-EVOLUTION-LIST"
#define EVC_X_MANAGER       "X-EVOLUTION-MANAGER"
#define EVC_X_MSN           "X-MSN"
#define EVC_X_SPOUSE        "X-EVOLUTION-SPOUSE"
#define EVC_X_WANTS_HTML          "X-MOZILLA-HTML"
#define EVC_X_WANTS_HTML          "X-MOZILLA-HTML"
#define EVC_X_YAHOO         "X-YAHOO"

typedef enum {
	EVC_FORMAT_VCARD_21,
	EVC_FORMAT_VCARD_30
} EVCardFormat;

#define E_TYPE_VCARD            (e_vcard_get_type ())
#define E_VCARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_VCARD, EVCard))
#define E_VCARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_VCARD, EVCardClass))
#define E_IS_VCARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_VCARD))
#define E_IS_VCARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_VCARD))
#define E_VCARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_VCARD, EVCardClass))

typedef struct _EVCard EVCard;
typedef struct _EVCardClass EVCardClass;
typedef struct _EVCardPrivate EVCardPrivate;
typedef struct _EVCardAttribute EVCardAttribute;
typedef struct _EVCardAttributeParam EVCardAttributeParam;

struct _EVCard {
	GObject parent;

	EVCardPrivate *priv;
};

struct _EVCardClass {
	GObjectClass parent_class;

	/* Padding for future expansion */
	void (*_ebook_reserved0) (void);
	void (*_ebook_reserved1) (void);
	void (*_ebook_reserved2) (void);
	void (*_ebook_reserved3) (void);
	void (*_ebook_reserved4) (void);
};

GType   e_vcard_get_type                     (void);

void    e_vcard_construct                    (EVCard *evc, const char *str);
EVCard* e_vcard_new                          (void);
EVCard* e_vcard_new_from_string              (const char *str);

char*   e_vcard_to_string                    (EVCard *evc, EVCardFormat format);

/* mostly for debugging */
void    e_vcard_dump_structure               (EVCard *evc);


/* attributes */
EVCardAttribute *e_vcard_attribute_new               (const char *attr_group, const char *attr_name);
void             e_vcard_attribute_free              (EVCardAttribute *attr);
EVCardAttribute *e_vcard_attribute_copy              (EVCardAttribute *attr);
void             e_vcard_remove_attributes           (EVCard *evcard, const char *attr_group, const char *attr_name);
void             e_vcard_remove_attribute            (EVCard *evcard, EVCardAttribute *attr);
void             e_vcard_add_attribute               (EVCard *evcard, EVCardAttribute *attr);
void             e_vcard_add_attribute_with_value    (EVCard *evcard, EVCardAttribute *attr, const char *value);
void             e_vcard_add_attribute_with_values   (EVCard *evcard, EVCardAttribute *attr, ...);
void             e_vcard_attribute_add_value         (EVCardAttribute *attr, const char *value);
void             e_vcard_attribute_add_value_decoded (EVCardAttribute *attr, const char *value, int len);
void             e_vcard_attribute_add_values        (EVCardAttribute *attr, ...);
void             e_vcard_attribute_remove_values     (EVCardAttribute *attr);
void             e_vcard_attribute_remove_params     (EVCardAttribute *attr);

/* attribute parameters */
EVCardAttributeParam* e_vcard_attribute_param_new             (const char *param_name);
void                  e_vcard_attribute_param_free            (EVCardAttributeParam *param);
EVCardAttributeParam* e_vcard_attribute_param_copy            (EVCardAttributeParam *param);
void                  e_vcard_attribute_add_param             (EVCardAttribute *attr, EVCardAttributeParam *param);
void                  e_vcard_attribute_add_param_with_value  (EVCardAttribute *attr,
							       EVCardAttributeParam *param, const char *value);
void                  e_vcard_attribute_add_param_with_values (EVCardAttribute *attr,
							       EVCardAttributeParam *param, ...);

void                  e_vcard_attribute_param_add_value       (EVCardAttributeParam *param,
							       const char *value);
void                  e_vcard_attribute_param_add_values      (EVCardAttributeParam *param,
							       ...);
void                  e_vcard_attribute_param_remove_values   (EVCardAttributeParam *param);

/* EVCard* accessors.  nothing returned from these functions should be
   freed by the caller. */
GList*           e_vcard_get_attributes       (EVCard *evcard);
const char*      e_vcard_attribute_get_group  (EVCardAttribute *attr);
const char*      e_vcard_attribute_get_name   (EVCardAttribute *attr);
GList*           e_vcard_attribute_get_values (EVCardAttribute *attr);  /* GList elements are of type char* */
GList*           e_vcard_attribute_get_values_decoded (EVCardAttribute *attr); /* GList elements are of type GString* */

GList*           e_vcard_attribute_get_params       (EVCardAttribute *attr);
const char*      e_vcard_attribute_param_get_name   (EVCardAttributeParam *param);
GList*           e_vcard_attribute_param_get_values (EVCardAttributeParam *param);

#endif /* _EVCARD_H */
