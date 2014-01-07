/*
 * e-composer-activity.h
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

#ifndef E_COMPOSER_ACTIVITY_H
#define E_COMPOSER_ACTIVITY_H

#include <composer/e-msg-composer.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_ACTIVITY \
	(e_composer_activity_get_type ())
#define E_COMPOSER_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_ACTIVITY, EComposerActivity))
#define E_COMPOSER_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_ACTIVITY, EComposerActivityClass))
#define E_IS_COMPOSER_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_ACTIVITY))
#define E_IS_COMPOSER_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_ACTIVITY))
#define E_COMPOSER_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_ACTIVITY, EComposerActivityClass))

G_BEGIN_DECLS

typedef struct _EComposerActivity EComposerActivity;
typedef struct _EComposerActivityClass EComposerActivityClass;
typedef struct _EComposerActivityPrivate EComposerActivityPrivate;

struct _EComposerActivity {
	EActivity parent;
	EComposerActivityPrivate *priv;
};

struct _EComposerActivityClass {
	EActivityClass parent_class;
};

GType		e_composer_activity_get_type	(void);
EActivity *	e_composer_activity_new		(EMsgComposer *composer);
EMsgComposer *	e_composer_activity_get_composer
						(EComposerActivity *activity);

G_END_DECLS

#endif /* E_COMPOSER_ACTIVITY_H */
