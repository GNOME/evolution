/*
 * e-mail-config-page.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-mail-config-page.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-marshal.h>

enum {
	CHANGED,
	SETUP_DEFAULTS,
	CHECK_COMPLETE,
	COMMIT_CHANGES,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (
	EMailConfigPage,
	e_mail_config_page,
	GTK_TYPE_WIDGET)

static gboolean
mail_config_page_check_complete (EMailConfigPage *page)
{
	return TRUE;
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
e_mail_config_page_default_init (EMailConfigPageInterface *interface)
{
	interface->title = _("Untitled");
	interface->page_type = GTK_ASSISTANT_PAGE_CONTENT;

	interface->check_complete = mail_config_page_check_complete;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_INTERFACE (interface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SETUP_DEFAULTS] = g_signal_new (
		"setup-defaults",
		G_TYPE_FROM_INTERFACE (interface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, setup_defaults),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CHECK_COMPLETE] = g_signal_new (
		"check-complete",
		G_TYPE_FROM_INTERFACE (interface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, check_complete),
		mail_config_page_check_complete_accumulator, NULL,
		e_marshal_BOOLEAN__VOID,
		G_TYPE_BOOLEAN, 0);

	signals[COMMIT_CHANGES] = g_signal_new (
		"commit-changes",
		G_TYPE_FROM_INTERFACE (interface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigPageInterface, commit_changes),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}

void
e_mail_config_page_changed (EMailConfigPage *page)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	g_signal_emit (page, signals[CHANGED], 0);
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

	if (interface_a != NULL && interface_b == NULL)
		return -1;

	if (interface_a == NULL && interface_b != NULL)
		return 1;

	if (interface_a->sort_order < interface_b->sort_order)
		return -1;

	if (interface_a->sort_order > interface_b->sort_order)
		return 1;

	return 0;
}

