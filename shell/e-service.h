/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.h : abstract class for the Evolution services */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Miguel de Icaza (miguel@helixcode.com)
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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


/* 
 * this class represents a service, that is, an object
 * we can get e-folders from.
 * 
 * In the case of the mail, it represents :
 *  - a store to get mail folder from (an imap server, 
 *  an mbox toplevel directory
 *            or
 *  - a mail transport thanks to which you can send
 *    mail from. 
 *
 * This class may probably handle the connection
 * operations, and probably allow the some kind of
 * configuration of the authentication methods
 * used. 
 * 
 *
 * NOTE : this class shares a lot of code and properties
 * with the folder class. It may be a good idea to determine
 * exactely if it would be useful to make both classes 
 * be children of another parent abstract class. 
 * for the moment, we don't really show the service to 
 * the user in the UI, so that there is not an urgent 
 * need to create the abstract parent class.
 * 
 *  - ber.
 * 
 */


#ifndef _E_SERVICE_H_
#define _E_SERVICE_H_

#include "eshell-types.h"
#include <gtk/gtkobject.h>

#define E_SERVICE_TYPE        (e_service_get_type ())
#define E_SERVICE(o)          (GTK_CHECK_CAST ((o), E_SERVICE_TYPE, EService))
#define E_SERVICE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SERVICE_TYPE, EServiceClass))
#define E_IS_SERVICE(o)       (GTK_CHECK_TYPE ((o), E_SERVICE_TYPE))
#define E_IS_SERVICE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SERVICE_TYPE))


typedef enum {
	E_SERVICE_MAIL        = 1 << 0, 
	E_SERVICE_CONTACTS    = 1 << 1, 
	E_SERVICE_CALENDAR    = 1 << 2,
	E_SERVICE_TASKS       = 1 << 3,
	E_SERVICE_OTHER       = 1 << 4
} EServiceType;

 struct _EService {

	GtkObject parent_object;

	EFolder *root_efolder;  /* a service may have a root EFolder */

	/*
	 * General properties
	 */
	char *uri;		/* Location */
	char *name;		/* Short name */
	char *desc;	        /* Full description */
	char *home_page;	/* Home page for this service */
       
	EServiceType type;      /* type of the service  */

	
};
 
typedef struct {
	GtkObjectClass parent_class;
	
} EServiceClass;

GtkType     e_service_get_type           (void);
void        e_service_construct          (EService *eservice, 
					  EServiceType type,
					  const char *uri, 
					  const char *name,
					  const char *desc, 
					  const char *home_page);
EService    *e_service_new               (EServiceType type,
					  const char *uri, 
					  const char *name,
					  const char *desc, 
					  const char *home_page);

EFolder    *e_service_get_root_efolder   (EService *eservice);

void        e_service_set_uri            (EService *eservice, 
					  const char *uri);
const char *e_service_get_uri            (EService *eservice);

void        e_service_set_description    (EService *eservice, 
					  const char *desc);
const char *e_service_get_description    (EService *eservice);

void        e_service_set_home_page      (EService *eservice, 
					  const char *desc);
const char *e_service_get_home_page      (EService *eservice);

const char *e_service_get_name           (EService *eservice);
void        e_service_set_name           (EService *eservice, 
					  const char *name);


const char *e_service_get_type_name      (EService *eservice);

#endif /* _E_SERVICE_H_ */




