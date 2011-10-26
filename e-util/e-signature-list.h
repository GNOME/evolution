/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SIGNATURE_LIST_H
#define E_SIGNATURE_LIST_H

#include <libedataserver/e-list.h>
#include <e-util/e-signature.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_LIST \
	(e_signature_list_get_type ())
#define E_SIGNATURE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_LIST, ESignatureList))
#define E_SIGNATURE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_LIST, ESignatureListClass))
#define E_IS_SIGNATURE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_LIST))
#define E_IS_SIGNATURE_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_LIST))
#define E_SIGNATURE_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_LIST, ESignatureListClass))

G_BEGIN_DECLS

typedef struct _ESignatureList ESignatureList;
typedef struct _ESignatureListClass ESignatureListClass;
typedef struct _ESignatureListPrivate ESignatureListPrivate;

struct _ESignatureList {
	EList parent;
	ESignatureListPrivate *priv;
};

struct _ESignatureListClass {
	EListClass parent_class;

	/* Signals */
	void		(*signature_added)	(ESignatureList *signature_list,
						 ESignature *signature);
	void		(*signature_changed)	(ESignatureList *signature_list,
						 ESignature *signature);
	void		(*signature_removed)	(ESignatureList *signature_list,
						 ESignature *signature);
};

GType		e_signature_list_get_type	(void);
ESignatureList *e_signature_list_new		(void);
void		e_signature_list_construct	(ESignatureList *signature_list,
						 GConfClient *client);
void		e_signature_list_save		(ESignatureList *signature_list);
void		e_signature_list_add		(ESignatureList *signature_list,
						 ESignature *signature);
void		e_signature_list_change		(ESignatureList *signature_list,
						 ESignature *signature);
void		e_signature_list_remove		(ESignatureList *signature_list,
						 ESignature *signature);
ESignature *	e_signature_list_find_by_name	(ESignatureList *signature_list,
						 const gchar *signature_name);
ESignature *	e_signature_list_find_by_uid	(ESignatureList *signature_list,
						 const gchar *signature_uid);

G_END_DECLS

#endif /* E_SIGNATURE_LIST_H */
