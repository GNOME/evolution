/*
 * e-composer-registry.h
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

#ifndef E_COMPOSER_REGISTRY_H
#define E_COMPOSER_REGISTRY_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_REGISTRY \
	(e_composer_registry_get_type ())
#define E_COMPOSER_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_REGISTRY, EComposerRegistry))
#define E_COMPOSER_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_REGISTRY, EComposerRegistryClass))
#define E_IS_COMPOSER_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_REGISTRY))
#define E_IS_COMPOSER_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_REGISTRY))
#define E_COMPOSER_REGISTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_REGISTRY, EComposerRegistryClass))

G_BEGIN_DECLS

typedef struct _EComposerRegistry EComposerRegistry;
typedef struct _EComposerRegistryClass EComposerRegistryClass;
typedef struct _EComposerRegistryPrivate EComposerRegistryPrivate;

struct _EComposerRegistry {
	EExtension parent;
	EComposerRegistryPrivate *priv;
};

struct _EComposerRegistryClass {
	EExtensionClass parent_class;
};

GType		e_composer_registry_get_type	(void) G_GNUC_CONST;
void		e_composer_registry_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_COMPOSER_REGISTRY_H */

