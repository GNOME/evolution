/*
 * e-mail-config-page.c
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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include <e-util/e-marshal.h>
#include <e-util/e-util.h>

#include "e-mail-config-page.h"

enum {
	CHANGED,
	SETUP_DEFAULTS,
	CHECK_COMPLETE,
	COMMIT_CHANGES,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (EMailConfigPage, e_mail_config_page, GTK_TYPE_SCROLLED_WINDOW)

static gboolean
mail_config_page_check_complete (EMailConfigPage *page)
{
	return TRUE;
}

static gboolean
mail_config_page_submit_sync (EMailConfigPage *page,
                              GCancellable *cancellable,
                              GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;
	gboolean success;

	closure = e_async_closure_new ();

	e_mail_config_page_submit (
		page, cancellable, e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	success = e_mail_config_page_submit_finish (page, result, error);

	e_async_closure_free (closure);

	return success;
}

static void
mail_config_page_submit (EMailConfigPage *page,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GTask *task;

	task = g_task_new (page, cancellable, callback, user_data);
	g_task_set_source_tag (task, mail_config_page_submit);

	g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static gboolean
mail_config_page_submit_finish (EMailConfigPage *page,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, page), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, mail_config_page_submit), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
mail_config_page_check_complete_accumulator (GSignalInvocationHint *ihint,
                                             GValue *return_accu,
                                             const GValue *handler_return,
                                             gpointer unused)
{
	gboolean v_boolean;

	/* Abort emission if a handler returns FALSE. */
	v_boolean = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, v_boolean);

	return v_boolean;
}

static void
e_mail_config_page_default_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Untitled");
	iface->page_type = GTK_ASSISTANT_PAGE_CONTENT;

	iface->check_complete = mail_config_page_check_complete;
	iface->submit_sync = mail_config_page_submit_sync;
	iface->submit = mail_config_page_submit;
	iface->submit_finish = mail_config_page_submit_finish;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SETUP_DEFAULTS] = g_signal_new (
		"setup-defaults",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, setup_defaults),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CHECK_COMPLETE] = g_signal_new (
		"check-complete",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, check_complete),
		mail_config_page_check_complete_accumulator, NULL,
		e_marshal_BOOLEAN__VOID,
		G_TYPE_BOOLEAN, 0);

	signals[COMMIT_CHANGES] = g_signal_new (
		"commit-changes",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, commit_changes),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}

void
e_mail_config_page_set_content (EMailConfigPage *page,
				GtkWidget *content)
{
	GtkScrolledWindow *scrolled;
	GtkWidget *child;

	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));
	g_return_if_fail (!content || GTK_IS_WIDGET (content));

	scrolled = GTK_SCROLLED_WINDOW (page);

	if (content)
		gtk_container_add (GTK_CONTAINER (scrolled), content);

	gtk_scrolled_window_set_policy (scrolled, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (scrolled, GTK_SHADOW_NONE);

	child = gtk_bin_get_child (GTK_BIN (scrolled));
	if (GTK_IS_VIEWPORT (child))
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (child), GTK_SHADOW_OUT);

	gtk_widget_show (content);

	g_object_set (GTK_WIDGET (page),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	gtk_widget_show (GTK_WIDGET (page));
}

gint
e_mail_config_page_compare (GtkWidget *page_a,
                            GtkWidget *page_b)
{
	EMailConfigPageInterface *interface_a = NULL;
	EMailConfigPageInterface *interface_b = NULL;

	if (E_IS_MAIL_CONFIG_PAGE (page_a))
		interface_a = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page_a);

	if (E_IS_MAIL_CONFIG_PAGE (page_b))
		interface_b = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page_b);

	if (interface_a == interface_b)
		return 0;

	if (!interface_a || !interface_b) {
		if (interface_a == interface_b)
			return 0;

		return interface_a ? -1 : 1;
	}

	/* coverity[var_deref_op] */
	if (interface_a->sort_order < interface_b->sort_order)
		return -1;

	if (interface_a->sort_order > interface_b->sort_order)
		return 1;

	return 0;
}

static gboolean
mail_config_page_emit_changed_idle (gpointer user_data)
{
	EMailConfigPage *page = user_data;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_PAGE (page), FALSE);

	g_signal_emit (page, signals[CHANGED], 0);

	return FALSE;
}

void
e_mail_config_page_changed (EMailConfigPage *page)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	if (e_util_is_main_thread (NULL)) {
		g_signal_emit (page, signals[CHANGED], 0);
	} else {
		/* Ensure the signal is emitted in the main/UI thread. */
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			mail_config_page_emit_changed_idle,
			g_object_ref (page), g_object_unref);
	}
}

void
e_mail_config_page_setup_defaults (EMailConfigPage *page)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	g_signal_emit (page, signals[SETUP_DEFAULTS], 0);
}

gboolean
e_mail_config_page_check_complete (EMailConfigPage *page)
{
	gboolean complete;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_PAGE (page), FALSE);

	g_signal_emit (page, signals[CHECK_COMPLETE], 0, &complete);

	return complete;
}

void
e_mail_config_page_commit_changes (EMailConfigPage *page,
                                   GQueue *source_queue)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));
	g_return_if_fail (source_queue != NULL);

	g_signal_emit (page, signals[COMMIT_CHANGES], 0, source_queue);
}

gboolean
e_mail_config_page_submit_sync (EMailConfigPage *page,
                                GCancellable *cancellable,
                                GError **error)
{
	EMailConfigPageInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_PAGE (page), FALSE);

	iface = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page);
	g_return_val_if_fail (iface->submit_sync != NULL, FALSE);

	return iface->submit_sync (page, cancellable, error);
}

void
e_mail_config_page_submit (EMailConfigPage *page,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	EMailConfigPageInterface *iface;

	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	iface = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page);
	g_return_if_fail (iface->submit != NULL);

	return iface->submit (page, cancellable, callback, user_data);
}

gboolean
e_mail_config_page_submit_finish (EMailConfigPage *page,
                                  GAsyncResult *result,
                                  GError **error)
{
	EMailConfigPageInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_PAGE (page), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	iface = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page);
	g_return_val_if_fail (iface->submit_finish != NULL, FALSE);

	return iface->submit_finish (page, result, error);
}

