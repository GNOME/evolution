/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_COMPOSER_POST_HEADER_H
#define E_COMPOSER_POST_HEADER_H

#include <composer/e-composer-text-header.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_POST_HEADER \
	(e_composer_post_header_get_type ())
#define E_COMPOSER_POST_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeader))
#define E_COMPOSER_POST_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeaderClass))
#define E_IS_COMPOSER_POST_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_POST_HEADER))
#define E_IS_COMPOSER_POST_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_POST_HEADER))
#define E_COMPOSER_POST_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeaderClass))

G_BEGIN_DECLS

/* Avoid including files from the Mail module. */

typedef struct _EComposerPostHeader EComposerPostHeader;
typedef struct _EComposerPostHeaderClass EComposerPostHeaderClass;
typedef struct _EComposerPostHeaderPrivate EComposerPostHeaderPrivate;

struct _EComposerPostHeader {
	EComposerTextHeader parent;
	EComposerPostHeaderPrivate *priv;
};

struct _EComposerPostHeaderClass {
	EComposerTextHeaderClass parent_class;
};

GType		e_composer_post_header_get_type	(void);
EComposerHeader *
		e_composer_post_header_new	(ESourceRegistry *registry,
						 const gchar *label);
GList *		e_composer_post_header_get_folders
						(EComposerPostHeader *header);
void		e_composer_post_header_set_folders
						(EComposerPostHeader *header,
						 GList *folders);
void		e_composer_post_header_set_folders_base
						(EComposerPostHeader *header,
						 const gchar *base_url,
						 const gchar *folders);
ESource *	e_composer_post_header_get_mail_account
						(EComposerPostHeader *header);
void		e_composer_post_header_set_mail_account
						(EComposerPostHeader *header,
						 ESource *mail_account);

G_END_DECLS

#endif /* E_COMPOSER_POST_HEADER_H */
