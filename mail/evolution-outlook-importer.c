/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <bonobo.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <stdio.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <camel/camel-session.h>
#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-exception.h>
#include <camel/camel-url.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Mail_Outlook_ImporterFactory"

static BonoboGenericFactory *factory = NULL;

typedef struct {
	char *filename;
	gboolean oe4; /* Is file OE4 or not? */
	FILE *handle;
	fpos_t pos;
	off_t size;

	CamelStream *mstream;
	CamelFolder *folder;

	gboolean busy;
} OutlookImporter;

struct oe_msg_segmentheader {
	int self;
	int increase;
	int include;
	int next;
	int usenet;
};

typedef struct oe_msg_segmentheader oe_msg_segmentheader;


/* EvolutionImporter methods */

static void
add_line (OutlookImporter *oli,
	  const char *str,
	  gboolean finished)
{
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	CamelException *ex;

	if (oli->mstream == NULL) {
		oli->mstream = camel_stream_mem_new ();
	}

	camel_stream_write (oli->mstream, str, strlen (str));
	
	if (finished == FALSE)
		return;

	camel_stream_reset (oli->mstream);
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_SEEN;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  oli->mstream);
	
	camel_object_unref (CAMEL_OBJECT (oli->mstream));
	oli->mstream = NULL;

	ex = camel_exception_new ();
	camel_folder_append_message (oli->folder, msg, info, ex);
	camel_object_unref (CAMEL_OBJECT (msg));

	camel_exception_free (ex);
	g_free (info);
}

/* Based on code from liboe 0.92 (STABLE)
   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com)
   Modified 2001 Iain Holmes  <iain@ximian.com>
   Copyright (C) 2001 Ximian, Inc. */

static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	OutlookImporter *oli = (OutlookImporter *) closure;
	oe_msg_segmentheader *header;
	gboolean more = TRUE;
	char *cb, *sfull, *s;
	fpos_t end_pos = 0;
	int i;

	if (oli->busy == TRUE) {
		GNOME_Evolution_ImporterListener_notifyResult (listener, 
							       GNOME_Evolution_ImporterListener_BUSY,
							       more, ev);
		return;
	}

	oli->busy = TRUE;
	header = g_new (oe_msg_segmentheader, 1);
	fread (header, 16, 1, oli->handle);

	/* Write a From line */
	add_line (oli, "From evolution-outlook-importer", FALSE);
	end_pos = oli->pos + header->include;
	if (end_pos >= oli->size) {
		end_pos = oli->size;
		more = FALSE;
	}

	oli->pos += 4;

	cb = g_new (char, 4);
	sfull = g_new (char, 65536);
	s = sfull;

	while (oli->pos < end_pos) {
		fread (cb, 1, 4, oli->handle);
		for (i = 0; i < 4; i++, oli->pos++) {
			if (*(cb + i ) != 0x0d) {
				*s++ = *(cb + i);

				if (*(cb + i) == 0x0a) {
					*s = '\0';
					add_line (oli, sfull, FALSE);
					s = sfull;
				}
			}
		}
	}

	if (s != sfull) {
		*s = '\0';
		add_line (oli, sfull, FALSE);
		s = sfull;
	}

	add_line (oli, "\n", TRUE);

	oli->pos = end_pos;
	fsetpos (oli->handle, &oli->pos);

	g_free (header);
	g_free (sfull);
	g_free (cb);

	GNOME_Evolution_ImporterListener_notifyResult (listener, 
						       GNOME_Evolution_ImporterListener_OK,
						       more, ev);
	if (more == FALSE) {
		CamelException *ex;

		ex = camel_exception_new ();
		camel_folder_thaw (oli->folder);
		camel_folder_sync (oli->folder, FALSE, ex);
		camel_exception_free (ex);
		fclose (oli->handle);
		oli->handle = NULL;
	}

	oli->busy = FALSE;
	return;
}


/* EvolutionImporterFactory methods */

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	FILE *handle;
	int signature[4];

	/* Outlook Express sniffer.
	   Taken from liboe 0.92 (STABLE)
	   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com) */

	handle = fopen (filename, "rb");
	if (handle == NULL)
		return FALSE; /* Can't open file: Can't support it :) */

	  /* SIGNATURE */
	fread (&signature, 16, 1, handle); 
	if ((signature[0]!=0xFE12ADCF) || /* OE 5 & OE 5 BETA SIGNATURE */
	    (signature[1]!=0x6F74FDC5) ||
	    (signature[2]!=0x11D1E366) ||
	    (signature[3]!=0xC0004E9A)) {
		if ((signature[0]==0x36464D4A) &&
		    (signature[1]==0x00010003)) /* OE4 SIGNATURE */ {
			fclose (handle);
			return TRUE; /* OE 4 */
		}
		fclose (handle);
		return FALSE; /* Not Outlook 4 or 5 */
	}

	fclose (handle);
	return FALSE; /* Can't handle OE 5 yet */
}

static void
importer_destroy_cb (GtkObject *object,
		     OutlookImporter *oli)
{
	if (oli->folder)
		camel_object_unref (CAMEL_OBJECT (oli->folder));
	g_free (oli->filename);
	if (oli->handle)
		fclose (oli->handle);
	g_free (oli);
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      void *closure)
{
	OutlookImporter *oli;
	CamelException *ex;
	struct stat buf;
	fpos_t pos = 0x54;

	oli = (OutlookImporter *) closure;
	oli->filename = g_strdup (filename);
	/* Will return TRUE if oe4 format */
	oli->oe4 = support_format_fn (NULL, filename, NULL);
	if (oli->oe4 == FALSE)
		return FALSE;

	oli->handle = fopen (filename, "rb");
	if (oli->handle == NULL) {
		return FALSE;
	}

	/* Get size of file */
	if (stat (filename, &buf) == -1) {
		return FALSE;
	}
	
	oli->size = buf.st_size;

	/* Set the fposition to the begining */
	fsetpos (oli->handle, &pos);
	oli->pos = pos;

	oli->mstream = NULL;

	ex = camel_exception_new ();
	oli->folder = mail_local_lookup_folder ("home/iain/evolution/local/Inbox", ex);
	camel_exception_free (ex);

	if (oli->folder == NULL){
		g_print ("Bad folder\n");
		return FALSE;
	}

	camel_folder_freeze (oli->folder);
	oli->busy = FALSE;
	return TRUE;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	EvolutionImporter *importer;
	OutlookImporter *oli;

	oli = g_new0 (OutlookImporter, 1);
	importer = evolution_importer_new (support_format_fn, load_file_fn, 
					   process_item_fn, NULL, oli);
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (importer_destroy_cb), oli);

	return BONOBO_OBJECT (importer);
}

void
outlook_importer_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_IID,
					      factory_fn, NULL);

	if (factory == NULL) {
		g_error ("Unable to create factory.");
	}
}

