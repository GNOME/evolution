/*
 * e-mail-folder-utils.c
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

#include "e-mail-folder-utils.h"

#include <config.h>
#include <glib/gi18n-lib.h>

/* X-Mailer header value */
#define X_MAILER ("Evolution " VERSION SUB_VERSION " " VERSION_COMMENT)

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	GCancellable *cancellable;
	gchar *message_uid;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->cancellable != NULL)
		g_object_unref (context->cancellable);

	g_free (context->message_uid);

	g_slice_free (AsyncContext, context);
}

static void
mail_folder_append_message_ready (CamelFolder *folder,
                                  GAsyncResult *result,
                                  GSimpleAsyncResult *simple)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	camel_folder_append_message_finish (
		folder, result, &context->message_uid, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	camel_operation_pop_message (context->cancellable);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

void
e_mail_folder_append_message (CamelFolder *folder,
                              CamelMimeMessage *message,
                              CamelMessageInfo *info,
                              gint io_priority,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;
	CamelMedium *medium;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	medium = CAMEL_MEDIUM (message);

	context = g_slice_new0 (AsyncContext);

	if (G_IS_CANCELLABLE (cancellable))
		context->cancellable = g_object_ref (cancellable);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_append_message);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	camel_operation_push_message (
		context->cancellable,
		_("Saving message to folder '%s'"),
		camel_folder_get_full_name (folder));

	if (camel_medium_get_header (medium, "X-Mailer") == NULL)
		camel_medium_set_header (medium, "X-Mailer", X_MAILER);

	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	camel_folder_append_message (
		folder, message, info, io_priority,
		context->cancellable, (GAsyncReadyCallback)
		mail_folder_append_message_ready, simple);
}

gboolean
e_mail_folder_append_message_finish (CamelFolder *folder,
                                     GAsyncResult *result,
                                     gchar **appended_uid,
                                     GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_append_message), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (appended_uid != NULL) {
		*appended_uid = context->message_uid;
		context->message_uid = NULL;
	}

	return TRUE;
}
