/*
 * e-source-local.h
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

#ifndef E_SOURCE_LOCAL_H
#define E_SOURCE_LOCAL_H

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_LOCAL \
	(e_source_local_get_type ())
#define E_SOURCE_LOCAL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_LOCAL, ESourceLocal))
#define E_SOURCE_LOCAL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_LOCAL, ESourceLocalClass))
#define E_IS_SOURCE_LOCAL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_LOCAL))
#define E_IS_SOURCE_LOCAL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_LOCAL))
#define E_SOURCE_LOCAL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_LOCAL, ESourceLocalClass))

#define E_SOURCE_EXTENSION_LOCAL_BACKEND "Local Backend"

G_BEGIN_DECLS

typedef struct _ESourceLocal ESourceLocal;
typedef struct _ESourceLocalClass ESourceLocalClass;
typedef struct _ESourceLocalPrivate ESourceLocalPrivate;

struct _ESourceLocal {
	ESourceExtension parent;
	ESourceLocalPrivate *priv;
};

struct _ESourceLocalClass {
	ESourceExtensionClass parent_class;
};

GType		e_source_local_get_type		(void);
void		e_source_local_type_register	(GTypeModule *type_module);
GFile *		e_source_local_get_custom_file	(ESourceLocal *extension);
GFile *		e_source_local_dup_custom_file	(ESourceLocal *extension);
void		e_source_local_set_custom_file	(ESourceLocal *extension,
						 GFile *custom_file);

G_END_DECLS

#endif /* E_SOURCE_LOCAL_H */
