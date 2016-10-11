/*
 * e-settings-mail-session.h
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

#ifndef E_SETTINGS_MAIL_SESSION_H
#define E_SETTINGS_MAIL_SESSION_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_MAIL_SESSION \
	(e_settings_mail_session_get_type ())
#define E_SETTINGS_MAIL_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_MAIL_SESSION, ESettingsMailSession))
#define E_SETTINGS_MAIL_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_MAIL_SESSION, ESettingsMailSessionClass))
#define E_IS_SETTINGS_MAIL_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_MAIL_SESSION))
#define E_IS_SETTINGS_MAIL_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_MAIL_SESSION))
#define E_SETTINGS_MAIL_SESSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_MAIL_SESSION, ESettingsMailSessionClass))

G_BEGIN_DECLS

typedef struct _ESettingsMailSession ESettingsMailSession;
typedef struct _ESettingsMailSessionClass ESettingsMailSessionClass;
typedef struct _ESettingsMailSessionPrivate ESettingsMailSessionPrivate;

struct _ESettingsMailSession {
	EExtension parent;
	ESettingsMailSessionPrivate *priv;
};

struct _ESettingsMailSessionClass {
	EExtensionClass parent_class;
};

GType		e_settings_mail_session_get_type
						(void) G_GNUC_CONST;
void		e_settings_mail_session_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_MAIL_SESSION_H */

