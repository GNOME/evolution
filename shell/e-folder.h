/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */



#ifndef _E_FOLDER_H_
#define _E_FOLDER_H_


#include "eshell-types.h"
#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#define E_FOLDER_TYPE        (e_folder_get_type ())
#define E_FOLDER(o)          (GTK_CHECK_CAST ((o), E_FOLDER_TYPE, EFolder))
#define E_FOLDER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_FOLDER_TYPE, EFolderClass))
#define E_IS_FOLDER(o)       (GTK_CHECK_TYPE ((o), E_FOLDER_TYPE))
#define E_IS_FOLDER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_FOLDER_TYPE))

typedef enum {
	E_FOLDER_DND_AS_FORWARD,
	E_FOLDER_DND_AS_MOVE_COPY
} EFolderDragDropAction;

typedef enum {
	E_FOLDER_SUMMARY,
	E_FOLDER_MAIL,
	E_FOLDER_CONTACTS,
	E_FOLDER_CALENDAR,
	E_FOLDER_TASKS,
	E_FOLDER_OTHER
} EFolderType;

struct  _EFolder {
	GtkObject parent_object;

	EFolderType type;

	EService *eservice;     /* an Efolder should have an eservice */
  
	/*
	 * General properties
	 */
	char *uri;		/* Location */
	char *name;		/* Short name */
	char *desc;	        /* Full description */
	char *home_page;	/* Home page for this folder */

	/*
	 * Administration properties
	 */
	char *view_name;	/* View name */
};
 
typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Notifies views of visible changes in the Efolder
	 */
	void (*changed) (EFolder *efolder);
} EFolderClass;

GtkType     e_folder_get_type        (void);
void        e_folder_construct       (EFolder *efolder, EFolderType type,
				      const char *uri, const char *name,
				      const char *desc, const char *home_page,
				      const char *view_name);
EFolder    *e_folder_new             (EFolderType type,
				      const char *uri, const char *name,
				      const char *desc, const char *home_page,
				      const char *view_name);

void        e_folder_set_uri         (EFolder *efolder, const char *uri);
const char *e_folder_get_uri         (EFolder *efolder);

void        e_folder_set_description (EFolder *efolder, const char *desc);
const char *e_folder_get_description (EFolder *efolder);

void        e_folder_set_home_page   (EFolder *efolder, const char *desc);
const char *e_folder_get_home_page   (EFolder *efolder);

const char *e_folder_get_name        (EFolder *efolder);
void        e_folder_set_name        (EFolder *efolder, const char *name);

const char *e_folder_get_view_name   (EFolder *efolder);
void        e_folder_set_view_name   (EFolder *efolder, const char *view_name);

const char *e_folder_get_type_name   (EFolder *efolder);

EFolderType e_folder_get_folder_type (EFolder *efolder);

#endif /* _E_FOLDER_H_ */

