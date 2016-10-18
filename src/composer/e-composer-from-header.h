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

#ifndef E_COMPOSER_FROM_HEADER_H
#define E_COMPOSER_FROM_HEADER_H

#include <composer/e-composer-header.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_FROM_HEADER \
	(e_composer_from_header_get_type ())
#define E_COMPOSER_FROM_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeader))
#define E_COMPOSER_FROM_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeaderClass))
#define E_IS_COMPOSER_FROM_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_FROM_HEADER))
#define E_IS_COMPOSER_FROM_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_COMPOSER_FROM_HEADER))
#define E_COMPOSER_FROM_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerFromHeader EComposerFromHeader;
typedef struct _EComposerFromHeaderClass EComposerFromHeaderClass;

struct _EComposerFromHeader {
	EComposerHeader parent;

	GtkWidget *override_widget;
	gboolean override_visible;
};

struct _EComposerFromHeaderClass {
	EComposerHeaderClass parent_class;
};

GType		e_composer_from_header_get_type	(void);
EComposerHeader *
		e_composer_from_header_new	(ESourceRegistry *registry,
						 const gchar *label);
gchar *		e_composer_from_header_dup_active_id
						(EComposerFromHeader *header,
						 gchar **alias_name,
						 gchar **alias_address);
void		e_composer_from_header_set_active_id
						(EComposerFromHeader *header,
						 const gchar *active_id,
						 const gchar *alias_name,
						 const gchar *alias_address);
GtkEntry *	e_composer_from_header_get_name_entry
						(EComposerFromHeader *header);
const gchar *	e_composer_from_header_get_name (EComposerFromHeader *header);
void		e_composer_from_header_set_name (EComposerFromHeader *header,
						 const gchar *name);
GtkEntry *	e_composer_from_header_get_address_entry
						(EComposerFromHeader *header);
const gchar *	e_composer_from_header_get_address
						(EComposerFromHeader *header);
void		e_composer_from_header_set_address
						(EComposerFromHeader *header,
						 const gchar *address);
gboolean	e_composer_from_header_get_override_visible
						(EComposerFromHeader *header);
void		e_composer_from_header_set_override_visible
						(EComposerFromHeader *header,
						 gboolean visible);

G_END_DECLS

#endif /* E_COMPOSER_FROM_HEADER_H */
