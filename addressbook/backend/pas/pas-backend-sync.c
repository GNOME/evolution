/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#include <config.h>
#include "pas-backend-sync.h"
#include "pas-marshal.h"

struct _PASBackendSyncPrivate {
  int mumble;
};

static GObjectClass *parent_class;

gboolean
pas_backend_sync_construct (PASBackendSync *backend)
{
	return TRUE;
}

PASBackendSyncStatus
pas_backend_sync_create_contact (PASBackendSync *backend,
				 PASBook *book,
				 const char *vcard,
				 EContact **contact)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (vcard, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (contact, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->create_contact_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->create_contact_sync) (backend, book, vcard, contact);
}

PASBackendSyncStatus
pas_backend_sync_remove (PASBackendSync *backend,
			 PASBook *book)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->remove_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->remove_sync) (backend, book);
}

PASBackendSyncStatus
pas_backend_sync_remove_contacts (PASBackendSync *backend,
				  PASBook *book,
				  GList *id_list,
				  GList **removed_ids)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (id_list, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (removed_ids, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->remove_contacts_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->remove_contacts_sync) (backend, book, id_list, removed_ids);
}

PASBackendSyncStatus
pas_backend_sync_modify_contact (PASBackendSync *backend,
				 PASBook *book,
				 const char *vcard,
				 EContact **contact)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (vcard, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (contact, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->modify_contact_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->modify_contact_sync) (backend, book, vcard, contact);
}

PASBackendSyncStatus
pas_backend_sync_get_contact (PASBackendSync *backend,
			      PASBook *book,
			      const char *id,
			      char **vcard)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (id, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (vcard, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->get_contact_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->get_contact_sync) (backend, book, id, vcard);
}

PASBackendSyncStatus
pas_backend_sync_get_contact_list (PASBackendSync *backend,
				   PASBook *book,
				   const char *query,
				   GList **contacts)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (query, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (contacts, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->get_contact_list_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->get_contact_list_sync) (backend, book, query, contacts);
}

PASBackendSyncStatus
pas_backend_sync_get_changes (PASBackendSync *backend,
			      PASBook *book,
			      const char *change_id,
			      GList **changes)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (change_id, GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (changes, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->get_changes_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->get_changes_sync) (backend, book, change_id, changes);
}

PASBackendSyncStatus
pas_backend_sync_authenticate_user (PASBackendSync *backend,
				    PASBook *book,
				    const char *user,
				    const char *passwd,
				    const char *auth_method)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (user && passwd && auth_method, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->authenticate_user_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->authenticate_user_sync) (backend, book, user, passwd, auth_method);
}

PASBackendSyncStatus
pas_backend_sync_get_supported_fields (PASBackendSync *backend,
				       PASBook *book,
				       GList **fields)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (fields, GNOME_Evolution_Addressbook_OtherError);
	
	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->get_supported_fields_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->get_supported_fields_sync) (backend, book, fields);
}

PASBackendSyncStatus
pas_backend_sync_get_supported_auth_methods (PASBackendSync *backend,
					     PASBook *book,
					     GList **methods)
{
	g_return_val_if_fail (backend && PAS_IS_BACKEND_SYNC (backend), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (book && PAS_IS_BOOK (book), GNOME_Evolution_Addressbook_OtherError);
	g_return_val_if_fail (methods, GNOME_Evolution_Addressbook_OtherError);

	g_assert (PAS_BACKEND_SYNC_GET_CLASS (backend)->get_supported_auth_methods_sync);

	return (* PAS_BACKEND_SYNC_GET_CLASS (backend)->get_supported_auth_methods_sync) (backend, book, methods);
}

static void
_pas_backend_remove (PASBackend *backend,
		     PASBook    *book)
{
	PASBackendSyncStatus status;

	status = pas_backend_sync_remove (PAS_BACKEND_SYNC (backend), book);

	pas_book_respond_remove (book, status);
}

static void
_pas_backend_create_contact (PASBackend *backend,
			     PASBook    *book,
			     const char *vcard)
{
	PASBackendSyncStatus status;
	EContact *contact;

	status = pas_backend_sync_create_contact (PAS_BACKEND_SYNC (backend), book, vcard, &contact);

	pas_book_respond_create (book, status, contact);

	g_object_unref (contact);
}

static void
_pas_backend_remove_contacts (PASBackend *backend,
			      PASBook    *book,
			      GList      *id_list)
{
	PASBackendSyncStatus status;
	GList *ids = NULL;

	status = pas_backend_sync_remove_contacts (PAS_BACKEND_SYNC (backend), book, id_list, &ids);

	pas_book_respond_remove_contacts (book, status, ids);

	g_list_free (ids);
}

static void
_pas_backend_modify_contact (PASBackend *backend,
			     PASBook    *book,
			     const char *vcard)
{
	PASBackendSyncStatus status;
	EContact *contact;

	status = pas_backend_sync_modify_contact (PAS_BACKEND_SYNC (backend), book, vcard, &contact);

	pas_book_respond_modify (book, status, contact);

	g_object_unref (contact);
}

static void
_pas_backend_get_contact (PASBackend *backend,
			  PASBook    *book,
			  const char *id)
{
	PASBackendSyncStatus status;
	char *vcard;

	status = pas_backend_sync_get_contact (PAS_BACKEND_SYNC (backend), book, id, &vcard);

	pas_book_respond_get_contact (book, status, vcard);

	g_free (vcard);
}

static void
_pas_backend_get_contact_list (PASBackend *backend,
			       PASBook    *book,
			       const char *query)
{
	PASBackendSyncStatus status;
	GList *cards = NULL;

	status = pas_backend_sync_get_contact_list (PAS_BACKEND_SYNC (backend), book, query, &cards);

	pas_book_respond_get_contact_list (book, status, cards);
}

static void
_pas_backend_get_changes (PASBackend *backend,
			  PASBook    *book,
			  const char *change_id)
{
	PASBackendSyncStatus status;
	GList *changes = NULL;

	status = pas_backend_sync_get_changes (PAS_BACKEND_SYNC (backend), book, change_id, &changes);

	pas_book_respond_get_changes (book, status, changes);

	/* XXX free view? */
}

static void
_pas_backend_authenticate_user (PASBackend *backend,
				PASBook    *book,
				const char *user,
				const char *passwd,
				const char *auth_method)
{
	PASBackendSyncStatus status;

	status = pas_backend_sync_authenticate_user (PAS_BACKEND_SYNC (backend), book, user, passwd, auth_method);

	pas_book_respond_authenticate_user (book, status);
}

static void
_pas_backend_get_supported_fields (PASBackend *backend,
				   PASBook    *book)
{
	PASBackendSyncStatus status;
	GList *fields = NULL;

	status = pas_backend_sync_get_supported_fields (PAS_BACKEND_SYNC (backend), book, &fields);

	pas_book_respond_get_supported_fields (book, status, fields);

	g_list_foreach (fields, (GFunc)g_free, NULL);
	g_list_free (fields);
}

static void
_pas_backend_get_supported_auth_methods (PASBackend *backend,
					 PASBook    *book)
{
	PASBackendSyncStatus status;
	GList *methods = NULL;

	status = pas_backend_sync_get_supported_auth_methods (PAS_BACKEND_SYNC (backend), book, &methods);

	pas_book_respond_get_supported_auth_methods (book, status, methods);

	g_list_foreach (methods, (GFunc)g_free, NULL);
	g_list_free (methods);
}

static void
pas_backend_sync_init (PASBackendSync *backend)
{
	PASBackendSyncPrivate *priv;

	priv          = g_new0 (PASBackendSyncPrivate, 1);

	backend->priv = priv;
}

static void
pas_backend_sync_dispose (GObject *object)
{
	PASBackendSync *backend;

	backend = PAS_BACKEND_SYNC (object);

	if (backend->priv) {
		g_free (backend->priv);

		backend->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
pas_backend_sync_class_init (PASBackendSyncClass *klass)
{
	GObjectClass *object_class;
	PASBackendClass *backend_class = PAS_BACKEND_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass *) klass;

	backend_class->remove = _pas_backend_remove;
	backend_class->create_contact = _pas_backend_create_contact;
	backend_class->remove_contacts = _pas_backend_remove_contacts;
	backend_class->modify_contact = _pas_backend_modify_contact;
	backend_class->get_contact = _pas_backend_get_contact;
	backend_class->get_contact_list = _pas_backend_get_contact_list;
	backend_class->get_changes = _pas_backend_get_changes;
	backend_class->authenticate_user = _pas_backend_authenticate_user;
	backend_class->get_supported_fields = _pas_backend_get_supported_fields;
	backend_class->get_supported_auth_methods = _pas_backend_get_supported_auth_methods;

	object_class->dispose = pas_backend_sync_dispose;
}

/**
 * pas_backend_get_type:
 */
GType
pas_backend_sync_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (PASBackendSyncClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  pas_backend_sync_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PASBackendSync),
			0,    /* n_preallocs */
			(GInstanceInitFunc) pas_backend_sync_init
		};

		type = g_type_register_static (PAS_TYPE_BACKEND, "PASBackendSync", &info, 0);
	}

	return type;
}
