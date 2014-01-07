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

#ifndef E_COMPOSER_TEXT_HEADER_H
#define E_COMPOSER_TEXT_HEADER_H

#include <composer/e-composer-header.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_TEXT_HEADER \
	(e_composer_text_header_get_type ())
#define E_COMPOSER_TEXT_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_TEXT_HEADER, EComposerTextHeader))
#define E_COMPOSER_TEXT_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_TEXT_HEADER, EComposerTextHeaderClass))
#define E_IS_COMPOSER_TEXT_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_TEXT_HEADER))
#define E_IS_COMPOSER_TEXT_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_TEXT_HEADER))
#define E_COMPOSER_TEXT_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_TEXT_HEADER, EComposerTextHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerTextHeader EComposerTextHeader;
typedef struct _EComposerTextHeaderClass EComposerTextHeaderClass;

struct _EComposerTextHeader {
	EComposerHeader parent;
};

struct _EComposerTextHeaderClass {
	EComposerHeaderClass parent_class;

	GType entry_type;
};

GType		e_composer_text_header_get_type	(void);
EComposerHeader *
		e_composer_text_header_new_label
						(ESourceRegistry *registry,
						 const gchar *label);
EComposerHeader *
		e_composer_text_header_new_button
						(ESourceRegistry *registry,
						 const gchar *label);
const gchar *	e_composer_text_header_get_text	(EComposerTextHeader *header);
void		e_composer_text_header_set_text (EComposerTextHeader *header,
						 const gchar *text);

G_END_DECLS

#endif /* E_COMPOSER_TEXT_HEADER_H */
