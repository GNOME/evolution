/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-multipart.c : Abstract class for a multipart */


/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "gmime-content-field.h"
#include "gmime-utils.h"
#include "camel-stream-mem.h"
#include "camel-seekable-substream.h"
#include "camel-mime-body-part.h"
#include "camel-multipart.h"

#define d(x)

static void                  add_part          (CamelMultipart *multipart,
						   CamelMimeBodyPart *part);
static void                  add_part_at       (CamelMultipart *multipart,
						CamelMimeBodyPart *part,
						guint index);
static void                  remove_part       (CamelMultipart *multipart,
						CamelMimeBodyPart *part);
static CamelMimeBodyPart *   remove_part_at    (CamelMultipart *multipart,
						guint index);
static CamelMimeBodyPart *   get_part          (CamelMultipart *multipart,
						guint index);
static guint                 get_number        (CamelMultipart *multipart);
static void                  set_parent        (CamelMultipart *multipart,
						CamelMimePart *parent);
static CamelMimePart *       get_parent        (CamelMultipart *multipart);
static void                  set_boundary      (CamelMultipart *multipart,
						gchar *boundary);
static const gchar *         get_boundary      (CamelMultipart *multipart);
static void                  write_to_stream   (CamelDataWrapper *data_wrapper,
						CamelStream *stream);
static void                  set_input_stream  (CamelDataWrapper *data_wrapper,
						CamelStream *stream);
static void                  finalize          (GtkObject *object);
static void construct_from_parser(CamelDataWrapper *dw, CamelMimeParser *mp);


static CamelDataWrapperClass *parent_class = NULL;



/* Returns the class for a CamelMultipart */
#define CMP_CLASS(so) CAMEL_MULTIPART_CLASS (GTK_OBJECT(so)->klass)

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)


static void
camel_multipart_class_init (CamelMultipartClass *camel_multipart_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_multipart_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_multipart_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());

	/* virtual method definition */
	camel_multipart_class->add_part = add_part;
	camel_multipart_class->add_part_at = add_part_at;
	camel_multipart_class->remove_part = remove_part;
	camel_multipart_class->remove_part_at = remove_part_at;
	camel_multipart_class->get_part = get_part;
	camel_multipart_class->get_number = get_number;
	camel_multipart_class->set_parent = set_parent;
	camel_multipart_class->get_parent = get_parent;
	camel_multipart_class->set_boundary = set_boundary;
	camel_multipart_class->get_boundary = get_boundary;

	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->set_input_stream = set_input_stream;

	camel_data_wrapper_class->construct_from_parser = construct_from_parser;	

	gtk_object_class->finalize = finalize;
}

static void
camel_multipart_init (gpointer object, gpointer klass)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (object);

	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
					  "multipart/mixed");
	camel_multipart_set_boundary (multipart, "=-=-=-=");
	multipart->preface = NULL;
	multipart->postface = NULL;
}


GtkType
camel_multipart_get_type (void)
{
	static GtkType camel_multipart_type = 0;

	if (!camel_multipart_type) {
		GtkTypeInfo camel_multipart_info =
		{
			"CamelMultipart",
			sizeof (CamelMultipart),
			sizeof (CamelMultipartClass),
			(GtkClassInitFunc) camel_multipart_class_init,
			(GtkObjectInitFunc) camel_multipart_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_multipart_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_multipart_info);
	}

	return camel_multipart_type;
}

static void
unref_part (gpointer data, gpointer user_data)
{
	GtkObject *body_part = GTK_OBJECT (data);

	gtk_object_unref (body_part);
}

static void
finalize (GtkObject *object)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (object);

	if (multipart->parent)
		gtk_object_unref (GTK_OBJECT (multipart->parent));

	g_list_foreach (multipart->parts, unref_part, NULL);

	if (multipart->boundary)
		g_free (multipart->boundary);
	if (multipart->preface)
		g_free (multipart->preface);
	if (multipart->postface)
		g_free (multipart->postface);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}



/**
 * camel_multipart_new:
 *
 * Create a new CamelMultipart object.
 *
 * Return value: a new CamelMultipart
 **/
CamelMultipart *
camel_multipart_new (void)
{
	CamelMultipart *multipart;

	multipart = (CamelMultipart *)gtk_type_new (CAMEL_MULTIPART_TYPE);
	multipart->preface = NULL;
	multipart->postface = NULL;

	return multipart;
}


static void
add_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	multipart->parts = g_list_append (multipart->parts, part);
	gtk_object_ref (GTK_OBJECT (part));
}

/**
 * camel_multipart_add_part:
 * @multipart: a CamelMultipart
 * @part: the body part to add
 *
 * Appends the body part to the multipart object.
 **/
void
camel_multipart_add_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	g_return_if_fail (CAMEL_IS_MIME_BODY_PART (part));

	CMP_CLASS (multipart)->add_part (multipart, part);
}


static void
add_part_at (CamelMultipart *multipart, CamelMimeBodyPart *part, guint index)
{
	multipart->parts = g_list_insert (multipart->parts, part, index);
	gtk_object_ref (GTK_OBJECT (part));
}

/**
 * camel_multipart_add_part_at:
 * @multipart: a CamelMultipart
 * @part: the body part to add
 * @index: index to add the multipart at
 *
 * Adds the body part to the multipart object after the @index'th
 * element. If @index is greater than the number of parts, it is
 * equivalent to camel_multipart_add_part().
 **/
void
camel_multipart_add_part_at (CamelMultipart *multipart,
			     CamelMimeBodyPart *part, guint index)
{
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	g_return_if_fail (CAMEL_IS_MIME_BODY_PART (part));

	CMP_CLASS (multipart)->add_part_at (multipart, part, index);
}


static void
remove_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	if (!multipart->parts)
		return;
	multipart->parts = g_list_remove (multipart->parts, part);
	gtk_object_unref (GTK_OBJECT (part));
}

/**
 * camel_multipart_remove_part:
 * @multipart: a CamelMultipart
 * @part: the part to remove
 *
 * Removes @part from @multipart.
 **/
void
camel_multipart_remove_part (CamelMultipart *multipart,
			     CamelMimeBodyPart *part)
{
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	g_return_if_fail (CAMEL_IS_MIME_BODY_PART (part));

	CMP_CLASS (multipart)->remove_part (multipart, part);
}


static CamelMimeBodyPart *
remove_part_at (CamelMultipart *multipart, guint index)
{
	GList *parts_list;
	GList *part_to_remove;
	CamelMimeBodyPart *removed_body_part;

	if (!(multipart->parts))
		return NULL;

	parts_list = multipart->parts;
	part_to_remove = g_list_nth (parts_list, index);
	if (!part_to_remove) {
		g_warning ("CamelMultipart::remove_part_at: "
			   "part to remove is NULL\n");
		return NULL;
	}
	removed_body_part = CAMEL_MIME_BODY_PART (part_to_remove->data);

	multipart->parts = g_list_remove_link (parts_list, part_to_remove);
	if (part_to_remove->data)
		gtk_object_unref (GTK_OBJECT (part_to_remove->data));
	g_list_free_1 (part_to_remove);

	return removed_body_part;
}

/**
 * camel_multipart_remove_part_at:
 * @multipart: a CamelMultipart
 * @index: a zero-based index indicating the part to remove
 *
 * Remove the indicated part from the multipart object.
 *
 * Return value: the removed part. Note that it is gtk_object_unref()ed
 * before being returned, which may cause it to be destroyed.
 **/
CamelMimeBodyPart *
camel_multipart_remove_part_at (CamelMultipart *multipart, guint index)
{
	g_return_val_if_fail (CAMEL_IS_MULTIPART (multipart), NULL);

	return CMP_CLASS (multipart)->remove_part_at (multipart, index);
}


static CamelMimeBodyPart *
get_part (CamelMultipart *multipart, guint index)
{
	GList *part;

	if (!(multipart->parts))
		return NULL;

	part = g_list_nth (multipart->parts, index);
	if (part)
		return CAMEL_MIME_BODY_PART (part->data);
	else
		return NULL;
}

/**
 * camel_multipart_get_part:
 * @multipart: a CamelMultipart
 * @index: a zero-based index indicating the part to get
 *
 * Return value: the indicated subpart, or %NULL
 **/
CamelMimeBodyPart *
camel_multipart_get_part (CamelMultipart *multipart, guint index)
{
	g_return_val_if_fail (CAMEL_IS_MULTIPART (multipart), NULL);

	return CMP_CLASS (multipart)->get_part (multipart, index);
}


static guint
get_number (CamelMultipart *multipart)
{
	return g_list_length (multipart->parts);
}

/**
 * camel_multipart_get_number:
 * @multipart: a CamelMultipart
 *
 * Return value: the number of subparts in @multipart
 **/
guint
camel_multipart_get_number (CamelMultipart *multipart)
{
	g_return_val_if_fail (CAMEL_IS_MULTIPART (multipart), 0);

	return CMP_CLASS (multipart)->get_number (multipart);
}


static void
set_parent (CamelMultipart *multipart, CamelMimePart *parent)
{
	multipart->parent = parent;
	if (parent)
		gtk_object_ref (GTK_OBJECT (parent));
}

/**
 * camel_multipart_set_parent:
 * @multipart: a CamelMultipart
 * @parent: the CamelMimePart that is @multipart's parent
 *
 * Sets the parent of @multipart.
 **/
void
camel_multipart_set_parent (CamelMultipart *multipart, CamelMimePart *parent)
{
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	g_return_if_fail (CAMEL_IS_MIME_PART (parent));

	CMP_CLASS (multipart)->set_parent (multipart, parent);
}


static CamelMimePart *
get_parent (CamelMultipart *multipart)
{
	return multipart->parent;
}

/**
 * camel_multipart_get_parent:
 * @multipart: a CamelMultipart
 *
 * Return value: @multipart's parent part
 **/
CamelMimePart *
camel_multipart_get_parent (CamelMultipart *multipart)
{
	g_return_val_if_fail (CAMEL_IS_MULTIPART (multipart), NULL);

	return CMP_CLASS (multipart)->get_parent (multipart);
}



static void
set_boundary (CamelMultipart *multipart, gchar *boundary)
{
	CamelDataWrapper *cdw = CAMEL_DATA_WRAPPER (multipart);

	g_return_if_fail (cdw->mime_type != NULL);

	gmime_content_field_set_parameter (cdw->mime_type, "boundary",
					   boundary);
}

/**
 * camel_multipart_set_boundary:
 * @multipart: a CamelMultipart
 * @boundary: the message boundary
 *
 * Sets the message boundary for @multipart to @boundary. This should
 * be a string which does not occur anywhere in any of @multipart's
 * subparts.
 **/
void
camel_multipart_set_boundary (CamelMultipart *multipart, gchar *boundary)
{
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	g_return_if_fail (boundary != NULL);

	CMP_CLASS (multipart)->set_boundary (multipart, boundary);
}


static const gchar *
get_boundary (CamelMultipart *multipart)
{
	CamelDataWrapper *cdw = CAMEL_DATA_WRAPPER (multipart);

	g_return_val_if_fail (cdw->mime_type != NULL, NULL);
	return gmime_content_field_get_parameter (cdw->mime_type, "boundary");
}

/**
 * camel_multipart_get_boundary:
 * @multipart: a CamelMultipart
 *
 * Return value: @multipart's message boundary
 **/
const gchar *
camel_multipart_get_boundary (CamelMultipart *multipart)
{
	return CMP_CLASS (multipart)->get_boundary (multipart);
}


struct print_part_user_data {
	CamelStream *stream;
	const gchar *boundary;
};

static void
print_part (gpointer data, gpointer user_data)
{
	CamelMimeBodyPart *body_part = CAMEL_MIME_BODY_PART (data);
	struct print_part_user_data *ud = user_data;

	if (ud->boundary)
		camel_stream_write_strings (ud->stream, "\n--",
					    ud->boundary, "\n", NULL);
	else camel_stream_write_strings (ud->stream, "\n--\n", NULL);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (body_part),
					    ud->stream);
}

static void
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (data_wrapper);
	struct print_part_user_data ud;
	const gchar *boundary;

	/* get the bundary text */
	boundary = camel_multipart_get_boundary (multipart);

	/* we cannot write a multipart without a boundary string */
	g_return_if_fail (boundary);

	/*
	 * write the preface text (usually something like
	 *   "This is a mime message, if you see this, then
	 *    your mail client probably doesn't support ...."
	 */
	if (multipart->preface)
		camel_stream_write_strings (stream, multipart->preface, NULL);

	/*
	 * Now, write all the parts, separated by the boundary
	 * delimiter
	 */
	ud.boundary = boundary;
	ud.stream = stream;
	if (boundary && (boundary[0] == '\0'))
		g_warning ("Multipart boundary is zero length\n");
	g_list_foreach (multipart->parts, print_part, (gpointer)&ud);

	/* write the terminating boudary delimiter */
	camel_stream_write_strings (stream, "\n--", boundary, "--\n", NULL);

	/* and finally the postface */
	if (multipart->postface)
		camel_stream_write_strings (stream, multipart->postface, NULL);
}


/**
 * separate_part: separate one part in a multipart environement.
 * @stream: the stream to read the lines from.
 * @normal_boundary: end of part bundary.
 * @end_boundary: end of multipart boundary.
 * @end_position: end position of the mime part
 *
 * This routine is a bit special: RFC 2046 says that, in a multipart
 * environment, the last CRLF before a boundary belongs to the boundary.
 * Thus, if there is no blank line before the boundary, the last CRLF
 * of the last line of the part is removed.
 *
 * Return value: %TRUE if the last boundary element has been found or
 * if no more data was available from the stream, %FALSE otherwise
 **/
static gboolean
separate_part (CamelStream *stream, gchar *normal_boundary,
	       gchar *end_boundary, guint32 *end_position)
{
	gchar *new_line = NULL;
	gboolean end_of_part = FALSE;
	gboolean last_part = FALSE;
	guint32 last_position;

	/* Note for future enhancements */
	/* RFC 2046 specifies that when parsing the content of a
	 * multipart element, the program should not assume it will
	 * find the last boundary, and in particular, if the message
	 * is damaged during transport, the parsing should still be
	 * OK.
	 */

	last_position = camel_seekable_stream_get_current_position (
		CAMEL_SEEKABLE_STREAM (stream));
	new_line = gmime_read_line_from_stream (stream);

	while (new_line && !end_of_part && !last_part) {
		end_of_part = (strcmp (new_line, normal_boundary) == 0);
		last_part   = (strcmp (new_line, end_boundary) == 0);
		if (!end_of_part && !last_part) {
			g_free (new_line);

			last_position =
				camel_seekable_stream_get_current_position (
					CAMEL_SEEKABLE_STREAM (stream));

			new_line = gmime_read_line_from_stream (stream);
		}
	}

	if (new_line)
		g_free (new_line);
	else
		last_part = TRUE;

	*end_position = last_position;

	return last_part;
}


static void
set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (data_wrapper);
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	const gchar *boundary;
	gchar *real_boundary_line;
	gchar *end_boundary_line;
	gboolean end_of_multipart;
	CamelMimeBodyPart *body_part;
	guint32 part_begining, part_end;
	CamelSeekableSubstream *body_part_input_stream;
	guint32 saved_stream_pos;

	/* Call parent class implementation. */
	parent_class->set_input_stream (data_wrapper, stream);

	boundary = camel_multipart_get_boundary (multipart);
	g_return_if_fail (boundary);

	real_boundary_line = g_strdup_printf ("--%s", boundary);
	end_boundary_line  = g_strdup_printf ("--%s--", boundary);

	/* Read the prefix, if any. */
	end_of_multipart = separate_part (stream, real_boundary_line,
					  end_boundary_line, &part_end);
	if (multipart->preface)
		g_free (multipart->preface);

	/* Read all the real parts. */
	while (!end_of_multipart) {
		/* Determine the position of the begining of the part. */
		part_begining = camel_seekable_stream_get_current_position (seekable_stream);

		body_part = camel_mime_body_part_new ();

		end_of_multipart = separate_part (stream, real_boundary_line,
						  end_boundary_line,
						  &part_end);
		body_part_input_stream = CAMEL_SEEKABLE_SUBSTREAM (
			camel_seekable_substream_new_with_seekable_stream_and_bounds (seekable_stream,
										      part_begining,
										      part_end));

		/* The seekable substream may change the position of
		 * the stream so we must save it before calling
		 * set_input_stream.
		 */
		saved_stream_pos = camel_seekable_stream_get_current_position (seekable_stream);
		camel_data_wrapper_set_input_stream (CAMEL_DATA_WRAPPER (body_part), 
						     CAMEL_STREAM (body_part_input_stream));
		
		/* restore the stream position */
		camel_seekable_stream_seek (seekable_stream, saved_stream_pos, CAMEL_STREAM_SET);
		
		/* add the body part to the multipart object */
		camel_multipart_add_part (multipart, body_part);		
	}
	
	/* g_string_assign (new_part, ""); */
	/* my_localize_part (new_part, stream, real_boundary_line, end_boundary_line); */
	
	if (multipart->postface) g_free (multipart->postface);
	/* if ( (new_part->str)[0] != '\0') multipart->postface = g_strdup (new_part->str); */
	
	/* g_string_free (new_part, TRUE); */
	
	g_free (real_boundary_line);
	g_free (end_boundary_line);
}

/* multi_part */
static void
construct_from_parser(CamelDataWrapper *dw, CamelMimeParser *mp)
{
	CamelDataWrapper *bodypart;
	char *buf;
	int len;

	d(printf("constructing multipart\n"));

	/* get/set boundary? */

	while (camel_mime_parser_step(mp, &buf, &len) != HSCAN_MULTIPART_END) {
		camel_mime_parser_unstep(mp);
		bodypart = (CamelDataWrapper *)camel_mime_body_part_new();
		camel_data_wrapper_construct_from_parser(bodypart, mp);
		camel_multipart_add_part((CamelMultipart *)dw, (CamelMimeBodyPart *)bodypart);
	}
}
