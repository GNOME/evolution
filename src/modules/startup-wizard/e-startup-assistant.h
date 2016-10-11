/*
 * e-startup-assistant.h
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

#ifndef E_STARTUP_ASSISTANT_H
#define E_STARTUP_ASSISTANT_H

#include <mail/e-mail-config-assistant.h>

/* Standard GObject macros */
#define E_TYPE_STARTUP_ASSISTANT \
	(e_startup_assistant_get_type ())
#define E_STARTUP_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_STARTUP_ASSISTANT, EStartupAssistant))
#define E_STARTUP_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_STARTUP_ASSISTANT, EStartupAssistantClass))
#define E_IS_STARTUP_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_STARTUP_ASSISTANT))
#define E_IS_STARTUP_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_STARTUP_ASSISTANT))
#define E_STARTUP_ASSISTANT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_STARTUP_ASSISTANT, EStartupAssistantClass))

G_BEGIN_DECLS

typedef struct _EStartupAssistant EStartupAssistant;
typedef struct _EStartupAssistantClass EStartupAssistantClass;
typedef struct _EStartupAssistantPrivate EStartupAssistantPrivate;

struct _EStartupAssistant {
	EMailConfigAssistant parent;
	EStartupAssistantPrivate *priv;
};

struct _EStartupAssistantClass {
	EMailConfigAssistantClass parent_class;
};

GType		e_startup_assistant_get_type	(void) G_GNUC_CONST;
void		e_startup_assistant_type_register
						(GTypeModule *type_module);
GtkWidget *	e_startup_assistant_new		(EMailSession *session);

#endif /* E_STARTUP_ASSISTANT_H */

