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

#define EVC_FN "FN"
#define EVC_ORG "ORG"
#define EVC_URL "URL"
#define EVC_VERSION "VERSION"
#define EVC_REV "REV"
#define EVC_PRODID "PRODID"
#define EVC_TYPE "TYPE"
#define EVC_ADR "ADR"
#define EVC_TEL "TEL"

#define EVC_ENCODING "ENCODING"
#define EVC_QUOTEDPRINTABLE "QUOTED-PRINTABLE"

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
};

GType   e_vcard_get_type                     (void);
EVCard* e_vcard_new                          (void);
EVCard* e_vcard_new_from_string              (const char *str);
char*   e_vcard_to_string                    (EVCard *evcard);
/* mostly for debugging */
void    e_vcard_dump_structure               (EVCard *evc);


/* attributes */
EVCardAttribute *e_vcard_attribute_new             (const char *attr_group, const char *attr_name);
void             e_vcard_attribute_free            (EVCardAttribute *attr);
void             e_vcard_add_attribute             (EVCard *evcard, EVCardAttribute *attr);
void             e_vcard_add_attribute_with_value  (EVCard *evcard, EVCardAttribute *attr, const char *value);
void             e_vcard_add_attribute_with_values (EVCard *evcard, EVCardAttribute *attr, ...);
void             e_vcard_attribute_add_value       (EVCardAttribute *attr, const char *value);
void             e_vcard_attribute_add_values      (EVCardAttribute *attr, ...);

/* attribute parameters */
EVCardAttributeParam* e_vcard_attribute_param_new             (const char *param_name);
void                  e_vcard_attribute_param_free            (EVCardAttributeParam *param);
void                  e_vcard_attribute_add_param             (EVCardAttribute *attr, EVCardAttributeParam *param);
void                  e_vcard_attribute_add_param_with_value  (EVCardAttribute *attr,
							       EVCardAttributeParam *param, const char *value);
void                  e_vcard_attribute_add_param_with_values (EVCardAttribute *attr,
							       EVCardAttributeParam *param, ...);

void                  e_vcard_attribute_param_add_value       (EVCardAttributeParam *param,
							       const char *value);
void                  e_vcard_attribute_param_add_values      (EVCardAttributeParam *param,
							       ...);

/* EVCard* accessors.  nothing returned from these functions should be
   freed by the caller. */
GList*           e_vcard_get_attributes       (EVCard *evcard);
const char*      e_vcard_attribute_get_group  (EVCardAttribute *attr);
const char*      e_vcard_attribute_get_name   (EVCardAttribute *attr);
GList*           e_vcard_attribute_get_values (EVCardAttribute *attr);

GList*           e_vcard_attribute_get_params       (EVCardAttribute *attr);
const char*      e_vcard_attribute_param_get_name   (EVCardAttributeParam *param);
GList*           e_vcard_attribute_param_get_values (EVCardAttributeParam *param);


#endif /* _EVCARD_H */
