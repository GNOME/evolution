/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-multipart.c : Abstract class for a multipart */


/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-log.h"
#include "gmime-content-field.h"
#include "gmime-utils.h"
#include "camel-stream-mem.h"
#include "camel-seekable-substream.h" 

#include "camel-multipart.h"


static void                  _add_part          (CamelMultipart *multipart, 
						 CamelMimeBodyPart *part);
static void                  _add_part_at       (CamelMultipart *multipart, 
						 CamelMimeBodyPart *part, 
						 guint index);
static void                  _remove_part       (CamelMultipart *multipart, 
						 CamelMimeBodyPart *part);
static CamelMimeBodyPart *   _remove_part_at    (CamelMultipart *multipart, 
						 guint index);
static CamelMimeBodyPart *   _get_part          (CamelMultipart *multipart, 
						 guint index);
static guint                 _get_number        (CamelMultipart *multipart);
static void                  _set_parent        (CamelMultipart *multipart, 
						 CamelMimePart *parent);
static CamelMimePart *       _get_parent        (CamelMultipart *multipart);
static void                  _set_boundary      (CamelMultipart *multipart, 
						 gchar *boundary);
static const gchar *         _get_boundary      (CamelMultipart *multipart);
static void                  _write_to_stream   (CamelDataWrapper *data_wrapper, 
						 CamelStream *stream);
static void                  _set_input_stream  (CamelDataWrapper *data_wrapper, 
						 CamelStream *stream);

static void                  _finalize          (GtkObject *object);

static CamelDataWrapperClass *parent_class=NULL;



/* Returns the class for a CamelMultipart */
#define CMP_CLASS(so) CAMEL_MULTIPART_CLASS (GTK_OBJECT(so)->klass)

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)


static void
camel_multipart_class_init (CamelMultipartClass *camel_multipart_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_multipart_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_multipart_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
		
	/* virtual method definition */
	camel_multipart_class->add_part = _add_part;
	camel_multipart_class->add_part_at = _add_part_at;
	camel_multipart_class->remove_part = _remove_part;
	camel_multipart_class->remove_part_at = _remove_part_at;
	camel_multipart_class->get_part = _get_part;
	camel_multipart_class->get_number = _get_number;
	camel_multipart_class->set_parent = _set_parent;
	camel_multipart_class->get_parent = _get_parent;
	camel_multipart_class->set_boundary = _set_boundary;
	camel_multipart_class->get_boundary = _get_boundary;

	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_data_wrapper_class->set_input_stream = _set_input_stream;

	gtk_object_class->finalize = _finalize;
}

static void
camel_multipart_init (gpointer   object,  gpointer   klass)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (object);
	camel_data_wrapper_set_mime_type ( CAMEL_DATA_WRAPPER (multipart), "multipart");
	camel_multipart_set_boundary (multipart, "__camel_boundary__");
	multipart->preface = NULL;
	multipart->postface = NULL;
	
}




GtkType
camel_multipart_get_type (void)
{
	static GtkType camel_multipart_type = 0;
	
	if (!camel_multipart_type)	{
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
_unref_part (gpointer data, gpointer user_data)
{
	GtkObject *body_part = GTK_OBJECT (data);
	
	gtk_object_unref (body_part);
}

static void           
_finalize (GtkObject *object)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMultipart::finalize\n");

	if (multipart->parent) gtk_object_unref (GTK_OBJECT (multipart->parent));

	g_list_foreach (multipart->parts, _unref_part, NULL);
	
	if (multipart->boundary) g_free (multipart->boundary);
	if (multipart->preface)  g_free (multipart->preface);
	if (multipart->postface) g_free (multipart->postface);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMultipart::finalize\n");
}



CamelMultipart *           
camel_multipart_new (void)
{
	CamelMultipart *multipart;
	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Entering new()\n");
	
	multipart = (CamelMultipart *)gtk_type_new (CAMEL_MULTIPART_TYPE);
	multipart->preface = NULL;
	multipart->postface = NULL;


	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Leaving new()\n");
	return multipart;
}


static void
_add_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	multipart->parts = g_list_append (multipart->parts, part);
	if (part) gtk_object_ref (GTK_OBJECT (part));
}

void 
camel_multipart_add_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	CMP_CLASS (multipart)->add_part (multipart, part);	
} 


static void
_add_part_at (CamelMultipart *multipart, CamelMimeBodyPart *part, guint index)
{
	multipart->parts = g_list_insert (multipart->parts, part, index);
	if (part) gtk_object_ref (GTK_OBJECT (part));
}

void 
camel_multipart_add_part_at (CamelMultipart *multipart, CamelMimeBodyPart *part, guint index)
{
	CMP_CLASS (multipart)->add_part_at (multipart, part, index);
}

static void
_remove_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	if (!multipart->parts) {
		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::remove_part part list id void\n");
		return;
	}
	multipart->parts = g_list_remove (multipart->parts, part);
	if (part) gtk_object_unref (GTK_OBJECT (part));
}

void 
camel_multipart_remove_part (CamelMultipart *multipart, CamelMimeBodyPart *part)
{
	CMP_CLASS (multipart)->remove_part (multipart, part);
}


static CamelMimeBodyPart *
_remove_part_at (CamelMultipart *multipart, guint index)
{
	GList *parts_list;
	GList *part_to_remove;
	CamelMimeBodyPart *removed_body_part;

	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Entering remove_part_at\n");
	CAMEL_LOG_TRACE ("CamelMultipart::remove_part_at : Removing part number %d\n", index);

	if (!(multipart->parts)) {
		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::remove_part_at part list is void \n");
		return NULL;
	}

	parts_list = multipart->parts;
	part_to_remove = g_list_nth (parts_list, index);
	if (!part_to_remove) {
		CAMEL_LOG_WARNING ("CamelMultipart::remove_part_at : part to remove is NULL\n");
		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::remove_part_at : index = %d, number of parts=%d\n", 
				      index, g_list_length (parts_list));
		return NULL;
	}
	removed_body_part = CAMEL_MIME_BODY_PART (part_to_remove->data);

	multipart->parts = g_list_remove_link (parts_list, part_to_remove);
	if (part_to_remove->data) gtk_object_unref (GTK_OBJECT (part_to_remove->data));
	g_list_free_1 (part_to_remove);
	
	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Leaving remove_part_at\n");
	return removed_body_part;
	
}

CamelMimeBodyPart *
camel_multipart_remove_part_at (CamelMultipart *multipart, guint index)
{
	return CMP_CLASS (multipart)->remove_part_at (multipart, index);
}


static CamelMimeBodyPart *
_get_part (CamelMultipart *multipart, guint index)
{
	GList *part;
	if (!(multipart->parts)) {
		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::get_part part list is void \n");
		return NULL;
	}
	
	part = g_list_nth (multipart->parts, index);
	if (part) return CAMEL_MIME_BODY_PART (part->data);
	else {
		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::get_part part number %d not found\n", index);
		return NULL;
	}		
}

CamelMimeBodyPart * 
camel_multipart_get_part (CamelMultipart *multipart, guint index)
{
	return CMP_CLASS (multipart)->get_part (multipart, index);
}


static guint 
_get_number (CamelMultipart *multipart)
{
	return g_list_length (multipart->parts);
}

guint 
camel_multipart_get_number (CamelMultipart *multipart)
{
	return CMP_CLASS (multipart)->get_number (multipart);
}


static void
_set_parent (CamelMultipart *multipart, CamelMimePart *parent)
{
	multipart->parent = parent;
	if (parent) gtk_object_ref (GTK_OBJECT (parent));
}

void
camel_multipart_set_parent (CamelMultipart *multipart, CamelMimePart *parent)
{
	CMP_CLASS (multipart)->set_parent (multipart, parent);
}


static CamelMimePart *
_get_parent (CamelMultipart *multipart)
{
	return multipart->parent;
}


CamelMimePart *
camel_multipart_get_parent (CamelMultipart *multipart)
{
	return CMP_CLASS (multipart)->get_parent (multipart);
}





static void 
_set_boundary (CamelMultipart *multipart, gchar *boundary)
{
	gmime_content_field_set_parameter (CAMEL_DATA_WRAPPER (multipart)->mime_type, "boundary", boundary);
}

void 
camel_multipart_set_boundary (CamelMultipart *multipart, gchar *boundary)
{
	CMP_CLASS (multipart)->set_boundary (multipart, boundary);
}


static const gchar *
_get_boundary (CamelMultipart *multipart)
{
	const gchar *boundary;
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMultipart::_get_boundary\n");
	if (!CAMEL_DATA_WRAPPER (multipart)->mime_type) {
		CAMEL_LOG_WARNING ("CamelMultipart::_get_boundary CAMEL_DATA_WRAPPER (multipart)->mime_type is NULL\n");
		return NULL;
	}
	boundary = gmime_content_field_get_parameter (CAMEL_DATA_WRAPPER (multipart)->mime_type, "boundary");
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMultipart::boundary found : \"%s\"\n", boundary);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMultipart::_get_boundary\n");
	return boundary;
}


const gchar *
camel_multipart_get_boundary (CamelMultipart *multipart)
{
	return CMP_CLASS (multipart)->get_boundary (multipart);
}


struct _print_part_user_data {
	CamelStream *stream;
	const gchar *boundary;
};



static void
_print_part (gpointer data, gpointer user_data)
{
	CamelMimeBodyPart *body_part = CAMEL_MIME_BODY_PART (data);
	struct _print_part_user_data *ud = (struct _print_part_user_data *)user_data;
	
	if (ud->boundary) camel_stream_write_strings (ud->stream, "\n--", ud->boundary, "\n", NULL);
	else camel_stream_write_strings (ud->stream, "\n--\n", NULL);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (body_part), ud->stream);

	
}


	
static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMultipart *multipart = CAMEL_MULTIPART (data_wrapper);
	struct _print_part_user_data ud;
	const gchar *boundary;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMultipart::_write_to_stream entering\n");
	boundary = camel_multipart_get_boundary (multipart);
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMultipart::_write_to_stream, boundary = %s\n", boundary);
	g_return_if_fail (boundary);
	ud.boundary = boundary;
	ud.stream = stream;
	if (multipart->preface) camel_stream_write_strings (stream, multipart->preface, NULL);
	g_list_foreach (multipart->parts, _print_part, (gpointer)&ud);
	camel_stream_write_strings (stream, "\n--", boundary, "--\n", NULL);
	if (multipart->postface) camel_stream_write_strings (stream, multipart->postface, NULL);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMultipart::_write_to_stream leaving \n");
}







/**********************/
/* new implementation */


/**
 * _localize_part: localize one part in a multipart environement.
 * @stream: the stream  to read the lines from.
 * @normal_boundary: end of part bundary.
 * @end_boundary: end of multipart boundary.
 * @end_position : end position of the mime part
 * 
 * This routine is a bit special: RFC 2046 says that, in a multipart 
 * environment, the last crlf before a boundary belongs to the boundary.
 * Thus, if there is no blank line before the boundary, the last crlf
 * of the last line of the part is removed. 
 * 
 * Return value: true if the last boundary element has been found or if no more data was available from the stream, flase otherwise 
 **/

static gboolean
_localize_part (CamelStream *stream, 
		gchar *normal_boundary, 
		gchar *end_boundary,
		guint32 *end_position)
{
	gchar *new_line = NULL;
	gboolean end_of_part = FALSE;
	gboolean last_part = FALSE;
	guint32 last_position;

	/* Note for future enhancements */
	/* RFC 2046 precises that when parsing the content of a multipart 
	 * element, the program should not think it will find the last boundary,
	 * and in particular, the message could have been damaged during
	 * transport, the parsing should still be OK */
	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Entering _localize_part\n");

	g_assert (CAMEL_IS_SEEKABLE_STREAM (stream));

	last_position = camel_seekable_stream_get_current_position (CAMEL_SEEKABLE_STREAM (stream));
	new_line = gmime_read_line_from_stream (stream);

	while (new_line && !end_of_part && !last_part) {
		end_of_part = (strcmp (new_line, normal_boundary) == 0);
		last_part   = (strcmp (new_line, end_boundary) == 0);
		if (!end_of_part && !last_part) {
			
			g_free (new_line);
			
			last_position = 
				camel_seekable_stream_get_current_position (CAMEL_SEEKABLE_STREAM (stream));

			new_line = gmime_read_line_from_stream (stream);
		}
	}
	
	if (new_line) g_free (new_line);
	else last_part = TRUE;

	*end_position = last_position;

	CAMEL_LOG_FULL_DEBUG ("CamelMultipart:: Leaving _localize_part\n");
	return (last_part);
}




static void
_set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
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
	

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMultipart::_set_input_stream\n");
	boundary = camel_multipart_get_boundary (multipart);
	g_return_if_fail (boundary);
	
	real_boundary_line = g_strdup_printf ("--%s", boundary);
	end_boundary_line  = g_strdup_printf ("--%s--", boundary);

	
	/* read the prefix if any */
	end_of_multipart = _localize_part (stream, 
					   real_boundary_line, 
					   end_boundary_line, 
					   &part_end);
	if (multipart->preface) g_free (multipart->preface);

	/* if ( (new_part->str)[0] != '\0') multipart->preface = g_strdup (new_part->str); */
	
	/* read all the real parts */
	while (!end_of_multipart) {
		/* determine the position of the begining of the part */
		part_begining = camel_seekable_stream_get_current_position (seekable_stream);

		CAMEL_LOG_FULL_DEBUG ("CamelMultipart::set_input_stream, detected a new part\n");
		body_part = camel_mime_body_part_new ();
		
		end_of_multipart = _localize_part (stream, 
						   real_boundary_line, 
						   end_boundary_line, 
						   &part_end);
		body_part_input_stream = 
			camel_seekable_substream_new_with_seekable_stream_and_bounds (seekable_stream,
										      part_begining, 
										      part_end);

		/* the seekable substream may change the position of the stream
		   so we must save it before calling set_input_stream */
		saved_stream_pos = camel_seekable_stream_get_current_position (seekable_stream);

		camel_data_wrapper_set_input_stream (CAMEL_DATA_WRAPPER (body_part), 
						     CAMEL_STREAM (body_part_input_stream));

		/* restore the stream position */
		camel_seekable_stream_seek (seekable_stream, saved_stream_pos, CAMEL_STREAM_SET);

		/* add the body part to the multipart object */
		camel_multipart_add_part (multipart, body_part);
		
	}

	/* g_string_assign (new_part, ""); */
	/* _localize_part (new_part, stream, real_boundary_line, end_boundary_line); */

	if (multipart->postface) g_free (multipart->postface);
	/* if ( (new_part->str)[0] != '\0') multipart->postface = g_strdup (new_part->str); */

	/* g_string_free (new_part, TRUE); */

	g_free (real_boundary_line);
	g_free (end_boundary_line);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMultipart::_set_input_stream\n");
}
