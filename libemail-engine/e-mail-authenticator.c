/*
 * e-mail-authenticator.c
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

#include "e-mail-authenticator.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_AUTHENTICATOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_AUTHENTICATOR, EMailAuthenticatorPrivate))

struct _EMailAuthenticatorPrivate {
	CamelService *service;
	gchar *mechanism;
};

enum {
	PROP_0,
	PROP_MECHANISM,
	PROP_SERVICE
};

/* Forward Declarations */
static void	e_mail_authenticator_interface_init
				(ESourceAuthenticatorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailAuthenticator,
	e_mail_authenticator,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SOURCE_AUTHENTICATOR,
		e_mail_authenticator_interface_init))

static void
mail_authenticator_set_mechanism (EMailAuthenticator *auth,
                                  const gchar *mechanism)
{
	g_return_if_fail (auth->priv->mechanism == NULL);

	auth->priv->mechanism = g_strdup (mechanism);
}

static void
mail_authenticator_set_service (EMailAuthenticator *auth,
                                CamelService *service)
{
	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (auth->priv->service == NULL);

	auth->priv->service = g_object_ref (service);
}

static void
mail_authenticator_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MECHANISM:
			mail_authenticator_set_mechanism (
				E_MAIL_AUTHENTICATOR (object),
				g_value_get_string (value));
			return;

		case PROP_SERVICE:
			mail_authenticator_set_service (
				E_MAIL_AUTHENTICATOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_authenticator_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MECHANISM:
			g_value_set_string (
				value,
				e_mail_authenticator_get_mechanism (
				E_MAIL_AUTHENTICATOR (object)));
			return;

		case PROP_SERVICE:
			g_value_set_object (
				value,
				e_mail_authenticator_get_service (
				E_MAIL_AUTHENTICATOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_authenticator_dispose (GObject *object)
{
	EMailAuthenticatorPrivate *priv;

	priv = E_MAIL_AUTHENTICATOR_GET_PRIVATE (object);

	if (priv->service != NULL) {
		g_object_unref (priv->service);
		priv->service = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_authenticator_parent_class)->dispose (object);
}

static void
mail_authenticator_finalize (GObject *object)
{
	EMailAuthenticatorPrivate *priv;

	priv = E_MAIL_AUTHENTICATOR_GET_PRIVATE (object);

	g_free (priv->mechanism);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_authenticator_parent_class)->finalize (object);
}

static ESourceAuthenticationResult
mail_authenticator_try_password_sync (ESourceAuthenticator *auth,
                                      const GString *password,
                                      GCancellable *cancellable,
                                      GError **error)
{
	CamelService *service;
	EMailAuthenticator *mail_auth;
	CamelAuthenticationResult camel_result;
	ESourceAuthenticationResult source_result;
	const gchar *mechanism;

	mail_auth = E_MAIL_AUTHENTICATOR (auth);
	service = e_mail_authenticator_get_service (mail_auth);
	mechanism = e_mail_authenticator_get_mechanism (mail_auth);

	camel_service_set_password (service, password->str);

	camel_result = camel_service_authenticate_sync (
		service, mechanism, cancellable, error);

	switch (camel_result) {
		case CAMEL_AUTHENTICATION_ERROR:
			source_result = E_SOURCE_AUTHENTICATION_ERROR;
			break;
		case CAMEL_AUTHENTICATION_ACCEPTED:
			source_result = E_SOURCE_AUTHENTICATION_ACCEPTED;
			break;
		case CAMEL_AUTHENTICATION_REJECTED:
			source_result = E_SOURCE_AUTHENTICATION_REJECTED;
			break;
		default:
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Invalid authentication result code (%d)"),
				camel_result);
			source_result = E_SOURCE_AUTHENTICATION_ERROR;
			break;
	}

	return source_result;
}

static void
e_mail_authenticator_class_init (EMailAuthenticatorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailAuthenticatorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_authenticator_set_property;
	object_class->get_property = mail_authenticator_get_property;
	object_class->dispose = mail_authenticator_dispose;
	object_class->finalize = mail_authenticator_finalize;

	g_object_class_install_property (
		object_class,
		PROP_MECHANISM,
		g_param_spec_string (
			"mechanism",
			"Mechanism",
			"Authentication mechanism",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SERVICE,
		g_param_spec_object (
			"service",
			"Service",
			"The CamelService to authenticate",
			CAMEL_TYPE_SERVICE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_authenticator_interface_init (ESourceAuthenticatorInterface *iface)
{
	iface->try_password_sync = mail_authenticator_try_password_sync;
}

static void
e_mail_authenticator_init (EMailAuthenticator *auth)
{
	auth->priv = E_MAIL_AUTHENTICATOR_GET_PRIVATE (auth);
}

ESourceAuthenticator *
e_mail_authenticator_new (CamelService *service,
                          const gchar *mechanism)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);

	return g_object_new (
		E_TYPE_MAIL_AUTHENTICATOR,
		"service", service, "mechanism", mechanism, NULL);
}

CamelService *
e_mail_authenticator_get_service (EMailAuthenticator *auth)
{
	g_return_val_if_fail (E_IS_MAIL_AUTHENTICATOR (auth), NULL);

	return auth->priv->service;
}

const gchar *
e_mail_authenticator_get_mechanism (EMailAuthenticator *auth)
{
	g_return_val_if_fail (E_IS_MAIL_AUTHENTICATOR (auth), NULL);

	return auth->priv->mechanism;
}

