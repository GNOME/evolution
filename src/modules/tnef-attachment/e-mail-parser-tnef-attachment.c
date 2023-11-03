/*
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
 */

#include "evolution-config.h"

#include "e-mail-parser-tnef-attachment.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdio.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_YTNEF_H
#include <ytnef.h>
#elif defined HAVE_LIBYTNEF_YTNEF_H
#include <libytnef/ytnef.h>
#endif

#include <libebackend/libebackend.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <mail/em-utils.h>

#define d(x)

typedef struct _EMailParserTnefAttachment {
	EMailParserExtension parent;

	GSettings *settings;
	gint mode;
	gboolean show_suppressed;
} EMailParserTnefAttachment;

typedef struct _EMailParserTnefAttachmentClass {
	EMailParserExtensionClass parent_class;
} EMailParserTnefAttachmentClass;

typedef EExtension EMailParserTnefAttachmentLoader;
typedef EExtensionClass EMailParserTnefAttachmentLoaderClass;

GType e_mail_parser_tnef_attachment_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserTnefAttachment,
	e_mail_parser_tnef_attachment,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/vnd.ms-tnef",
	"application/ms-tnef",
	NULL
};

static gint verbose = 0;
static gint saveRTF = 0;
static gint saveintermediate = 0;
static void processTnef (TNEFStruct *tnef, const gchar *tmpdir, CamelMimePart **out_mainpart, GSList **out_attachments);
static void saveVCalendar (TNEFStruct *tnef, const gchar *tmpdir);
static void saveVCard (TNEFStruct *tnef, const gchar *tmpdir);
static void saveVTask (TNEFStruct *tnef, const gchar *tmpdir);

/* Other Prototypes */
static void fprintProperty (TNEFStruct *tnef, FILE *fptr, DWORD proptype, DWORD propid, const gchar text[]);
static void fprintUserProp (TNEFStruct *tnef, FILE *fptr, DWORD proptype, DWORD propid, const gchar text[]);
static void quotedfprint (FILE *fptr, variableLength *vl);
static void cstylefprint (FILE *fptr, variableLength *vl);
static void printRtf (FILE *fptr, variableLength *vl);
static void printRrule (FILE *fptr, const guchar *recur_data, gint size, TNEFStruct *tnef);
static guchar getRruleCount (guchar a, guchar b);
static guchar getRruleMonthNum (guchar a, guchar b);
static gchar * getRruleDayname (guchar a);

static gchar *
sanitize_filename (const gchar *filename)
{
	gchar * sanitized_name;
	sanitized_name = g_path_get_basename (filename);
	if (sanitized_name == NULL || !g_strcmp0 (sanitized_name, ".")) {
		g_free (sanitized_name);
		return NULL;
	} else {
		return g_strdelimit (sanitized_name, " ", '_');
	}
}

static gboolean
empe_tnef_attachment_parse (EMailParserExtension *extension,
                            EMailParser *parser,
                            CamelMimePart *part,
                            GString *part_id,
                            GCancellable *cancellable,
                            GQueue *out_mail_parts)
{
	gchar *tmpdir, *name;
	CamelStream *out;
	CamelMimePart *mainpart = NULL;
	CamelDataWrapper *content;
	GSList *attachments = NULL; /* CamelMimePart * */
	TNEFStruct tnef;
	gboolean success = FALSE;

	tmpdir = e_mkdtemp ("tnef-attachment-XXXXXX");
	if (tmpdir == NULL)
		return FALSE;

	name = g_build_filename (tmpdir, ".evo-attachment.tnef", NULL);

	out = camel_stream_fs_new_with_name (name, O_RDWR | O_CREAT, 0666, NULL);
	if (out == NULL) {
		g_free (tmpdir);
		g_free (name);
		return FALSE;
	}
	content = camel_medium_get_content ((CamelMedium *) part);
	if (content == NULL) {
		g_object_unref (out);
		g_free (tmpdir);
		g_free (name);
		return FALSE;
	}
	if (camel_data_wrapper_decode_to_stream_sync (content, out, cancellable, NULL) == -1
	    || camel_stream_flush (out, cancellable, NULL) == -1
	    || camel_stream_close (out, cancellable, NULL) == -1) {
		g_object_unref (out);
		g_free (tmpdir);
		g_free (name);
		return FALSE;
	}
	g_clear_object (&out);

	/* Extracting the winmail.dat */
	TNEFInitialize (&tnef);
	tnef.Debug = verbose;
	if (TNEFParseFile (name, &tnef) == -1) {
		printf ("ERROR processing file\n");
	} else {
		success = TRUE;
	}
	processTnef (&tnef, tmpdir, &mainpart, &attachments);

	TNEFFree (&tnef);
	/* Extraction done */

	if (mainpart) {
		success = TRUE;
		if (attachments) {
			CamelMultipart *mp;
			GSList *link;

			mp = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/mixed");
			camel_multipart_set_boundary (mp, NULL);
			camel_multipart_add_part (mp, mainpart);
			g_object_unref (mainpart);

			attachments = g_slist_reverse (attachments);

			for (link = attachments; link; link = g_slist_next (link)) {
				CamelMimePart *apart = link->data;

				camel_multipart_add_part (mp, apart);
			}

			mainpart = camel_mime_part_new ();
			camel_medium_set_content (CAMEL_MEDIUM (mainpart), CAMEL_DATA_WRAPPER (mp));

			g_slist_free_full (attachments, g_object_unref);
			g_object_unref (mp);
		}
	} else {
		CamelMultipart *mp;
		DIR *dir;
		struct dirent *d;

		g_warn_if_fail (attachments == NULL);

		dir = success ? opendir (tmpdir) : NULL;
		if (dir == NULL) {
			g_free (tmpdir);
			g_free (name);
			return FALSE;
		}

		mainpart = camel_mime_part_new ();

		mp = camel_multipart_new ();
		camel_data_wrapper_set_mime_type ((CamelDataWrapper *) mp, "multipart/mixed");
		camel_multipart_set_boundary (mp, NULL);

		camel_medium_set_content ((CamelMedium *) mainpart, (CamelDataWrapper *) mp);

		while ((d = readdir (dir))) {
			CamelMimePart *new_part;
			CamelStream *stream;
			gchar *path;
			gchar *guessed_mime_type;

			if (!strcmp (d->d_name, ".")
			    || !strcmp (d->d_name, "..")
			    || !strcmp (d->d_name, ".evo-attachment.tnef"))
			    continue;

			path = g_build_filename (tmpdir, d->d_name, NULL);

			stream = camel_stream_fs_new_with_name (path, O_RDONLY, 0, NULL);
			content = camel_data_wrapper_new ();
			camel_data_wrapper_construct_from_stream_sync (
				content, stream, NULL, NULL);
			g_object_unref (stream);

			new_part = camel_mime_part_new ();
			camel_mime_part_set_encoding (new_part, CAMEL_TRANSFER_ENCODING_BINARY);

			camel_medium_set_content ((CamelMedium *) new_part, content);
			g_object_unref (content);

			guessed_mime_type = e_mail_part_guess_mime_type (new_part);
			if (guessed_mime_type) {
				camel_data_wrapper_set_mime_type ((CamelDataWrapper *) new_part, guessed_mime_type);
				g_free (guessed_mime_type);
			}

			camel_mime_part_set_filename (new_part, d->d_name);

			g_free (path);

			camel_multipart_add_part (mp, new_part);
			g_object_unref (new_part);
		}

		closedir (dir);

		success = camel_multipart_get_number (mp) > 0;

		g_object_unref (mp);
	}

	if (success) {
		GQueue work_queue = G_QUEUE_INIT;
		gint len;

		len = part_id->len;
		g_string_append_printf (part_id, ".tnef");

		e_mail_parser_parse_part_as (
			parser, mainpart, part_id, "multipart/mixed",
			cancellable, &work_queue);

		e_queue_transfer (&work_queue, out_mail_parts);

		g_string_truncate (part_id, len);
	}

	g_object_unref (mainpart);

	g_free (name);
	g_free (tmpdir);

	return success;
}

static void
e_mail_parser_tnef_attachment_class_init (EMailParserTnefAttachmentClass *class)
{
	EMailParserExtensionClass *extension_class;

	extension_class = E_MAIL_PARSER_EXTENSION_CLASS (class);
	extension_class->mime_types = parser_mime_types;
	extension_class->parse = empe_tnef_attachment_parse;
}

void
e_mail_parser_tnef_attachment_class_finalize (EMailParserTnefAttachmentClass *class)
{
}

static void
e_mail_parser_tnef_attachment_init (EMailParserTnefAttachment *extension)
{
}

void
e_mail_parser_tnef_attachment_type_register (GTypeModule *type_module)
{
	e_mail_parser_tnef_attachment_register_type (type_module);
}

static variableLength *
e_tnef_get_string_prop (MAPIProps *props,
			guint32 tagid)
{
	variableLength *res;

	res = MAPIFindProperty (props, PROP_TAG (PT_UNICODE, tagid));

	if (res == MAPI_UNDEFINED)
		res = MAPIFindProperty (props, PROP_TAG (PT_STRING8, tagid));
	else if (res->data)
		res->size = strlen ((const gchar *) res->data);

	return res;
}

static variableLength *
e_tnef_get_string_user_prop (MAPIProps *props,
			     guint32 tagid)
{
	variableLength *res;

	res = MAPIFindUserProp (props, PROP_TAG (PT_UNICODE, tagid));

	if (res == MAPI_UNDEFINED)
		res = MAPIFindUserProp (props, PROP_TAG (PT_STRING8, tagid));
	else if (res->data)
		res->size = strlen ((const gchar *) res->data);

	return res;
}

static void
extract_attachments (TNEFStruct *tnef,
		     const gchar *tmpdir,
		     GSList **out_attachments)
{
	variableLength *filename;
	variableLength *filedata;
	Attachment *p;
	gint RealAttachment;
	gint object;
	gint count;

	p = tnef->starting_attach.next;
	count = 0;
	while (p != NULL) {
		count++;
		/* Make sure it has a size. */
		if (p->FileData.size > 0) {
			TNEFStruct emb_tnef;
			DWORD signature;

			object = 1;

			/* See if the contents are stored as "attached data" */
			/* Inside the MAPI blocks. */
			if ((filedata = MAPIFindProperty (&(p->MAPI), PROP_TAG (PT_OBJECT, PR_ATTACH_DATA_OBJ))) == MAPI_UNDEFINED) {
				if ((filedata = MAPIFindProperty (&(p->MAPI), PROP_TAG (PT_BINARY, PR_ATTACH_DATA_OBJ))) == MAPI_UNDEFINED) {
					/* Nope, standard TNEF stuff. */
					filedata = &(p->FileData);
					object = 0;
				}
			}

			/* See if this is an embedded TNEF stream. */
			RealAttachment = 1;
			/*  when this is an "embedded object", so skip the  16-byte identifier first. */
			memcpy (&signature, filedata->data + (object ? 16 : 0), sizeof (DWORD));
			if (TNEFCheckForSignature (signature) == 0) {
				/* Has a TNEF signature, so process it. */
				TNEFInitialize (&emb_tnef);
				emb_tnef.Debug = tnef->Debug;
				if (TNEFParseMemory ((guchar *) filedata->data + (object ? 16 : 0), filedata->size - (object ? 16 : 0), &emb_tnef) != -1) {
					processTnef (&emb_tnef, tmpdir, NULL, out_attachments);
					RealAttachment = 0;
				}
				TNEFFree (&emb_tnef);
			}
			if ((RealAttachment == 1) || (saveintermediate == 1)) {
				gchar tmpname[20];
				/* Ok, it's not an embedded stream, so now we */
				/* process it. */
				if ((filename = e_tnef_get_string_prop (&(p->MAPI), PR_ATTACH_LONG_FILENAME)) == MAPI_UNDEFINED) {
					if ((filename = e_tnef_get_string_prop (&(p->MAPI), PR_ATTACH_FILENAME)) == MAPI_UNDEFINED) {
						if ((filename = e_tnef_get_string_prop (&(p->MAPI), PR_DISPLAY_NAME)) == MAPI_UNDEFINED) {
							filename = &(p->Title);
						}
					}
				}
				if (filename->size == 1) {
					filename->size = 20;
					g_sprintf (tmpname, "file_%03i.dat", count);
					filename->data = (guchar *) tmpname;
				}
				if (out_attachments) {
					CamelMimePart *apart;
					variableLength *tmp;

					apart = camel_mime_part_new ();
					camel_mime_part_set_content (apart, (const gchar *) filedata->data + (object ? 16 : 0),
						filedata->size - (object ? 16 : 0), "application/octet-stream");
					camel_mime_part_set_filename (apart, (const gchar *) filename->data);
					camel_mime_part_set_encoding (apart, CAMEL_TRANSFER_ENCODING_BASE64);

					tmp = e_tnef_get_string_prop (&(p->MAPI), PR_ATTACH_CONTENT_ID);
					if (tmp != MAPI_UNDEFINED)
						camel_mime_part_set_content_id (apart, (const gchar *) tmp->data);

					tmp = e_tnef_get_string_prop (&(p->MAPI), PR_ATTACH_CONTENT_LOCATION);
					if (tmp != MAPI_UNDEFINED)
						camel_mime_part_set_content_location (apart, (const gchar *) tmp->data);

					tmp = e_tnef_get_string_prop (&(p->MAPI), PR_ATTACH_MIME_TAG);
					if (tmp != MAPI_UNDEFINED) {
						camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (apart), (const gchar *) tmp->data);
					} else {
						gchar *guessed_mime_type;

						guessed_mime_type = e_mail_part_guess_mime_type (apart);
						if (guessed_mime_type) {
							camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (apart), guessed_mime_type);
							g_free (guessed_mime_type);
						}
					}

					*out_attachments = g_slist_prepend (*out_attachments, apart);
				} else {
					FILE *fptr;
					gchar *absfilename, *ifilename;

					absfilename = sanitize_filename ((const gchar *) filename->data);
					if (!absfilename)
						return;
					ifilename = g_build_filename (tmpdir, absfilename, NULL);
					g_free (absfilename);

					if ((fptr = fopen (ifilename, "wb")) == NULL) {
						printf ("ERROR: Error writing file to disk!");
					} else {
						fwrite (filedata->data + (object ? 16 : 0), sizeof (BYTE), filedata->size - (object ? 16 : 0), fptr);
						fclose (fptr);
					}
					g_clear_pointer (&ifilename, g_free);
				}
			}
		} /* if size>0 */
		p = p->next;
	} /* while p!= null */
}

static void
processTnef (TNEFStruct *tnef,
             const gchar *tmpdir,
	     CamelMimePart **out_mainpart,
	     GSList **out_attachments) /* CamelMimePart * */
{
	variableLength *filename;
	gchar *ifilename = NULL;
	gchar *absfilename, *file;
	gint foundCal = 0;

	/* First see if this requires special processing. */
	/* ie: it's a Contact Card, Task, or Meeting request (vCal/vCard) */
	if (strcmp (tnef->messageClass, "IPM.Contact") == 0) {
		saveVCard (tnef, tmpdir);
	} else if (strcmp (tnef->messageClass, "IPM.Task") == 0) {
		saveVTask (tnef, tmpdir);
	} else if (strcmp (tnef->messageClass, "IPM.Appointment") == 0 ||
		   g_str_has_prefix (tnef->messageClass, "IPM.Microsoft Schedule.")) {
		saveVCalendar (tnef, tmpdir);
		foundCal = 1;
	}

	if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x24)) != MAPI_UNDEFINED) {
		if (strcmp ((const gchar *) filename->data, "IPM.Appointment") == 0 ||
		    g_str_has_prefix ((const gchar *) filename->data, "IPM.Microsoft Schedule.")) {
			/* If it's "indicated" twice, we don't want to save 2 calendar entries. */
			if (foundCal == 0) {
				saveVCalendar (tnef, tmpdir);
			}
		}
	}

	if (strcmp (tnef->messageClass, "IPM.Microsoft Mail.Note") == 0) {
		if ((saveRTF == 1) && (tnef->subject.size > 0)) {
			/* Description */
			if ((filename = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (PT_BINARY, PR_RTF_COMPRESSED))) != MAPI_UNDEFINED) {
				variableLength buf;
				if ((buf.data = DecompressRTF (filename, &buf.size)) != NULL) {
					FILE *fptr;

					file = sanitize_filename ((const gchar *) tnef->subject.data);
					if (!file)
						return;
					absfilename = g_strconcat (file, ".rtf", NULL);
					ifilename = g_build_filename (tmpdir, file, NULL);
					g_free (absfilename);
					g_free (file);

					if ((fptr = fopen (ifilename, "wb")) == NULL) {
						printf ("ERROR: Error writing file to disk!");
					} else {
						fwrite (buf.data, sizeof (BYTE), buf.size, fptr);
						fclose (fptr);
					}
					free (buf.data);
					g_clear_pointer (&ifilename, g_free);
				}
			}
		} else if (out_mainpart) {
			variableLength *body_html;

			body_html = e_tnef_get_string_user_prop (&(tnef->MapiProperties), PR_BODY_HTML);
			if (body_html == MAPI_UNDEFINED)
				body_html = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (PT_BINARY, PR_BODY_HTML));
			if (body_html != MAPI_UNDEFINED) {
				CamelMultipart *mp;
				GSList *attachments = NULL, *link;

				*out_mainpart = camel_mime_part_new ();
				camel_mime_part_set_encoding (*out_mainpart, CAMEL_TRANSFER_ENCODING_BINARY);
				camel_mime_part_set_content (*out_mainpart, (const gchar *) body_html->data, body_html->size, "text/html");

				extract_attachments (tnef, tmpdir, &attachments);

				if (attachments) {
					GSList *noncid_attachments = NULL;
					gboolean any_cid = FALSE;

					for (link = attachments; link && !any_cid; link = g_slist_next (link)) {
						CamelMimePart *apart = link->data;

						any_cid = camel_mime_part_get_content_id (apart) != NULL;
					}

					if (any_cid) {
						mp = camel_multipart_new ();
						camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/related");
						camel_multipart_set_boundary (mp, NULL);
						camel_multipart_add_part (mp, *out_mainpart);
						g_object_unref (*out_mainpart);

						*out_mainpart = camel_mime_part_new ();
						camel_medium_set_content (CAMEL_MEDIUM (*out_mainpart), CAMEL_DATA_WRAPPER (mp));

						attachments = g_slist_reverse (attachments);

						for (link = attachments; link; link = g_slist_next (link)) {
							CamelMimePart *apart = link->data;

							if (camel_mime_part_get_content_id (apart)) {
								camel_multipart_add_part (mp, apart);
							} else {
								noncid_attachments = g_slist_prepend (noncid_attachments, g_object_ref (apart));
							}
						}

						g_slist_free_full (attachments, g_object_unref);
						g_object_unref (mp);

						*out_attachments = noncid_attachments;
					} else {
						*out_attachments = attachments;
					}
				}

				return;
			}
		}
	}

	/* Now process each attachment */
	extract_attachments (tnef, tmpdir, NULL);
}

static void
saveVCard (TNEFStruct *tnef,
           const gchar *tmpdir)
{
    gchar *ifilename;
    gchar *absfilename, *file = NULL;
    FILE *fptr;
    variableLength *vl;
    variableLength *pobox, *street, *city, *state, *zip, *country;
    dtr thedate;
    gint boolean;

    if ((vl = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_DISPLAY_NAME)) == MAPI_UNDEFINED) {
	if ((vl = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_COMPANY_NAME)) == MAPI_UNDEFINED) {
	    if (tnef->subject.size > 0) {
	       file = sanitize_filename ((const gchar *) tnef->subject.data);
		if (!file)
		    return;
		absfilename = g_strconcat (file, ".vcard", NULL);
	    } else
		absfilename = g_strdup ("unknown.vcard");
     } else {
	    file = sanitize_filename ((const gchar *) vl->data);
	    if (!file)
		return;
	    absfilename = g_strconcat (file, ".vcard", NULL);
	}
    } else {
	file = sanitize_filename ((const gchar *) vl->data);
	if (!file)
	    return;
	absfilename = g_strconcat (file, ".vcard", NULL);
    }

    ifilename = g_build_filename (tmpdir, absfilename, NULL);
    g_free (file);
    g_free (absfilename);

    if ((fptr = fopen (ifilename, "wb")) == NULL) {
	    printf ("Error writing file to disk!");
    } else {
	fprintf (fptr, "BEGIN:VCARD\n");
	fprintf (fptr, "VERSION:2.1\n");
	if (vl != MAPI_UNDEFINED) {
	    fprintf (fptr, "FN:%s\n", vl->data);
	}
	fprintProperty (tnef, fptr, PT_UNICODE, PR_NICKNAME, "NICKNAME:%s\n");
	fprintUserProp (tnef, fptr, PT_UNICODE, 0x8554, "MAILER:Microsoft Outlook %s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_SPOUSE_NAME, "X-EVOLUTION-SPOUSE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_MANAGER_NAME, "X-EVOLUTION-MANAGER:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_ASSISTANT, "X-EVOLUTION-ASSISTANT:%s\n");

        /* Organizational */
	if ((vl = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_COMPANY_NAME)) != MAPI_UNDEFINED) {
	    if (vl->size > 0) {
		if ((vl->size == 1) && (vl->data[0] == 0)) {
		} else {
		    fprintf (fptr,"ORG:%s", vl->data);
		    if ((vl = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_DEPARTMENT_NAME)) != MAPI_UNDEFINED) {
			fprintf (fptr,";%s", vl->data);
		    }
		    fprintf (fptr, "\n");
		}
	    }
	}

	fprintProperty (tnef, fptr, PT_UNICODE, PR_OFFICE_LOCATION, "X-EVOLUTION-OFFICE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_TITLE, "TITLE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_PROFESSION, "ROLE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_BODY, "NOTE:%s\n");
	if (tnef->body.size > 0) {
	    fprintf (fptr, "NOTE;QUOTED-PRINTABLE:");
	    quotedfprint (fptr, &(tnef->body));
	    fprintf (fptr,"\n");
	}

        /* Business Address */
	boolean = 0;
	if ((pobox = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_POST_OFFICE_BOX)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((street = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_STREET_ADDRESS)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((city = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_LOCALITY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((state = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_STATE_OR_PROVINCE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((zip = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_POSTAL_CODE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((country = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_COUNTRY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if (boolean == 1) {
	    fprintf (fptr, "ADR;QUOTED-PRINTABLE;WORK:");
	    if (pobox != MAPI_UNDEFINED) {
		quotedfprint (fptr, pobox);
	    }
	    fprintf (fptr, ";;");
	    if (street != MAPI_UNDEFINED) {
		quotedfprint (fptr, street);
	    }
	    fprintf (fptr, ";");
	    if (city != MAPI_UNDEFINED) {
		quotedfprint (fptr, city);
	    }
	    fprintf (fptr, ";");
	    if (state != MAPI_UNDEFINED) {
		quotedfprint (fptr, state);
	    }
	    fprintf (fptr, ";");
	    if (zip != MAPI_UNDEFINED) {
		quotedfprint (fptr, zip);
	    }
	    fprintf (fptr, ";");
	    if (country != MAPI_UNDEFINED) {
		quotedfprint (fptr, country);
	    }
	    fprintf (fptr,"\n");
	    if ((vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x801b)) != MAPI_UNDEFINED) {
		fprintf (fptr, "LABEL;QUOTED-PRINTABLE;WORK:");
		quotedfprint (fptr, vl);
		fprintf (fptr,"\n");
	    }
	}

        /* Home Address */
	boolean = 0;
	if ((pobox = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_POST_OFFICE_BOX)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((street = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_STREET)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((city = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_CITY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((state = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_STATE_OR_PROVINCE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((zip = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_POSTAL_CODE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((country = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_HOME_ADDRESS_COUNTRY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if (boolean == 1) {
	    fprintf (fptr, "ADR;QUOTED-PRINTABLE;HOME:");
	    if (pobox != MAPI_UNDEFINED) {
		quotedfprint (fptr, pobox);
	    }
	    fprintf (fptr, ";;");
	    if (street != MAPI_UNDEFINED) {
		quotedfprint (fptr, street);
	    }
	    fprintf (fptr, ";");
	    if (city != MAPI_UNDEFINED) {
		quotedfprint (fptr, city);
	    }
	    fprintf (fptr, ";");
	    if (state != MAPI_UNDEFINED) {
		quotedfprint (fptr, state);
	    }
	    fprintf (fptr, ";");
	    if (zip != MAPI_UNDEFINED) {
		quotedfprint (fptr, zip);
	    }
	    fprintf (fptr, ";");
	    if (country != MAPI_UNDEFINED) {
		quotedfprint (fptr, country);
	    }
	    fprintf (fptr,"\n");
	    if ((vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x801a)) != MAPI_UNDEFINED) {
		fprintf (fptr, "LABEL;QUOTED-PRINTABLE;WORK:");
		quotedfprint (fptr, vl);
		fprintf (fptr,"\n");
	    }
	}

        /* Other Address */
	boolean = 0;
	if ((pobox = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_POST_OFFICE_BOX)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((street = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_STREET)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((city = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_CITY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((state = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_STATE_OR_PROVINCE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((zip = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_POSTAL_CODE)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if ((country = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_OTHER_ADDRESS_COUNTRY)) != MAPI_UNDEFINED) {
	    boolean = 1;
	}
	if (boolean == 1) {
	    fprintf (fptr, "ADR;QUOTED-PRINTABLE;OTHER:");
	    if (pobox != MAPI_UNDEFINED) {
		quotedfprint (fptr, pobox);
	    }
	    fprintf (fptr, ";;");
	    if (street != MAPI_UNDEFINED) {
		quotedfprint (fptr, street);
	    }
	    fprintf (fptr, ";");
	    if (city != MAPI_UNDEFINED) {
		quotedfprint (fptr, city);
	    }
	    fprintf (fptr, ";");
	    if (state != MAPI_UNDEFINED) {
		quotedfprint (fptr, state);
	    }
	    fprintf (fptr, ";");
	    if (zip != MAPI_UNDEFINED) {
		quotedfprint (fptr, zip);
	    }
	    fprintf (fptr, ";");
	    if (country != MAPI_UNDEFINED) {
		quotedfprint (fptr, country);
	    }
	    fprintf (fptr,"\n");
	}

	fprintProperty (tnef, fptr, PT_UNICODE, PR_CALLBACK_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-CALLBACK:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_PRIMARY_TELEPHONE_NUMBER, "TEL;PREF:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_MOBILE_TELEPHONE_NUMBER, "TEL;CELL:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_RADIO_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-RADIO:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_CAR_TELEPHONE_NUMBER, "TEL;CAR:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_OTHER_TELEPHONE_NUMBER, "TEL;VOICE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_PAGER_TELEPHONE_NUMBER, "TEL;PAGER:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_TELEX_NUMBER, "TEL;X-EVOLUTION-TELEX:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_ISDN_NUMBER, "TEL;ISDN:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_HOME2_TELEPHONE_NUMBER, "TEL;HOME:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_TTYTDD_PHONE_NUMBER, "TEL;X-EVOLUTION-TTYTDD:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_HOME_TELEPHONE_NUMBER, "TEL;HOME;VOICE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_ASSISTANT_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-ASSISTANT:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_COMPANY_MAIN_PHONE_NUMBER, "TEL;WORK:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_BUSINESS_TELEPHONE_NUMBER, "TEL;WORK:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_BUSINESS2_TELEPHONE_NUMBER, "TEL;WORK;VOICE:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_PRIMARY_FAX_NUMBER, "TEL;PREF;FAX:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_BUSINESS_FAX_NUMBER, "TEL;WORK;FAX:%s\n");
	fprintProperty (tnef, fptr, PT_UNICODE, PR_HOME_FAX_NUMBER, "TEL;HOME;FAX:%s\n");

        /* Email addresses */
	if ((vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8083)) == MAPI_UNDEFINED) {
	    vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8084);
	}
	if (vl != MAPI_UNDEFINED) {
	    if (vl->size > 0)
		fprintf (fptr, "EMAIL:%s\n", vl->data);
	}
	if ((vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8093)) == MAPI_UNDEFINED) {
	    vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8094);
	}
	if (vl != MAPI_UNDEFINED) {
	    if (vl->size > 0)
		fprintf (fptr, "EMAIL:%s\n", vl->data);
	}
	if ((vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x80a3)) == MAPI_UNDEFINED) {
	    vl = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x80a4);
	}
	if (vl != MAPI_UNDEFINED) {
	    if (vl->size > 0)
		fprintf (fptr, "EMAIL:%s\n", vl->data);
	}

	fprintProperty (tnef, fptr, PT_UNICODE, PR_BUSINESS_HOME_PAGE, "URL:%s\n");
	fprintUserProp (tnef, fptr, PT_UNICODE, 0x80d8, "FBURL:%s\n");

        /* Birthday */
	if ((vl = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (PT_SYSTIME, PR_BIRTHDAY))) != MAPI_UNDEFINED) {
	    fprintf (fptr, "BDAY:");
	    MAPISysTimetoDTR ((guchar *) vl->data, &thedate);
	    fprintf (fptr, "%i-%02i-%02i\n", thedate.wYear, thedate.wMonth, thedate.wDay);
	}

        /* Anniversary */
	if ((vl = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (PT_SYSTIME, PR_WEDDING_ANNIVERSARY))) != MAPI_UNDEFINED) {
	    fprintf (fptr, "X-EVOLUTION-ANNIVERSARY:");
	    MAPISysTimetoDTR ((guchar *) vl->data, &thedate);
	    fprintf (fptr, "%i-%02i-%02i\n", thedate.wYear, thedate.wMonth, thedate.wDay);
	}
	fprintf (fptr, "END:VCARD\n");
	fclose (fptr);
    }
    g_free (ifilename);
}

static guchar getRruleCount (guchar a, guchar b) {
    return ((a << 8) | b);
}

static guchar getRruleMonthNum (guchar a, guchar b) {
    switch (a) {
        case 0x00:
            switch (b) {
                case 0x00:
                    /* Jan */
                    return (1);
                case 0xA3:
                    /* May */
                    return (5);
                case 0xAE:
                    /* Nov */
                    return (11);
            }
            break;
        case 0x60:
            switch (b) {
                case 0xAE:
                    /* Feb */
                    return (2);
                case 0x51:
                    /* Jun */
                    return (6);
            }
            break;
        case 0xE0:
            switch (b) {
                case 0x4B:
                    /* Mar */
                    return (3);
                case 0x56:
                    /* Sep */
                    return (9);
            }
            break;
        case 0x40:
            switch (b) {
                case 0xFA:
                    /* Apr */
                    return (4);
            }
            break;
        case 0x20:
            if (b == 0xFA) {
                /* Jul */
                return (7);
            }
            break;
        case 0x80:
            if (b == 0xA8) {
                /* Aug */
                return (8);
            }
            break;
        case 0xA0:
            if (b == 0xFF) {
                /* Oct */
                return (10);
            }
            break;
        case 0xC0:
            if (b == 0x56) {
                return (12);
            }
    }

    /*  Error */
    return (0);
}

static gchar * getRruleDayname (guchar a) {
    static gchar daystring[25];

    *daystring = 0;

    g_snprintf (daystring, sizeof (daystring), "%s%s%s%s%s%s%s",
	(a & 0x01) ? "SU," : "",
	(a & 0x02) ? "MO," : "",
	(a & 0x04) ? "TU," : "",
	(a & 0x08) ? "WE," : "",
	(a & 0x10) ? "TH," : "",
	(a & 0x20) ? "FR," : "",
	(a & 0x40) ? "SA," : "");

    if (*daystring) {
        daystring[strlen (daystring) - 1] = 0;
    }

    return (daystring);
}

static void printRrule (FILE *fptr, const guchar *recur_data, gint size, TNEFStruct *tnef)
{
    variableLength *filename;

    if (size < 0x1F) {
	return;
    }

    fprintf (fptr, "RRULE:FREQ=");

    if (recur_data[0x04] == 0x0A) {
	fprintf (fptr, "DAILY");

	if (recur_data[0x16] == 0x23 || recur_data[0x16] == 0x22 ||
		recur_data[0x16] == 0x21) {
	    if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
		    PROP_TAG (PT_I2, 0x0011))) != MAPI_UNDEFINED) {
		fprintf (fptr, ";INTERVAL=%d", *(filename->data));
	    }
	    if (recur_data[0x16] == 0x22 || recur_data[0x16] == 0x21) {
		fprintf (
			fptr, ";COUNT=%d",
			getRruleCount (recur_data[0x1B], recur_data[0x1A]));
	    }
	} else if (recur_data[0x16] == 0x3E) {
	    fprintf (fptr, ";BYDAY=MO,TU,WE,TH,FR");
	    if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
		fprintf (
			fptr, ";COUNT=%d",
			getRruleCount (recur_data[0x1F], recur_data[0x1E]));
	    }
	}
    } else if (recur_data[0x04] == 0x0B) {
	fprintf (
		fptr, "WEEKLY;INTERVAL=%d;BYDAY=%s",
		recur_data[0x0E], getRruleDayname (recur_data[0x16]));
	if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
	    fprintf (fptr, ";COUNT=%d",
		getRruleCount (recur_data[0x1F], recur_data[0x1E]));
	}
    } else if (recur_data[0x04] == 0x0C) {
	fprintf (fptr, "MONTHLY");
	if (recur_data[0x06] == 0x02) {
	    fprintf (fptr, ";INTERVAL=%d;BYMONTHDAY=%d", recur_data[0x0E],
		recur_data[0x16]);
	    if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
		fprintf (
			fptr, ";COUNT=%d", getRruleCount (recur_data[0x1F],
			recur_data[0x1E]));
	    }
	} else if (recur_data[0x06] == 0x03) {
	    fprintf (fptr, ";BYDAY=%s;BYSETPOS=%d;INTERVAL=%d",
		getRruleDayname (recur_data[0x16]),
		recur_data[0x1A] == 0x05 ? -1 : recur_data[0x1A],
		recur_data[0x0E]);
	    if (recur_data[0x1E] == 0x22 || recur_data[0x1E] == 0x21) {
		fprintf (
			fptr, ";COUNT=%d", getRruleCount (recur_data[0x23],
			recur_data[0x22]));
	    }
	}
    } else if (recur_data[0x04] == 0x0D) {
	fprintf (
		fptr, "YEARLY;BYMONTH=%d",
		getRruleMonthNum (recur_data[0x0A], recur_data[0x0B]));
	if (recur_data[0x06] == 0x02) {
	    fprintf (fptr, ";BYMONTHDAY=%d", recur_data[0x16]);
	} else if (recur_data[0x06] == 0x03) {
	    fprintf (fptr, ";BYDAY=%s;BYSETPOS=%d",
		getRruleDayname (recur_data[0x16]),
		recur_data[0x1A] == 0x05 ? -1 : recur_data[0x1A]);
	}
	if (recur_data[0x1E] == 0x22 || recur_data[0x1E] == 0x21) {
	    fprintf (fptr, ";COUNT=%d", getRruleCount (recur_data[0x23],
		recur_data[0x22]));
	}
    }
    fprintf (fptr, "\n");
}

static void saveVCalendar (TNEFStruct *tnef, const gchar *tmpdir) {
    gchar *ifilename;
    variableLength *filename;
    gchar *charptr, *charptr2;
    FILE *fptr;
    gint index;
    DWORD *dword_ptr;
    dtr thedate;

    ifilename = g_build_filename (tmpdir, "calendar.vcf", NULL);
    d (printf ("%s\n", ifilename);)

    if ((fptr = fopen (ifilename, "wb")) == NULL) {
            printf ("Error writing file to disk!");
    } else {
        fprintf (fptr, "BEGIN:VCALENDAR\n");
        if (tnef->messageClass[0] != 0) {
            charptr2 = tnef->messageClass;
            charptr = charptr2;
            while (*charptr != 0) {
                if (*charptr == '.') {
                    charptr2 = charptr;
                }
                charptr++;
            }
            if (strcmp (charptr2, ".MtgCncl") == 0) {
                fprintf (fptr, "METHOD:CANCEL\n");
            } else {
                fprintf (fptr, "METHOD:REQUEST\n");
            }
        } else {
            fprintf (fptr, "METHOD:REQUEST\n");
        }
        fprintf (fptr, "VERSION:2.0\n");
        fprintf (fptr, "BEGIN:VEVENT\n");

	/* UID
	 * After alot of comparisons, I'm reasonably sure this is totally
	 * wrong.  But it's not really necessary. */

	/* I think it only exists to connect future modification entries to
	 * this entry. so as long as it's incorrectly interpreted the same way
	 * every time, it should be ok :) */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_BINARY, 0x3))) == MAPI_UNDEFINED) {
            if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                            PROP_TAG (PT_BINARY, 0x23))) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename != NULL) {
            fprintf (fptr, "UID:");
            for (index = 0; index < filename->size; index++) {
                fprintf (fptr,"%02X", (guchar) filename->data[index]);
            }
            fprintf (fptr,"\n");
        }

        /* Sequence */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_LONG, 0x8201))) != MAPI_UNDEFINED) {
            dword_ptr = (DWORD *) filename->data;
            fprintf (fptr, "SEQUENCE:%i\n", (gint) *dword_ptr);
        }
        if ((filename = MAPIFindProperty (&(tnef->MapiProperties),
                        PROP_TAG (PT_BINARY, PR_SENDER_SEARCH_KEY)))
                != MAPI_UNDEFINED) {
            charptr = (gchar *) filename->data;
            charptr2 = strstr (charptr, ":");
            if (charptr2 == NULL)
                charptr2 = charptr;
            else
                charptr2++;
            fprintf (fptr, "ORGANIZER;CN=\"%s\":mailto:%s\n",
                    charptr2, charptr2);
        }

        /* Required Attendees */
        if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x823b)) != MAPI_UNDEFINED) {
	    /* We have a list of required participants, so
	       write them out. */
            if (filename->size > 1) {
                charptr = (gchar *) filename->data - 1;
                while (charptr != NULL) {
                    charptr++;
                    charptr2 = strstr (charptr, ";");
                    if (charptr2 != NULL) {
                        *charptr2 = 0;
                    }
                    while (*charptr == ' ')
                        charptr++;
                    fprintf (fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                    fprintf (fptr, "ROLE=REQ-PARTICIPANT;RSVP=TRUE;");
                    fprintf (fptr, "CN=\"%s\":mailto:%s\n",
                                charptr, charptr);
                    charptr = charptr2;
                }
            }
            /* Optional attendees */
            if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x823c)) != MAPI_UNDEFINED) {
                    /* The list of optional participants */
                if (filename->size > 1) {
                    charptr = (gchar *) filename->data - 1;
                    while (charptr != NULL) {
                        charptr++;
                        charptr2 = strstr (charptr, ";");
                        if (charptr2 != NULL) {
                            *charptr2 = 0;
                        }
                        while (*charptr == ' ')
                            charptr++;
                        fprintf (fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                        fprintf (fptr, "ROLE=OPT-PARTICIPANT;RSVP=TRUE;");
                        fprintf (fptr, "CN=\"%s\":mailto:%s\n",
                                charptr, charptr);
                        charptr = charptr2;
                    }
                }
            }
        } else if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8238)) != MAPI_UNDEFINED) {
            if (filename->size > 1) {
                charptr = (gchar *) filename->data - 1;
                while (charptr != NULL) {
                    charptr++;
                    charptr2 = strstr (charptr, ";");
                    if (charptr2 != NULL) {
                        *charptr2 = 0;
                    }
                    while (*charptr == ' ')
                        charptr++;
                    fprintf (fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                    fprintf (fptr, "ROLE=REQ-PARTICIPANT;RSVP=TRUE;");
                    fprintf (fptr, "CN=\"%s\":mailto:%s\n",
                                charptr, charptr);
                    charptr = charptr2;
                }
            }

        }
        /* Summary */
        filename = NULL;
        if ((filename = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_CONVERSATION_TOPIC)) != MAPI_UNDEFINED) {
            filename->size = strlen ((const gchar *) filename->data);
            fprintf (fptr, "SUMMARY:");
            cstylefprint (fptr, filename);
            fprintf (fptr, "\n");
        }

        /* Description */
        if ((filename = MAPIFindProperty (&(tnef->MapiProperties),
                                PROP_TAG (PT_BINARY, PR_RTF_COMPRESSED)))
                != MAPI_UNDEFINED) {
            variableLength buf;
            if ((buf.data = DecompressRTF (filename, &buf.size)) != NULL) {
		gchar *base64;

		base64 = g_base64_encode (buf.data, buf.size);
		if (base64) {
			guint ii, len = strlen (base64);

			fprintf (fptr, "ATTACH;VALUE=BINARY;FILENAME=description.rtf;ENCODING=BASE64:\n");
			for (ii = 0; ii < len; ii += 76) {
				fprintf (fptr, " %.*s\n", (gint) (MIN (76, len - ii)), base64 + ii);
			}

			g_free (base64);
		}
                fprintf (fptr, "DESCRIPTION:");
                printRtf (fptr, &buf);
                free (buf.data);
            }

        }

        /* Location */
        filename = NULL;
        if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x0002)) == MAPI_UNDEFINED) {
            if ((filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8208)) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename != NULL) {
            fprintf (fptr,"LOCATION: %s\n", filename->data);
        }
        /* Date Start */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_SYSTIME, 0x820d))) == MAPI_UNDEFINED) {
            if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                            PROP_TAG (PT_SYSTIME, 0x8516))) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename != NULL) {
            fprintf (fptr, "DTSTART:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        /* Date End */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_SYSTIME, 0x820e))) == MAPI_UNDEFINED) {
            if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                            PROP_TAG (PT_SYSTIME, 0x8517))) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename != NULL) {
            fprintf (fptr, "DTEND:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        /* Date Stamp */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_SYSTIME, 0x8202))) != MAPI_UNDEFINED) {
            fprintf (fptr, "CREATED:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        /* Class */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_BOOLEAN, 0x8506))) != MAPI_UNDEFINED) {
            dword_ptr = (DWORD *) filename->data;
            fprintf (fptr, "CLASS:");
            if (*dword_ptr == 1) {
                fprintf (fptr,"PRIVATE\n");
            } else {
                fprintf (fptr,"PUBLIC\n");
            }
        }
        /* Recurrence */
        filename = NULL;
        if ((filename = MAPIFindUserProp (&(tnef->MapiProperties),
                        PROP_TAG (PT_BINARY, 0x8216))) != MAPI_UNDEFINED) {
            printRrule (fptr, filename->data, filename->size, tnef);
        }

        /* Wrap it up */
        fprintf (fptr, "END:VEVENT\n");
        fprintf (fptr, "END:VCALENDAR\n");
        fclose (fptr);
    }
    g_free (ifilename);
}

static void saveVTask (TNEFStruct *tnef, const gchar *tmpdir) {
    variableLength *vl;
    variableLength *filename;
    gint index;
    gchar *ifilename;
    gchar *absfilename, *file;
    gchar *charptr, *charptr2;
    dtr thedate;
    FILE *fptr;
    DWORD *dword_ptr;

    vl = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_CONVERSATION_TOPIC);

    if (vl == MAPI_UNDEFINED) {
        return;
    }

    index = strlen ((const gchar *) vl->data);
    while (vl->data[index] == ' ')
            vl->data[index--] = 0;

    file = sanitize_filename ((const gchar *) vl->data);
    if (!file)
	return;
    absfilename = g_strconcat (file, ".vcf", NULL);
    ifilename = g_build_filename (tmpdir, absfilename, NULL);
    g_free (file);
    g_free (absfilename);

    printf ("%s\n", ifilename);

    if ((fptr = fopen (ifilename, "wb")) == NULL) {
            printf ("Error writing file to disk!");
    } else {
        fprintf (fptr, "BEGIN:VCALENDAR\n");
        fprintf (fptr, "VERSION:2.0\n");
        fprintf (fptr, "METHOD:PUBLISH\n");
        filename = NULL;

        fprintf (fptr, "BEGIN:VTODO\n");
        if (tnef->messageID[0] != 0) {
            fprintf (fptr,"UID:%s\n", tnef->messageID);
        }
        filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x8122);
        if (filename != MAPI_UNDEFINED) {
            fprintf (fptr, "ORGANIZER:%s\n", filename->data);
        }

        if ((filename = e_tnef_get_string_prop (&(tnef->MapiProperties), PR_DISPLAY_TO)) == MAPI_UNDEFINED) {
            filename = e_tnef_get_string_user_prop (&(tnef->MapiProperties), 0x811f);
        }
        if ((filename != MAPI_UNDEFINED) && (filename->size > 1)) {
            charptr = (gchar *) filename->data - 1;
            while (charptr != NULL) {
                charptr++;
                charptr2 = strstr (charptr, ";");
                if (charptr2 != NULL) {
                    *charptr2 = 0;
                }
                while (*charptr == ' ')
                    charptr++;
                fprintf (fptr, "ATTENDEE;CN=%s;ROLE=REQ-PARTICIPANT:%s\n", charptr, charptr);
                charptr = charptr2;
            }
        }

        if (tnef->subject.size > 0) {
            fprintf (fptr,"SUMMARY:");
            cstylefprint (fptr,&(tnef->subject));
            fprintf (fptr,"\n");
        }

        if (tnef->body.size > 0) {
            fprintf (fptr,"DESCRIPTION:");
            cstylefprint (fptr,&(tnef->body));
            fprintf (fptr,"\n");
        }

        filename = MAPIFindProperty (&(tnef->MapiProperties), \
                    PROP_TAG (PT_SYSTIME, PR_CREATION_TIME));
        if (filename != MAPI_UNDEFINED) {
            fprintf (fptr, "DTSTAMP:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }

        filename = MAPIFindUserProp (&(tnef->MapiProperties), \
                    PROP_TAG (PT_SYSTIME, 0x8517));
        if (filename != MAPI_UNDEFINED) {
            fprintf (fptr, "DUE:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        filename = MAPIFindProperty (&(tnef->MapiProperties), \
                    PROP_TAG (PT_SYSTIME, PR_LAST_MODIFICATION_TIME));
        if (filename != MAPI_UNDEFINED) {
            fprintf (fptr, "LAST-MODIFIED:");
            MAPISysTimetoDTR ((guchar *) filename->data, &thedate);
            fprintf (fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        /* Class */
        filename = MAPIFindUserProp (&(tnef->MapiProperties), \
                        PROP_TAG (PT_BOOLEAN, 0x8506));
        if (filename != MAPI_UNDEFINED) {
            dword_ptr = (DWORD *) filename->data;
            fprintf (fptr, "CLASS:");
            if (*dword_ptr == 1) {
                fprintf (fptr,"PRIVATE\n");
            } else {
                fprintf (fptr,"PUBLIC\n");
            }
        }
        fprintf (fptr, "END:VTODO\n");
        fprintf (fptr, "END:VCALENDAR\n");
        fclose (fptr);
    }
    g_free (ifilename);
}

static void fprintProperty (TNEFStruct *tnef, FILE *fptr, DWORD proptype, DWORD propid, const gchar text[]) {
    variableLength *vl;
    vl = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (proptype, propid));
    if (vl == MAPI_UNDEFINED && proptype == PT_UNICODE)
        vl = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG (PT_STRING8, propid));
    if (vl != MAPI_UNDEFINED) {
        if (vl->size > 0) {
            if ((vl->size == 1) && (vl->data[0] == 0)) {
            } else {
                fprintf (fptr, text, vl->data);
            }
	}
    }
}

static void fprintUserProp (TNEFStruct *tnef, FILE *fptr, DWORD proptype, DWORD propid, const gchar text[]) {
    variableLength *vl;
    vl = MAPIFindUserProp (&(tnef->MapiProperties), PROP_TAG (proptype, propid));
    if (vl == MAPI_UNDEFINED && proptype == PT_UNICODE)
        vl = MAPIFindUserProp (&(tnef->MapiProperties), PROP_TAG (PT_STRING8, propid));
    if (vl != MAPI_UNDEFINED) {
        if (vl->size > 0) {
            if ((vl->size == 1) && (vl->data[0] == 0)) {
            } else {
                fprintf (fptr, text, vl->data);
            }
	}
    }
}

static void quotedfprint (FILE *fptr, variableLength *vl) {
    gint index;

    for (index = 0; index < vl->size - 1; index++) {
        if (vl->data[index] == '\n') {
            fprintf (fptr, "=0A");
        } else if (vl->data[index] == '\r') {
        } else {
            fprintf (fptr, "%c", vl->data[index]);
        }
    }
}

static void cstylefprint (FILE *fptr, variableLength *vl) {
    gint index;

    for (index = 0; index < vl->size - 1; index++) {
        if (vl->data[index] == '\n') {
            fprintf (fptr, "\\n");
        } else if (vl->data[index] == '\r') {
            /* Print nothing. */
        } else if (vl->data[index] == ';') {
            fprintf (fptr, "\\;");
        } else if (vl->data[index] == ',') {
            fprintf (fptr, "\\,");
        } else if (vl->data[index] == '\\') {
            fprintf (fptr, "\\");
        } else {
            fprintf (fptr, "%c", vl->data[index]);
        }
    }
}

static void printRtf (FILE *fptr, variableLength *vl) {
    gint index;
    guchar *byte;
    gint brace_ct;
    gint key;

    key = 0;
    brace_ct = 0;

    for (index = 0, byte = vl->data; index < vl->size; index++, byte++) {
        if (*byte == '}') {
            brace_ct--;
            key = 0;
            continue;
        }
        if (*byte == '{') {
            brace_ct++;
            continue;
        }
        if (*byte == '\\') {
            key = 1;
        }
        if (isspace (*byte)) {
            key = 0;
        }
        if ((brace_ct == 1) && (key == 0)) {
            if (*byte == '\n') {
                fprintf (fptr, "\\n");
            } else if (*byte == '\r') {
		/* Print nothing. */
            } else if (*byte == ';') {
                fprintf (fptr, "\\;");
            } else if (*byte == ',') {
                fprintf (fptr, "\\,");
            } else if (*byte == '\\') {
                fprintf (fptr, "\\");
            } else {
                fprintf (fptr, "%c", *byte);
            }
        }
    }
    fprintf (fptr, "\n");
}
