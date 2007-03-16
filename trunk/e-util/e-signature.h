/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __E_SIGNATURE_H__
#define __E_SIGNATURE_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SIGNATURE            (e_signature_get_type ())
#define E_SIGNATURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SIGNATURE, ESignature))
#define E_SIGNATURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SIGNATURE, ESignatureClass))
#define E_IS_SIGNATURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SIGNATURE))
#define E_IS_SIGNATURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SIGNATURE))

typedef struct _ESignature ESignature;
typedef struct _ESignatureClass ESignatureClass;

struct _ESignature {
	GObject parent_object;
	
	gboolean autogen;
	gboolean script;
	gboolean html;
	
	char *filename;
	char *name;
	char *uid;
};

struct _ESignatureClass {
	GObjectClass parent_class;
	
};


GType e_signature_get_type (void);

ESignature *e_signature_new (void);
ESignature *e_signature_new_from_xml (const char *xml);

char *e_signature_uid_from_xml (const char *xml);

gboolean e_signature_set_from_xml (ESignature *sig, const char *xml);

char *e_signature_to_xml (ESignature *sig);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_SIGNATURE_H__ */
