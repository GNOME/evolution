/*
 * e-mail-parser-message-external.c
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

#include <string.h>
#include <ctype.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"

typedef EMailParserExtension EMailParserMessageExternal;
typedef EMailParserExtensionClass EMailParserMessageExternalClass;

GType e_mail_parser_message_external_get_type (void);

G_DEFINE_TYPE (
	EMailParserMessageExternal,
	e_mail_parser_message_external,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"message/external-body",
	NULL
};

static gboolean
empe_msg_external_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	EMailPart *mail_part;
	CamelMimePart *newpart;
	CamelContentType *type;
	const gchar *access_type;
	const gchar *mime_type;
	gchar *url = NULL, *desc = NULL;
	gchar *content;
	gint len;

	newpart = camel_mime_part_new ();

	/* needs to be cleaner */
	type = camel_mime_part_get_content_type (part);
	access_type = camel_content_type_param (type, "access-type");
	if (!access_type) {
		const gchar *msg = _("Malformed external-body part");
		mime_type = "text/plain";
		camel_mime_part_set_content (
			newpart, msg, strlen (msg), mime_type);
		goto addPart;
	}

	if (!g_ascii_strcasecmp (access_type, "ftp") ||
	    !g_ascii_strcasecmp (access_type, "anon-ftp")) {
		const gchar *name, *site, *dir, *mode;
		gchar *path;
		gchar ftype[16];

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		dir = camel_content_type_param (type, "directory");
		mode = camel_content_type_param (type, "mode");
		if (name == NULL || site == NULL)
			goto fail;

		/* Generate the path. */
		if (dir)
			path = g_strdup_printf ("/%s/%s", *dir == '/' ? dir + 1 : dir, name);
		else
			path = g_strdup_printf ("/%s", *name == '/' ? name + 1 : name);

		if (mode && *mode)
			sprintf (ftype, ";type=%c",  *mode);
		else
			ftype[0] = 0;

		url = g_strdup_printf ("ftp://%s%s%s", site, path, ftype);
		g_free (path);
		desc = g_strdup_printf (_("Pointer to FTP site (%s)"), url);
	} else if (!g_ascii_strcasecmp (access_type, "local-file")) {
		const gchar *name, *site;

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		if (name == NULL)
			goto fail;

		url = g_filename_to_uri (name, NULL, NULL);
		if (site)
			desc = g_strdup_printf (_("Pointer to local file (%s) valid at site “%s”"), name, site);
		else
			desc = g_strdup_printf (_("Pointer to local file (%s)"), name);
	} else if (!g_ascii_strcasecmp (access_type, "URL")) {
		const gchar *urlparam;
		gchar *s, *d;

		/* RFC 2017 */
		urlparam = camel_content_type_param (type, "url");
		if (urlparam == NULL)
			goto fail;

		/* For obscure MIMEy reasons, the URL may be split into words */
		url = g_strdup (urlparam);
		s = d = url;
		while (*s) {
			if (!isspace ((guchar) * s))
				*d++ = *s;
			s++;
		}
		*d = 0;
		desc = g_strdup_printf (_("Pointer to remote data (%s)"), url);
	} else {
		goto fail;
	}

	mime_type = "text/html";
	content = g_strdup_printf ("<a href=\"%s\">%s</a>", url, desc);
	camel_mime_part_set_content (
		newpart, content, strlen (content), mime_type);
	g_free (content);

	g_free (url);
	g_free (desc);

	goto addPart;

fail:
	content = g_strdup_printf (
		_("Pointer to unknown external data (“%s” type)"),
		access_type);
	mime_type = "text/plain";
	camel_mime_part_set_content (
		newpart, content, strlen (content), mime_type);
	g_free (content);

addPart:
	len = part_id->len;
	g_string_append (part_id, ".msg_external");
	mail_part = e_mail_part_new (part, part_id->str);
	e_mail_part_set_mime_type (mail_part, mime_type);
	g_string_truncate (part_id, len);

	g_queue_push_tail (out_mail_parts, mail_part);

	return TRUE;
}

static void
e_mail_parser_message_external_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_msg_external_parse;
}

static void
e_mail_parser_message_external_init (EMailParserExtension *extension)
{
}
