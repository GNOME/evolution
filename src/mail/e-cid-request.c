/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>

#include "e-cid-request.h"

G_DEFINE_INTERFACE (ECidResolver, e_cid_resolver, G_TYPE_OBJECT)

static void
e_cid_resolver_default_init (ECidResolverInterface *iface)
{
}

CamelMimePart *
e_cid_resolver_ref_part (ECidResolver *resolver,
			 const gchar *uri)
{
	ECidResolverInterface *iface;

	g_return_val_if_fail (E_IS_CID_RESOLVER (resolver), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	iface = E_CID_RESOLVER_GET_INTERFACE (resolver);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->ref_part != NULL, NULL);

	return iface->ref_part (resolver, uri);
}

gchar *
e_cid_resolver_dup_mime_type (ECidResolver *resolver,
			      const gchar *uri)
{
	ECidResolverInterface *iface;

	g_return_val_if_fail (E_IS_CID_RESOLVER (resolver), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	iface = E_CID_RESOLVER_GET_INTERFACE (resolver);
	g_return_val_if_fail (iface != NULL, NULL);

	if (iface->dup_mime_type)
		return iface->dup_mime_type (resolver, uri);

	return NULL;
}

struct _ECidRequestPrivate {
	gint dummy;
};

static void e_cid_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ECidRequest, e_cid_request, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ECidRequest)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_cid_request_content_request_init))

static gboolean
e_cid_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_CID_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "cid:", 4) == 0;
}

static gboolean
e_cid_request_process_sync (EContentRequest *request,
			    const gchar *uri,
			    GObject *requester,
			    GInputStream **out_stream,
			    gint64 *out_stream_length,
			    gchar **out_mime_type,
			    GCancellable *cancellable,
			    GError **error)
{
	GByteArray *byte_array;
	CamelStream *output_stream;
	CamelDataWrapper *dw;
	CamelMimePart *mime_part;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_CID_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	if (!E_IS_CID_RESOLVER (requester))
		return FALSE;

	mime_part = e_cid_resolver_ref_part (E_CID_RESOLVER (requester), uri);
	if (!mime_part)
		return FALSE;

	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	g_return_val_if_fail (dw != NULL, FALSE);

	byte_array = g_byte_array_new ();
	output_stream = camel_stream_mem_new ();

	/* We retain ownership of the byte array. */
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (output_stream), byte_array);

	if (camel_data_wrapper_decode_to_stream_sync (dw, output_stream, cancellable, error)) {
		GBytes *bytes;
		gchar *mime_type;

		bytes = g_byte_array_free_to_bytes (byte_array);

		success = TRUE;

		*out_stream = g_memory_input_stream_new_from_bytes (bytes);
		*out_stream_length = g_bytes_get_size (bytes);

		mime_type = camel_data_wrapper_get_mime_type (dw);
		if (mime_type && *mime_type) {
			*out_mime_type = mime_type;
		} else {
			g_free (mime_type);
			*out_mime_type = e_cid_resolver_dup_mime_type (E_CID_RESOLVER (requester), uri);

			if (!*out_mime_type)
				*out_mime_type = g_strdup ("application/octet-stream");
		}

		g_bytes_unref (bytes);
	} else {
		g_byte_array_free (byte_array, TRUE);
	}

	g_object_unref (output_stream);
	g_object_unref (mime_part);

	return success;
}

static void
e_cid_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_cid_request_can_process_uri;
	iface->process_sync = e_cid_request_process_sync;
}

static void
e_cid_request_class_init (ECidRequestClass *class)
{
}

static void
e_cid_request_init (ECidRequest *request)
{
	request->priv = e_cid_request_get_instance_private (request);
}

EContentRequest *
e_cid_request_new (void)
{
	return g_object_new (E_TYPE_CID_REQUEST, NULL);
}
