/*
 * e-book-source-config.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_BOOK_SOURCE_CONFIG_H
#define E_BOOK_SOURCE_CONFIG_H

#include <e-util/e-source-config.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_SOURCE_CONFIG \
	(e_book_source_config_get_type ())
#define E_BOOK_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SOURCE_CONFIG, EBookSourceConfig))
#define E_BOOK_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SOURCE_CONFIG, EBookSourceConfigClass))
#define E_IS_BOOK_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SOURCE_CONFIG))
#define E_IS_BOOK_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SOURCE_CONFIG))
#define E_BOOK_SOURCE_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SOURCE_CONFIG, EBookSourceConfigClass))

G_BEGIN_DECLS

typedef struct _EBookSourceConfig EBookSourceConfig;
typedef struct _EBookSourceConfigClass EBookSourceConfigClass;
typedef struct _EBookSourceConfigPrivate EBookSourceConfigPrivate;

struct _EBookSourceConfig {
	ESourceConfig parent;
	EBookSourceConfigPrivate *priv;
};

struct _EBookSourceConfigClass {
	ESourceConfigClass parent_class;
};

GType		e_book_source_config_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_book_source_config_new	(ESourceRegistry *registry,
						 ESource *original_source);
void		e_book_source_config_add_offline_toggle
						(EBookSourceConfig *config,
						 ESource *scratch_source);

G_END_DECLS

#endif /* E_BOOK_SOURCE_CONFIG_H */
