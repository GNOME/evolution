/*
 * folder-info.c: Implementation of GNOME_Evolution_FolderInfo
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#include "Mail.h"

#include <glib.h>
#include <libgnome/gnome-defs.h>

#include <bonobo/bonobo-xobject.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>

#include "mail-mt.h"
#include <camel/camel-exception.h>

#define FOLDER_INFO_IID "OAFIID:GNOME_Evolution_FolderInfo_Factory"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

typedef struct _EvolutionFolderInfo EvolutionFolderInfo;
typedef struct _EvolutionFolderInfoClass EvolutionFolderInfoClass;

struct _EvolutionFolderInfo {
	BonoboXObject parent;
};

struct _EvolutionFolderInfoClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_FolderInfo__epv epv;
};

/* MT stuff */
struct _folder_info_msg {
	struct _mail_msg msg;

	Bonobo_Listener listener;
	char *foldername;

	int read;
	int unread;
};

static void
do_get_info (struct _mail_msg *mm)
{
	struct _folder_info_msg *m = (struct _folder_info_msg *) mm;
	char *uri_dup;
	char *foldername, *start, *end;
	char *storage, *protocol, *uri;
	CamelFolder *folder;
	CamelException *ex;
#if 0
	/* Fixme: Do other stuff. Different stuff to the stuff below */
	uri_dup = g_strdup (m->foldername);
	start = uri_dup + 11;
	g_warning ("Start: %s", start);

	end = strrchr (start, '/');
	if (end == NULL) {
		g_warning ("Bugger");
		return;
	}

	storage = g_strndup (start, end - start);
	start = end + 1;
	foldername = g_strdup (start);

	g_free (uri_dup);

	/* Work out the protocol.
	   The storage is going to start as local, or vfolder, or an imap
	   server. */
	g_warning ("Storage: %s", storage);
	if (strncmp (storage, "local", 5) == 0) {
		char *evolution_dir;
		char *proto;

		evolution_dir = gnome_util_prepend_user_home ("evolution/local");
		proto = g_strconcat ("file://", evolution_dir, NULL);
		uri = e_path_to_physical (proto, foldername);
		g_free (evolution_dir);
		g_free (proto);

	} else if (strncmp (storage, "vfolder", 7) == 0) {
		uri = g_strconcat ("vfolder://", foldername, NULL);
	} else {
		uri = g_strconcat ("imap://", storage, foldername, NULL);
	}
#endif

	ex = camel_exception_new ();
	folder = mail_tool_uri_to_folder (m->foldername, ex);
	if (camel_exception_is_set (ex)) {
		g_warning ("Camel exception: %s", camel_exception_get_description (ex));
	}

	camel_exception_free (ex);

	m->read = camel_folder_get_message_count (folder);
	m->unread = camel_folder_get_unread_message_count (folder);
}

static void
do_got_info (struct _mail_msg *mm)
{
	struct _folder_info_msg *m = (struct _folder_info_msg *) mm;
	CORBA_Environment ev;
	CORBA_any a;
	GNOME_Evolution_FolderInfo_MessageCount count;

	g_print ("You've got mail: %d, %d\n", m->read, m->unread);

	count.path = m->foldername;
	count.count = m->read;
	count.unread = m->unread;

	a._type = (CORBA_TypeCode) TC_GNOME_Evolution_FolderInfo_MessageCount;
	a._value = &count;

	CORBA_exception_init (&ev);
	Bonobo_Listener_event (m->listener, "youve-got-mail", &a, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Got exception on listener: %s", CORBA_exception_id (&ev));
	}
	CORBA_exception_free (&ev);
}

static void
do_free_info (struct _mail_msg *mm)
{
	struct _folder_info_msg *m = (struct _folder_info_msg *) mm;

	g_free (m->foldername);
}

struct _mail_msg_op get_info_op = {
	NULL,
	do_get_info,
	do_got_info,
	do_free_info,
};

typedef struct {
	int read;
	int unread;
} MailFolderInfo;

/* Returns a MailFolderInfo struct or NULL on error */
static void
mail_get_info (const char *foldername, 
	       Bonobo_Listener listener)
{
	CORBA_Environment ev;
	struct _folder_info_msg *m;

	m = mail_msg_new (&get_info_op, NULL, sizeof (*m));

	g_print ("Folder: %s", foldername);
	m->foldername = g_strdup (foldername);

	CORBA_exception_init (&ev);
	m->listener = bonobo_object_dup_ref (listener, &ev); 
	CORBA_exception_free (&ev);

	e_thread_put (mail_thread_new, (EMsg *) m);
}

static void
impl_GNOME_Evolution_FolderInfo_getInfo (PortableServer_Servant servant,
					 const CORBA_char *foldername,
					 const Bonobo_Listener listener,
					 CORBA_Environment *ev)
{
	mail_get_info (foldername, listener);
}

static void
evolution_folder_info_class_init (EvolutionFolderInfoClass *klass)
{
	POA_GNOME_Evolution_FolderInfo__epv *epv = &klass->epv;

	parent_class = gtk_type_class (PARENT_TYPE);
	epv->getInfo = impl_GNOME_Evolution_FolderInfo_getInfo;
}

static void
evolution_folder_info_init (EvolutionFolderInfo *info)
{
}

BONOBO_X_TYPE_FUNC_FULL (EvolutionFolderInfo,
			 GNOME_Evolution_FolderInfo,
			 PARENT_TYPE,
			 evolution_folder_info);

static BonoboObject *
evolution_folder_info_factory_fn (BonoboGenericFactory *factory,
				  void *closure)
{
	EvolutionFolderInfo *info;

	info = gtk_type_new (evolution_folder_info_get_type ());
	return BONOBO_OBJECT (info);
}

void 
evolution_folder_info_factory_init (void)
{
	BonoboGenericFactory *factory;
	
	factory = bonobo_generic_factory_new (FOLDER_INFO_IID,
					      evolution_folder_info_factory_fn,
					      NULL);

	if (factory == NULL) {
		g_warning ("Error starting FolderInfo");
		return;
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}
