/*
 * em-subscription-editor.h
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

#ifndef EM_SUBSCRIPTION_EDITOR_H
#define EM_SUBSCRIPTION_EDITOR_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_SUBSCRIPTION_EDITOR \
	(em_subscription_editor_get_type ())
#define EM_SUBSCRIPTION_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_SUBSCRIPTION_EDITOR, EMSubscriptionEditor))
#define EM_SUBSCRIPTION_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_SUBSCRIPTION_EDITOR, EMSubscriptionEditorClass))
#define EM_IS_SUBSCRIPTION_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_SUBSCRIPTION_EDITOR))
#define EM_IS_SUBSCRIPTION_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_SUBSCRIPTION_EDITOR))
#define EM_IS_SUBSCRIPTION_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_SUBSCRIPTION_EDITOR, EMSubscriptionEditorClass))

G_BEGIN_DECLS

typedef struct _EMSubscriptionEditor EMSubscriptionEditor;
typedef struct _EMSubscriptionEditorClass EMSubscriptionEditorClass;
typedef struct _EMSubscriptionEditorPrivate EMSubscriptionEditorPrivate;

struct _EMSubscriptionEditor {
	GtkDialog parent;
	EMSubscriptionEditorPrivate *priv;
};

struct _EMSubscriptionEditorClass {
	GtkDialogClass parent_class;
};

GType		em_subscription_editor_get_type	(void);
GtkWidget *	em_subscription_editor_new	(GtkWindow *parent,
						 EMailSession *session,
						 CamelStore *initial_store);
EMailSession *	em_subscription_editor_get_session
						(EMSubscriptionEditor *editor);
CamelStore *	em_subscription_editor_get_store
						(EMSubscriptionEditor *editor);

G_END_DECLS

#endif /* EM_SUBSCRIPTION_EDITOR_H */
