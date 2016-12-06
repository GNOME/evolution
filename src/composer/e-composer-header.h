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

#ifndef E_COMPOSER_HEADER_H
#define E_COMPOSER_HEADER_H

#include <composer/e-composer-common.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_HEADER \
	(e_composer_header_get_type ())
#define E_COMPOSER_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_HEADER, EComposerHeader))
#define E_COMPOSER_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_HEADER, EComposerHeaderClass))
#define E_IS_COMPOSER_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_HEADER))
#define E_IS_COMPOSER_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_HEADER))
#define E_COMPOSER_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_HEADER, EComposerHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerHeader EComposerHeader;
typedef struct _EComposerHeaderClass EComposerHeaderClass;
typedef struct _EComposerHeaderPrivate EComposerHeaderPrivate;

struct _EComposerHeader {
	GObject parent;
	GtkWidget *title_widget;
	GtkWidget *input_widget;
	EComposerHeaderPrivate *priv;
};

struct _EComposerHeaderClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)		(EComposerHeader *header);
	void		(*clicked)		(EComposerHeader *header);
};

GType		e_composer_header_get_type	(void);
const gchar *	e_composer_header_get_label	(EComposerHeader *header);
ESourceRegistry *
		e_composer_header_get_registry	(EComposerHeader *header);
gboolean	e_composer_header_get_sensitive	(EComposerHeader *header);
void		e_composer_header_set_sensitive	(EComposerHeader *header,
						 gboolean sensitive);
gboolean	e_composer_header_get_visible	(EComposerHeader *header);
void		e_composer_header_set_visible	(EComposerHeader *header,
						 gboolean visible);
void		e_composer_header_set_title_tooltip
						(EComposerHeader *header,
						 const gchar *tooltip);
void		e_composer_header_set_input_tooltip
						(EComposerHeader *header,
						 const gchar *tooltip);
gboolean	e_composer_header_get_title_has_tooltip
						(EComposerHeader *header);
void		e_composer_header_set_title_has_tooltip
						(EComposerHeader *header,
						 gboolean has_tooltip);
gboolean	e_composer_header_get_input_has_tooltip
						(EComposerHeader *header);
void		e_composer_header_set_input_has_tooltip
						(EComposerHeader *header,
						 gboolean has_tooltip);

G_END_DECLS

#endif /* E_COMPOSER_HEADER_H */
