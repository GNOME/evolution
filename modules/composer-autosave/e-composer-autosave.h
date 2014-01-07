/*
 * e-composer-autosave.h
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

#ifndef E_COMPOSER_AUTOSAVE_H
#define E_COMPOSER_AUTOSAVE_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_AUTOSAVE \
	(e_composer_autosave_get_type ())
#define E_COMPOSER_AUTOSAVE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_AUTOSAVE, EComposerAutosave))
#define E_COMPOSER_AUTOSAVE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_AUTOSAVE, EComposerAutosaveClass))
#define E_IS_COMPOSER_AUTOSAVE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_AUTOSAVE))
#define E_IS_COMPOSER_AUTOSAVE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_AUTOSAVE))
#define E_COMPOSER_AUTOSAVE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_AUTOSAVE, EComposerAutosaveClass))

G_BEGIN_DECLS

typedef struct _EComposerAutosave EComposerAutosave;
typedef struct _EComposerAutosaveClass EComposerAutosaveClass;
typedef struct _EComposerAutosavePrivate EComposerAutosavePrivate;

struct _EComposerAutosave {
	EExtension parent;
	EComposerAutosavePrivate *priv;
};

struct _EComposerAutosaveClass {
	EExtensionClass parent_class;
};

GType		e_composer_autosave_get_type	(void) G_GNUC_CONST;
void		e_composer_autosave_type_register
						(GTypeModule *type_module);

#endif /* E_COMPOSER_AUTOSAVE_H */

