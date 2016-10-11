/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_COMPOSER_NAME_HEADER_H
#define E_COMPOSER_NAME_HEADER_H

#include <libebook/libebook.h>

#include <composer/e-composer-header.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_NAME_HEADER \
	(e_composer_name_header_get_type ())
#define E_COMPOSER_NAME_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_NAME_HEADER, EComposerNameHeader))
#define E_COMPOSER_NAME_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_NAME_HEADER, EComposerNameHeaderClass))
#define E_IS_COMPOSER_NAME_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_NAME_HEADER))
#define E_IS_COMPOSER_NAME_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_NAME_HEADER))
#define E_COMPOSER_NAME_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_NAME_HEADER, EComposerNameHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerNameHeader EComposerNameHeader;
typedef struct _EComposerNameHeaderClass EComposerNameHeaderClass;
typedef struct _EComposerNameHeaderPrivate EComposerNameHeaderPrivate;

struct _EComposerNameHeader {
	EComposerHeader parent;
	EComposerNameHeaderPrivate *priv;
};

struct _EComposerNameHeaderClass {
	EComposerHeaderClass parent_class;
};

GType		e_composer_name_header_get_type	(void);
EComposerHeader *
		e_composer_name_header_new	(ESourceRegistry *registry,
						 const gchar *label,
						 ENameSelector *name_selector);
ENameSelector *	e_composer_name_header_get_name_selector
						(EComposerNameHeader *header);
EDestination **	e_composer_name_header_get_destinations
						(EComposerNameHeader *header);
void		e_composer_name_header_add_destinations
						(EComposerNameHeader *header,
						 EDestination **destination);
void		e_composer_name_header_set_destinations
						(EComposerNameHeader *header,
						 EDestination **destinations);

G_END_DECLS

#endif /* E_COMPOSER_NAME_HEADER_H */
