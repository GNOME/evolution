/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#include "config.h"  
#include "pas-backend-vcf.h"
#include "pas-backend-card-sexp.h"
#include "pas-book.h"
#include "pas-book-view.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include <ebook/e-contact.h>
#include <libgnome/gnome-i18n.h>

#define PAS_ID_PREFIX "pas-id-"
#define FILE_FLUSH_TIMEOUT 5000

static PASBackendSyncClass *pas_backend_vcf_parent_class;
typedef struct _PASBackendVCFBookView PASBackendVCFBookView;
typedef struct _PASBackendVCFSearchContext PASBackendVCFSearchContext;

struct _PASBackendVCFPrivate {
	char       *uri;
	char       *filename;
	GMutex     *mutex;
	GHashTable *contacts;
	gboolean    dirty;
	int         flush_timeout_tag;
};

static char *
pas_backend_vcf_create_unique_id ()
{
	/* use a 32 counter and the 32 bit timestamp to make an id.
	   it's doubtful 2^32 id's will be created in a second, so we
	   should be okay. */
	static guint c = 0;
	return g_strdup_printf (PAS_ID_PREFIX "%08lX%08X", time(NULL), c++);
}

typedef struct {
	PASBackendVCF *bvcf;
	PASBook *book;
	PASBookView *view;
} VCFBackendSearchClosure;

static void
free_search_closure (VCFBackendSearchClosure *closure)
{
	g_free (closure);
}

static void
foreach_search_compare (char *id, char *vcard_string, VCFBackendSearchClosure *closure)
{
	EContact *contact;

	contact = e_contact_new_from_vcard (vcard_string);
	pas_book_view_notify_update (closure->view, contact);
	g_object_unref (contact);
}

static gboolean
pas_backend_vcf_search_timeout (gpointer data)
{
	VCFBackendSearchClosure *closure = data;

	g_hash_table_foreach (closure->bvcf->priv->contacts,
			      (GHFunc)foreach_search_compare,
			      closure);

	pas_book_view_notify_complete (closure->view, GNOME_Evolution_Addressbook_Success);

	free_search_closure (closure);

	return FALSE;
}


static void
pas_backend_vcf_search (PASBackendVCF  	      *bvcf,
			PASBookView           *book_view)
{
	const char *query = pas_book_view_get_card_query (book_view);
	VCFBackendSearchClosure *closure = g_new0 (VCFBackendSearchClosure, 1);

	if ( ! strcmp (query, "(contains \"x-evolution-any-field\" \"\")"))
		pas_book_view_notify_status_message (book_view, _("Loading..."));
	else
		pas_book_view_notify_status_message (book_view, _("Searching..."));

	closure->view = book_view;
	closure->bvcf = bvcf;

	g_idle_add (pas_backend_vcf_search_timeout, closure);
}

static void
insert_contact (PASBackendVCF *vcf, char *vcard)
{
	EContact *contact = e_contact_new_from_vcard (vcard);
	char *id;

	id = e_contact_get (contact, E_CONTACT_UID);
	if (id)
		g_hash_table_insert (vcf->priv->contacts,
				     id,
				     e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30));
}

static void
load_file (PASBackendVCF *vcf, int fd)
{
	FILE *fp;
	GString *str;
	char buf[1024];

	fp = fdopen (fd, "r");
	if (!fp) {
		g_warning ("failed to open `%s' for reading", vcf->priv->filename);
		return;
	}

	str = g_string_new ("");

	while (fgets (buf, sizeof (buf), fp)) {
		if (!strcmp (buf, "\r\n")) {
			/* if the string has accumulated some stuff, create a contact for it and start over */
			if (str->len) {
				insert_contact (vcf, str->str);
				g_string_assign (str, "");
			}
		}
		else {
			g_string_append (str, buf);
		}
	}
	if (str->len) {
		insert_contact (vcf, str->str);
	}

	g_string_free (str, TRUE);

	fclose (fp);
}

static void
foreach_build_list (char *id, char *vcard_string, GList **list)
{
	*list = g_list_append (*list, vcard_string);
}

static gboolean
save_file (PASBackendVCF *vcf)
{
	GList *vcards = NULL;
	GList *l;
	char *new_path;
	int fd, rv;

	g_warning ("PASBackendVCF flushing file to disk");

	g_mutex_lock (vcf->priv->mutex);
	g_hash_table_foreach (vcf->priv->contacts, (GHFunc)foreach_build_list, &vcards);

	new_path = g_strdup_printf ("%s.new", vcf->priv->filename);

	fd = open (new_path, O_CREAT | O_TRUNC | O_WRONLY, 0666);

	for (l = vcards; l; l = l->next) {
		char *vcard_str = l->data;
		int len = strlen (vcard_str);

		rv = write (fd, vcard_str, len);

		if (rv < len) {
			/* XXX */
			g_warning ("write failed.  we need to handle short writes\n");
			close (fd);
			unlink (new_path);
			return FALSE;
		}

		rv = write (fd, "\r\n\r\n", 4);
		if (rv < 4) {
			/* XXX */
			g_warning ("write failed.  we need to handle short writes\n");
			close (fd);
			unlink (new_path);
			return FALSE;
		}
	}

	if (0 > rename (new_path, vcf->priv->filename)) {
		g_warning ("Failed to rename %s: %s\n", vcf->priv->filename, strerror(errno));
		unlink (new_path);
		return FALSE;
	}

	g_list_free (vcards);
	g_free (new_path);

	vcf->priv->dirty = FALSE;

	g_mutex_unlock (vcf->priv->mutex);

	return TRUE;
}

static gboolean
vcf_flush_file (gpointer data)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (data);

	if (!bvcf->priv->dirty) {
		bvcf->priv->flush_timeout_tag = 0;
		return FALSE;
	}

	if (!save_file (bvcf)) {
		g_warning ("failed to flush the .vcf file to disk, will try again next timeout");
		return TRUE;
	}

	bvcf->priv->flush_timeout_tag = 0;
	return FALSE;
}

static EContact *
do_create(PASBackendVCF  *bvcf,
	  const char     *vcard_req,
	  gboolean        dirty_the_file)
{
	char           *id;
	EContact       *contact;
	char           *vcard;

	/* at the very least we need the unique_id generation to be
	   protected by the lock, even if the actual vcard parsing
	   isn't. */
	g_mutex_lock (bvcf->priv->mutex);
	id = pas_backend_vcf_create_unique_id ();

	contact = e_contact_new_from_vcard (vcard_req);
	e_contact_set(contact, E_CONTACT_UID, id);
	vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

	g_hash_table_insert (bvcf->priv->contacts, id, vcard);

	if (dirty_the_file) {
		bvcf->priv->dirty = TRUE;

		if (!bvcf->priv->flush_timeout_tag)
			bvcf->priv->flush_timeout_tag = g_timeout_add (FILE_FLUSH_TIMEOUT,
								       vcf_flush_file, bvcf);
	}

	g_mutex_unlock (bvcf->priv->mutex);

	return contact;
}

static PASBackendSyncStatus
pas_backend_vcf_process_create_contact (PASBackendSync *backend,
					PASBook *book,
					const char *vcard,
					EContact **contact)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);

	*contact = do_create(bvcf, vcard, TRUE);
	if (*contact) {
		return GNOME_Evolution_Addressbook_Success;
	}
	else {
		/* XXX need a different call status for this case, i
                   think */
		return GNOME_Evolution_Addressbook_ContactNotFound;
	}
}

static PASBackendSyncStatus
pas_backend_vcf_process_remove_contacts (PASBackendSync *backend,
					 PASBook    *book,
					 GList *id_list,
					 GList **ids)
{
	/* FIXME: make this handle bulk deletes like the file backend does */
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);
	char *id = id_list->data;
	gboolean success;

	g_mutex_lock (bvcf->priv->mutex);
	success = g_hash_table_remove (bvcf->priv->contacts, id);

	if (!success) {
		g_mutex_unlock (bvcf->priv->mutex);
		return GNOME_Evolution_Addressbook_ContactNotFound;
	}
	else {
		bvcf->priv->dirty = TRUE;
		if (!bvcf->priv->flush_timeout_tag)
			bvcf->priv->flush_timeout_tag = g_timeout_add (FILE_FLUSH_TIMEOUT,
								       vcf_flush_file, bvcf);
		g_mutex_unlock (bvcf->priv->mutex);

		*ids = g_list_append (*ids, id);

		return GNOME_Evolution_Addressbook_Success;
	}
}

static PASBackendSyncStatus
pas_backend_vcf_process_modify_contact (PASBackendSync *backend,
					PASBook *book,
					const char *vcard,
					EContact **contact)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);
	char *old_id, *old_vcard_string;
	const char *id;
	gboolean success;

	/* create a new ecard from the request data */
	*contact = e_contact_new_from_vcard (vcard);
	id = e_contact_get_const (*contact, E_CONTACT_UID);

	g_mutex_lock (bvcf->priv->mutex);
	success = g_hash_table_lookup_extended (bvcf->priv->contacts, id, (gpointer)&old_id, (gpointer)&old_vcard_string);

	if (!success) {
		g_mutex_unlock (bvcf->priv->mutex);
		return GNOME_Evolution_Addressbook_ContactNotFound;
	}
	else {
		g_hash_table_insert (bvcf->priv->contacts, old_id, g_strdup (vcard));
		bvcf->priv->dirty = TRUE;
		if (!bvcf->priv->flush_timeout_tag)
			bvcf->priv->flush_timeout_tag = g_timeout_add (FILE_FLUSH_TIMEOUT,
								       vcf_flush_file, bvcf);
		g_mutex_unlock (bvcf->priv->mutex);

		g_free (old_vcard_string);

		return GNOME_Evolution_Addressbook_Success;
	}
}

static PASBackendSyncStatus
pas_backend_vcf_process_get_contact (PASBackendSync *backend,
				     PASBook    *book,
				     const char *id,
				     char **vcard)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);
	char *v;

	v = g_hash_table_lookup (bvcf->priv->contacts, id);

	if (v) {
		*vcard = g_strdup (v);
		return GNOME_Evolution_Addressbook_Success;
	} else {
		*vcard = g_strdup ("");
		return GNOME_Evolution_Addressbook_ContactNotFound;
	}
}


typedef struct {
	PASBackendVCF      *bvcf;
	gboolean            search_needed;
	PASBackendCardSExp *card_sexp;
	GList              *list;
} GetContactListClosure;

static void
foreach_get_contact_compare (char *id, char *vcard_string, GetContactListClosure *closure)
{
	if ((!closure->search_needed) || pas_backend_card_sexp_match_vcard  (closure->card_sexp, vcard_string)) {
		closure->list = g_list_append (closure->list, g_strdup (vcard_string));
	}
}

static PASBackendSyncStatus
pas_backend_vcf_process_get_contact_list (PASBackendSync *backend,
				       PASBook    *book,
				       const char *query,
				       GList **contacts)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);
	const char *search = query;
	GetContactListClosure closure;

	closure.bvcf = bvcf;
	closure.search_needed = strcmp (search, "(contains \"x-evolution-any-field\" \"\")");
	closure.card_sexp = pas_backend_card_sexp_new (search);
	closure.list = NULL;

	g_hash_table_foreach (bvcf->priv->contacts, (GHFunc)foreach_get_contact_compare, &closure);

	g_object_unref (closure.card_sexp);

	*contacts = closure.list;
	return GNOME_Evolution_Addressbook_Success;
}

static void
pas_backend_vcf_start_book_view (PASBackend  *backend,
				 PASBookView *book_view)
{
	pas_backend_vcf_search (PAS_BACKEND_VCF (backend), book_view);
}

static void
pas_backend_vcf_stop_book_view (PASBackend  *backend,
				PASBookView *book_view)
{
	/* XXX nothing here yet, and there should be. */
}

static char *
pas_backend_vcf_extract_path_from_uri (const char *uri)
{
	g_assert (strncasecmp (uri, "vcf://", 6) == 0);

	return g_strdup (uri + 6);
}

static PASBackendSyncStatus
pas_backend_vcf_process_authenticate_user (PASBackendSync *backend,
					   PASBook    *book,
					   const char *user,
					   const char *passwd,
					   const char *auth_method)
{
	return GNOME_Evolution_Addressbook_Success;
}

static PASBackendSyncStatus
pas_backend_vcf_process_get_supported_fields (PASBackendSync *backend,
					      PASBook    *book,
					      GList **fields_out)
{
	GList *fields = NULL;
	int i;

	/* XXX we need a way to say "we support everything", since the
	   vcf backend does */
	for (i = 0; i < E_CONTACT_FIELD_LAST; i ++)
		fields = g_list_append (fields, (char*)e_contact_field_name (i));		

	*fields_out = fields;
	return GNOME_Evolution_Addressbook_Success;
}

#include "ximian-vcard.h"

static GNOME_Evolution_Addressbook_CallStatus
pas_backend_vcf_load_uri (PASBackend             *backend,
			  const char             *uri,
			  gboolean                only_if_exists)
{
	PASBackendVCF *bvcf = PAS_BACKEND_VCF (backend);
	char           *dirname;
	gboolean        writable = FALSE;
	int fd;

	g_free(bvcf->priv->uri);
	bvcf->priv->uri = g_strdup (uri);

	dirname = pas_backend_vcf_extract_path_from_uri (uri);
	bvcf->priv->filename = g_build_filename (dirname, "addressbook.vcf", NULL);

	fd = open (bvcf->priv->filename, O_RDWR);

	bvcf->priv->contacts = g_hash_table_new_full (g_str_hash, g_str_equal,
						      g_free, g_free);

	if (fd != -1) {
		writable = TRUE;
	} else {
		fd = open (bvcf->priv->filename, O_RDONLY);

		if (fd == -1) {
			fd = open (bvcf->priv->filename, O_CREAT, 0666);

			if (fd != -1 && !only_if_exists) {
				EContact *contact;

				contact = do_create(bvcf, XIMIAN_VCARD, FALSE);
				save_file (bvcf);

				/* XXX check errors here */
				g_object_unref (contact);

				writable = TRUE;
			}
		}
	}

	if (fd == -1) {
		g_warning ("Failed to open addressbook at uri `%s'", uri);
		g_warning ("error == %s", strerror(errno));
		return GNOME_Evolution_Addressbook_OtherError;
	}

	load_file (bvcf, fd);

	pas_backend_set_is_loaded (backend, TRUE);
	pas_backend_set_is_writable (backend, writable);

	return GNOME_Evolution_Addressbook_Success;
}

static char *
pas_backend_vcf_get_static_capabilities (PASBackend *backend)
{
	return g_strdup("local,do-initial-query");
}

static GNOME_Evolution_Addressbook_CallStatus
pas_backend_vcf_cancel_operation (PASBackend *backend, PASBook *book)
{
	return GNOME_Evolution_Addressbook_CouldNotCancel;
}

static gboolean
pas_backend_vcf_construct (PASBackendVCF *backend)
{
	g_assert (backend != NULL);
	g_assert (PAS_IS_BACKEND_VCF (backend));

	if (! pas_backend_construct (PAS_BACKEND (backend)))
		return FALSE;

	return TRUE;
}

/**
 * pas_backend_vcf_new:
 */
PASBackend *
pas_backend_vcf_new (void)
{
	PASBackendVCF *backend;

	backend = g_object_new (PAS_TYPE_BACKEND_VCF, NULL);

	if (! pas_backend_vcf_construct (backend)) {
		g_object_unref (backend);

		return NULL;
	}

	return PAS_BACKEND (backend);
}

static void
pas_backend_vcf_dispose (GObject *object)
{
	PASBackendVCF *bvcf;

	bvcf = PAS_BACKEND_VCF (object);

	if (bvcf->priv) {

		g_mutex_lock (bvcf->priv->mutex);

		if (bvcf->priv->flush_timeout_tag) {
			g_source_remove (bvcf->priv->flush_timeout_tag);
			bvcf->priv->flush_timeout_tag = 0;
		}

		if (bvcf->priv->dirty)
			save_file (bvcf);

		g_hash_table_destroy (bvcf->priv->contacts);

		g_free (bvcf->priv->uri);
		g_free (bvcf->priv->filename);

		g_mutex_unlock (bvcf->priv->mutex);

		g_mutex_free (bvcf->priv->mutex);

		g_free (bvcf->priv);
		bvcf->priv = NULL;
	}

	G_OBJECT_CLASS (pas_backend_vcf_parent_class)->dispose (object);	
}

static void
pas_backend_vcf_class_init (PASBackendVCFClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	PASBackendSyncClass *sync_class;
	PASBackendClass *backend_class;

	pas_backend_vcf_parent_class = g_type_class_peek_parent (klass);

	sync_class = PAS_BACKEND_SYNC_CLASS (klass);
	backend_class = PAS_BACKEND_CLASS (klass);

	/* Set the virtual methods. */
	backend_class->load_uri                = pas_backend_vcf_load_uri;
	backend_class->get_static_capabilities = pas_backend_vcf_get_static_capabilities;
	backend_class->start_book_view         = pas_backend_vcf_start_book_view;
	backend_class->stop_book_view          = pas_backend_vcf_stop_book_view;
	backend_class->cancel_operation        = pas_backend_vcf_cancel_operation;

	sync_class->create_contact_sync        = pas_backend_vcf_process_create_contact;
	sync_class->remove_contacts_sync       = pas_backend_vcf_process_remove_contacts;
	sync_class->modify_contact_sync        = pas_backend_vcf_process_modify_contact;
	sync_class->get_contact_sync           = pas_backend_vcf_process_get_contact;
	sync_class->get_contact_list_sync      = pas_backend_vcf_process_get_contact_list;
	sync_class->authenticate_user_sync     = pas_backend_vcf_process_authenticate_user;
	sync_class->get_supported_fields_sync  = pas_backend_vcf_process_get_supported_fields;

	object_class->dispose = pas_backend_vcf_dispose;
}

static void
pas_backend_vcf_init (PASBackendVCF *backend)
{
	PASBackendVCFPrivate *priv;

	priv                 = g_new0 (PASBackendVCFPrivate, 1);
	priv->uri            = NULL;
	priv->mutex = g_mutex_new();

	backend->priv = priv;
}

/**
 * pas_backend_vcf_get_type:
 */
GType
pas_backend_vcf_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (PASBackendVCFClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  pas_backend_vcf_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PASBackendVCF),
			0,    /* n_preallocs */
			(GInstanceInitFunc) pas_backend_vcf_init
		};

		type = g_type_register_static (PAS_TYPE_BACKEND_SYNC, "PASBackendVCF", &info, 0);
	}

	return type;
}
