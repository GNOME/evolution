/*
 * folder-info.c: Implementation of GNOME_Evolution_FolderInfo
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "folder-info.h"

#include "Mailer.h"

#include <glib.h>

#include <bonobo/bonobo-xobject.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>

#include "mail.h"
#include "mail-mt.h"
#include "mail-tools.h"

#include <camel/camel-folder.h>
#include <camel/camel-exception.h>

#define FOLDER_INFO_IID "OAFIID:GNOME_Evolution_FolderInfo_Factory"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

static GSList *folder_infos;

typedef struct _EvolutionFolderInfo EvolutionFolderInfo;
typedef struct _EvolutionFolderInfoClass EvolutionFolderInfoClass;

struct _EvolutionFolderInfo {
	BonoboXObject parent;

	BonoboPropertyBag *pb;
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

gboolean ready;

static char *
do_describe_info (struct _mail_msg *mm, gint complete)
{
        return g_strdup (_("Getting Folder Information"));
}

static void
do_get_info (struct _mail_msg *mm)
{
	struct _folder_info_msg *m = (struct _folder_info_msg *) mm;
	CamelFolder *folder;
	
	folder = mail_tool_uri_to_folder (m->foldername, 0, NULL);
	if (folder) {
		m->read = camel_folder_get_message_count (folder);
		m->unread = camel_folder_get_unread_message_count (folder);
	}
}

static void
do_got_info (struct _mail_msg *mm)
{
	struct _folder_info_msg *m = (struct _folder_info_msg *) mm;
	CORBA_Environment ev;
	CORBA_any a;
	GNOME_Evolution_FolderInfo_MessageCount count;
	
/* 	g_print ("You've got mail: %d, %d\n", m->read, m->unread); */
	
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
	
	bonobo_object_release_unref (m->listener, NULL);
	g_free (m->foldername);
}

struct _mail_msg_op get_info_op = {
        do_describe_info,
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
	
/* 	g_print ("Folder: %s", foldername); */
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

#if 0
static void
destroy (GtkObject *object)
{
	EvolutionFolderInfo *info = (EvolutionFolderInfo *) object;
	
	bonobo_object_unref (BONOBO_OBJECT (info->pb));
}
#endif

static void
evolution_folder_info_class_init (EvolutionFolderInfoClass *klass)
{
	POA_GNOME_Evolution_FolderInfo__epv *epv = &klass->epv;
	
	parent_class = g_type_class_ref(PARENT_TYPE);
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

enum {
	PROP_FOLDER_INFO_READY
};

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg *arg,
	  guint arg_id,
	  CORBA_Environment *ev,
	  gpointer user_data)
{
	switch (arg_id) {
	case PROP_FOLDER_INFO_READY:
		ready = BONOBO_ARG_GET_BOOLEAN (arg);
		break;
	default:
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		break;
	}
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg *arg,
	  guint arg_id,
	  CORBA_Environment *ev,
	  gpointer user_data)
{
	switch (arg_id) {
	case PROP_FOLDER_INFO_READY:
		BONOBO_ARG_SET_BOOLEAN (arg, ready);
		break;
	default:
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		break;
	}
}

static BonoboObject *
evolution_folder_info_factory_fn (BonoboGenericFactory *factory,
				  const char *id,
				  void *closure)
{
	EvolutionFolderInfo *info;
	BonoboPropertyBag *pb;
	
	info = g_object_new (evolution_folder_info_get_type (), NULL);
	pb = bonobo_property_bag_new (get_prop, set_prop, info);
	info->pb = pb;
	/* Add properties */
	bonobo_property_bag_add (pb, "folder-info-ready",
				 PROP_FOLDER_INFO_READY,
				 BONOBO_ARG_BOOLEAN, NULL, FALSE,
				 BONOBO_PROPERTY_READABLE |
				 BONOBO_PROPERTY_WRITEABLE);
	
	bonobo_object_add_interface (BONOBO_OBJECT (info), BONOBO_OBJECT (pb));
	
	/* Add to the folder info list so we can get at them all afterwards */
	folder_infos = g_slist_append (folder_infos, info);
	
	return BONOBO_OBJECT (info);
}

gboolean
evolution_folder_info_factory_init (void)
{
	BonoboGenericFactory *factory;
	
	folder_infos = NULL;
	ready = FALSE;
	
	factory = bonobo_generic_factory_new (FOLDER_INFO_IID,
					      evolution_folder_info_factory_fn,
					      NULL);
	
	if (factory == NULL) {
		g_warning ("Error starting FolderInfo");
		return FALSE;
	}
	
	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
	return TRUE;
}

void
evolution_folder_info_notify_ready (void)
{
	GSList *p;
	
	ready = TRUE;
	
	for (p = folder_infos; p; p = p->next) {
		EvolutionFolderInfo *info = p->data;
		Bonobo_PropertyBag bag;

		bag = (Bonobo_PropertyBag)bonobo_object_corba_objref(BONOBO_OBJECT(info->pb));
		bonobo_pbclient_set_boolean("folder-info-ready", ready, NULL);
	}
}
