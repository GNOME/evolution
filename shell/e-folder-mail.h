#ifndef _E_FOLDER_MAIL_H_
#define _E_FOLDER_MAIL_H_

#include "e-folder.h"

#define E_FOLDER_MAIL_TYPE        (e_folder_mail_get_type ())
#define E_FOLDER_MAIL(o)          (GTK_CHECK_CAST ((o), E_FOLDER_MAIL_TYPE, EFolderMail))
#define E_FOLDER_MAIL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_FOLDER_MAIL_TYPE, EFolderMailClass))
#define E_IS_FOLDER_MAIL(o)       (GTK_CHECK_TYPE ((o), E_FOLDER_MAIL_TYPE))
#define E_IS_FOLDER_MAIL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_FOLDER_MAIL_TYPE))

typedef struct {
	EFolder parent;
} EFolderMail;
 
typedef struct {
	EFolderClass parent;
} EFolderMailClass;

GtkType     e_folder_mail_get_type (void);
EFolder    *e_folder_mail_new      (const char *uri, const char *name, const char *desc,
				    const char *home_page, const char *view_name);

#endif /* _E_FOLDER_MAIL_H_ */




