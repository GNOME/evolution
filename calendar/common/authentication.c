/*
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
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>
#include "authentication.h"
#include <libedataserver/e-url.h>

static gchar *
auth_func_cb (ECal *ecal,
              const gchar *prompt,
              const gchar *key,
              gpointer user_data)
{
	gboolean remember;
	gchar *password, *auth_domain;
	ESource *source;
	const gchar *component_name;

	source = e_cal_get_source (ecal);
	auth_domain = e_source_get_duped_property (source, "auth-domain");
	component_name = auth_domain ? auth_domain : "Calendar";
	password = e_passwords_get_password (component_name, key);

	if (!password)
		password = e_passwords_ask_password (
			_("Enter password"),
			component_name, key, prompt,
			E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET |
			E_PASSWORDS_ONLINE,
			&remember, NULL);

	g_free (auth_domain);

	return password;
}

static gchar *
build_pass_key (ECal *ecal)
{
	gchar *euri_str;
	const gchar *uri;
	EUri *euri;

	uri = e_cal_get_uri (ecal);

	euri = e_uri_new (uri);
	euri_str = e_uri_to_string (euri, FALSE);

	e_uri_free (euri);
	return euri_str;
}

void
e_auth_cal_forget_password (ECal *ecal)
{
	ESource *source = NULL;
	const gchar *auth_domain = NULL, *component_name = NULL,  *auth_type = NULL;

	source = e_cal_get_source (ecal);
	auth_domain = e_source_get_property (source, "auth-domain");
	component_name = auth_domain ? auth_domain : "Calendar";

	auth_type = e_source_get_property (source, "auth-type");
	if (auth_type) {
		gchar *key = NULL;

		key = build_pass_key (ecal);
		e_passwords_forget_password (component_name, key);
		g_free (key);
	}

	e_passwords_forget_password (component_name, e_source_get_uri (source));
}

ECal *
e_auth_new_cal_from_default (ECalSourceType type)
{
	ECal *ecal = NULL;

	if (!e_cal_open_default (&ecal, type, auth_func_cb, NULL, NULL))
		return NULL;

	return ecal;
}

ECal *
e_auth_new_cal_from_source (ESource *source, ECalSourceType type)
{
	ECal *cal;

	cal = e_cal_new (source, type);
	if (cal)
		e_cal_set_auth_func (cal, (ECalAuthFunc) auth_func_cb, NULL);

	return cal;
}

typedef struct {
	ECal *cal;
	GtkWindow *parent;
	GCancellable *cancellable;
	ECalSourceType source_type;
	icaltimezone *default_zone;

	/* Authentication Details */
	gchar *auth_component;
} LoadContext;

static void
load_cal_source_context_free (LoadContext *context)
{
	if (context->cal != NULL)
		g_object_unref (context->cal);

	if (context->parent != NULL)
		g_object_unref (context->parent);

	if (context->cancellable != NULL)
		g_object_unref (context->cancellable);

	g_free (context->auth_component);

	g_slice_free (LoadContext, context);
}

static void
load_cal_source_get_auth_details (ESource *source,
                                  LoadContext *context)
{
	const gchar *property;

	/* ECal figures out most of the details before invoking the
	 * authentication callback, but we still need a component name
	 * for e_passwords_ask_password(). */

	/* auth_component */

	property = e_source_get_property (source, "auth-domain");

	if (property == NULL)
		property = "Calendar";

	context->auth_component = g_strdup (property);
}

static gchar *
load_cal_source_authenticate (ECal *cal,
                              const gchar *prompt,
                              const gchar *uri,
                              gpointer not_used)
{
	const gchar *auth_component;
	const gchar *title;
	gboolean remember;  /* not used */
	GtkWindow *parent;
	gchar *password;

	/* XXX Dig up authentication info embedded in the ECal instance.
	 * (See load_cal_source_thread() for an explanation of why.) */
	auth_component = g_object_get_data (G_OBJECT (cal), "auth-component");
	g_return_val_if_fail (auth_component != NULL, NULL);

	parent = g_object_get_data (G_OBJECT (cal), "parent-window");

	/* Remember the URI so we don't have to reconstruct it if
	 * authentication fails and we have to forget the password. */
	g_object_set_data_full (
		G_OBJECT (cal),
		"auth-uri", g_strdup (uri),
		(GDestroyNotify) g_free);

	/* XXX Dialog windows should not have titles. */
	title = "";

	password = e_passwords_get_password (auth_component, uri);

	if (password == NULL)
		password = e_passwords_ask_password (
			title, auth_component, uri,
			prompt, E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET | E_PASSWORDS_ONLINE,
			&remember, parent);

	return password;
}

static void
load_cal_source_thread (GSimpleAsyncResult *simple,
                        ESource *source,
                        GCancellable *cancellable)
{
	ECal *cal;
	LoadContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	/* XXX This doesn't take a GError, it just dumps
	 *     error messages to the terminal... so broken. */
	cal = e_cal_new (source, context->source_type);
	g_return_if_fail (cal != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		g_simple_async_result_set_from_error (simple, error);
		g_object_unref (cal);
		g_error_free (error);
		return;
	}

	if (!e_cal_set_default_timezone (cal, context->default_zone, &error)) {
		g_simple_async_result_set_from_error (simple, error);
		g_object_unref (cal);
		g_error_free (error);
		return;
	}

	/* XXX e_cal_set_auth_func() does not take a GDestroyNotify callback
	 *     for the 'user_data' argument, which makes the argument rather
	 *     useless.  So instead, we'll embed the information needed by
	 *     the authentication callback directly into the ECal instance
	 *     using g_object_set_data_full(). */
	g_object_set_data_full (
		G_OBJECT (cal), "auth-component",
		g_strdup (context->auth_component),
		(GDestroyNotify) g_free);
	if (context->parent != NULL)
		g_object_set_data_full (
			G_OBJECT (cal), "parent-window",
			g_object_ref (context->parent),
			(GDestroyNotify) g_object_unref);

	e_cal_set_auth_func (
		cal, (ECalAuthFunc) load_cal_source_authenticate, NULL);

try_again:
	if (!e_cal_open (cal, FALSE, &error))
		goto fail;

	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		g_simple_async_result_set_from_error (simple, error);
		g_object_unref (cal);
		g_error_free (error);
		return;
	}

	context->cal = cal;

	return;

fail:
	g_return_if_fail (error != NULL);

	/* If authentication failed, forget the password and reprompt. */
	if (g_error_matches (
		error, E_CALENDAR_ERROR,
		E_CALENDAR_STATUS_AUTHENTICATION_FAILED)) {
		const gchar *auth_uri;

		/* Retrieve the URI set by the authentication function. */
		auth_uri = g_object_get_data (G_OBJECT (cal), "auth-uri");

		e_passwords_forget_password (
			context->auth_component, auth_uri);
		g_clear_error (&error);
		goto try_again;

	/* XXX Might this cause a busy loop? */
	} else if (g_error_matches (
		error, E_CALENDAR_ERROR, E_CALENDAR_STATUS_BUSY)) {
		g_clear_error (&error);
		g_usleep (250000);
		goto try_again;

	} else {
		g_simple_async_result_set_from_error (simple, error);
		g_object_unref (cal);
		g_error_free (error);
	}
}

/**
 * e_load_cal_source_async:
 * @source: an #ESource
 * @source_type: the type of #ECal to load
 * @default_zone: default time zone, or %NULL to use UTC
 * @parent: parent window for password dialogs, or %NULL
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to @callback
 *
 * Creates a new #ECal specified by @source and opens it, prompting the
 * user for authentication if necessary.
 *
 * When the operation is finished, @callback will be called.  You can
 * then call e_load_cal_source_finish() to obtain the resulting #ECal.
 **/
void
e_load_cal_source_async (ESource *source,
                         ECalSourceType source_type,
                         icaltimezone *default_zone,
                         GtkWindow *parent,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;
	LoadContext *context;

	g_return_if_fail (E_IS_SOURCE (source));

	/* Source must have a group so we can obtain its URI. */
	g_return_if_fail (e_source_peek_group (source) != NULL);

	if (parent != NULL) {
		g_return_if_fail (GTK_IS_WINDOW (parent));
		g_object_ref (parent);
	}

	if (cancellable != NULL) {
		g_return_if_fail (G_IS_CANCELLABLE (cancellable));
		g_object_ref (cancellable);
	} else {
		/* always provide cancellable, because the code depends on it */
		cancellable = g_cancellable_new ();
	}

	if (default_zone == NULL)
		default_zone = icaltimezone_get_utc_timezone ();

	context = g_slice_new0 (LoadContext);
	context->parent = parent;
	context->cancellable = cancellable;
	context->source_type = source_type;
	context->default_zone = default_zone;

	/* Extract authentication details from the ESource before
	 * spawning the thread, since ESource is not thread-safe. */
	load_cal_source_get_auth_details (source, context);

	simple = g_simple_async_result_new (
		G_OBJECT (source), callback,
		user_data, e_load_cal_source_async);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify)
		load_cal_source_context_free);

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc) load_cal_source_thread,
		G_PRIORITY_DEFAULT, context->cancellable);

	g_object_unref (simple);
}

/**
 * e_load_cal_source_finish:
 * @source: an #ESource
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous #ECal open operation started with
 * e_load_cal_source_async().  If an error occurred, or the user
 * declined to authenticate, the function will return %NULL and
 * set @error.
 *
 * Returns: a ready-to-use #ECal, or %NULL on error
 **/
ECal *
e_load_cal_source_finish (ESource *source,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	LoadContext *context;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
			result, G_OBJECT (source),
			e_load_cal_source_async), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);
	g_return_val_if_fail (context != NULL, NULL);

	return g_object_ref (context->cal);
}
