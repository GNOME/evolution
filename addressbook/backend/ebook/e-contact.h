/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
 */

#ifndef __E_CONTACT_H__
#define __E_CONTACT_H__

#include <time.h>
#include <glib-object.h>
#include <stdio.h>
#include <ebook/e-vcard.h>

#define E_TYPE_CONTACT            (e_contact_get_type ())
#define E_CONTACT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT, EContact))
#define E_CONTACT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT, EContactClass))
#define E_IS_CONTACT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT))
#define E_IS_CONTACT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CONTACT))
#define E_CONTACT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CONTACT, EContactClass))

typedef struct _EContact EContact;
typedef struct _EContactClass EContactClass;
typedef struct _EContactPrivate EContactPrivate;

typedef enum {

	E_CONTACT_UID = 1,     	 /* string field */
	E_CONTACT_FILE_AS,     	 /* string field */

	/* Name fields */
	E_CONTACT_FULL_NAME,   	 /* string field */
	E_CONTACT_GIVEN_NAME,  	 /* synthetic string field */
	E_CONTACT_FAMILY_NAME, 	 /* synthetic string field */
	E_CONTACT_NICKNAME,    	 /* string field */

	/* Email fields */
	E_CONTACT_EMAIL_1,     	 /* synthetic string field */
	E_CONTACT_EMAIL_2,     	 /* synthetic string field */
	E_CONTACT_EMAIL_3,     	 /* synthetic string field */

	E_CONTACT_MAILER,        /* string field */

	/* Address Labels */
	E_CONTACT_ADDRESS_LABEL_HOME,  /* synthetic string field */
	E_CONTACT_ADDRESS_LABEL_WORK,  /* synthetic string field */
	E_CONTACT_ADDRESS_LABEL_OTHER, /* synthetic string field */

	/* Phone fields */
	E_CONTACT_PHONE_ASSISTANT,
	E_CONTACT_PHONE_BUSINESS,
	E_CONTACT_PHONE_BUSINESS_2,
	E_CONTACT_PHONE_BUSINESS_FAX,
	E_CONTACT_PHONE_CALLBACK,
	E_CONTACT_PHONE_CAR,
	E_CONTACT_PHONE_COMPANY,
	E_CONTACT_PHONE_HOME,
	E_CONTACT_PHONE_HOME_2,
	E_CONTACT_PHONE_HOME_FAX,
	E_CONTACT_PHONE_ISDN,
	E_CONTACT_PHONE_MOBILE,
	E_CONTACT_PHONE_OTHER,
	E_CONTACT_PHONE_OTHER_FAX,
	E_CONTACT_PHONE_PAGER,
	E_CONTACT_PHONE_PRIMARY,
	E_CONTACT_PHONE_RADIO,
	E_CONTACT_PHONE_TELEX,
	E_CONTACT_PHONE_TTYTDD,

	/* Organizational fields */
	E_CONTACT_ORG,        	 /* string field */
	E_CONTACT_ORG_UNIT,   	 /* string field */
	E_CONTACT_OFFICE,     	 /* string field */
	E_CONTACT_TITLE,      	 /* string field */
	E_CONTACT_ROLE,       	 /* string field */
	E_CONTACT_MANAGER,    	 /* string field */
	E_CONTACT_ASSISTANT,  	 /* string field */

	/* Web fields */
	E_CONTACT_HOMEPAGE_URL,  /* string field */
	E_CONTACT_BLOG_URL,      /* string field */

	/* Contact categories */
	E_CONTACT_CATEGORIES,    /* string field */

	/* Collaboration fields */
	E_CONTACT_CALENDAR_URI,  /* string field */
	E_CONTACT_FREEBUSY_URL,  /* string field */
	E_CONTACT_ICS_CALENDAR,  /* string field */

	/* misc fields */
	E_CONTACT_SPOUSE,        /* string field */
	E_CONTACT_NOTE,          /* string field */

	/* fields used for describing contact lists.  a contact list
	   is just a contact with _IS_LIST set to true.  the members
	   are listed in the _EMAIL field. */
	E_CONTACT_IS_LIST,             /* boolean field */
	E_CONTACT_LIST_SHOW_ADDRESSES, /* boolean field */

	/* Instant Messaging fields */
	E_CONTACT_IM_AIM,     	 /* Multi-valued */
	E_CONTACT_IM_JABBER,  	 /* Multi-valued */
	E_CONTACT_IM_YAHOO,   	 /* Multi-valued */
	E_CONTACT_IM_MSN,     	 /* Multi-valued */
	E_CONTACT_IM_ICQ,     	 /* Multi-valued */

	/* Address fields */
	E_CONTACT_ADDRESS,       /* Multi-valued structured (EContactAddress) */
	E_CONTACT_ADDRESS_HOME,  /* synthetic structured field (EContactAddress) */
	E_CONTACT_ADDRESS_WORK,  /* synthetic structured field (EContactAddress) */
	E_CONTACT_ADDRESS_OTHER, /* synthetic structured field (EContactAddress) */

	E_CONTACT_CATEGORY_LIST, /* multi-valued */

	/* Photo/Logo */
	E_CONTACT_PHOTO,       	 /* structured field (EContactPhoto) */
	E_CONTACT_LOGO,       	 /* structured field (EContactPhoto) */

	E_CONTACT_NAME,        	 /* structured field (EContactName) */
	E_CONTACT_EMAIL,       	 /* Multi-valued */

	E_CONTACT_WANTS_HTML,    /* boolean field */

	E_CONTACT_BIRTH_DATE,    /* structured field (EContactDate) */
	E_CONTACT_ANNIVERSARY,   /* structured field (EContactDate) */

	E_CONTACT_FIELD_LAST,

	/* useful constants */
	E_CONTACT_LAST_SIMPLE_STRING = E_CONTACT_NOTE,
	E_CONTACT_FIRST_PHONE_ID     = E_CONTACT_PHONE_ASSISTANT,
	E_CONTACT_LAST_PHONE_ID      = E_CONTACT_PHONE_TTYTDD,
	E_CONTACT_FIRST_EMAIL_ID     = E_CONTACT_EMAIL_1,
	E_CONTACT_LAST_EMAIL_ID      = E_CONTACT_EMAIL_3,
	E_CONTACT_FIRST_ADDRESS_ID   = E_CONTACT_ADDRESS_HOME,
	E_CONTACT_LAST_ADDRESS_ID    = E_CONTACT_ADDRESS_OTHER,
	E_CONTACT_FIRST_LABEL_ID     = E_CONTACT_ADDRESS_LABEL_HOME,
	E_CONTACT_LAST_LABEL_ID      = E_CONTACT_ADDRESS_LABEL_OTHER

} EContactField;

typedef struct {
	char *family;
	char *given;
	char *additional;
	char *prefixes;
	char *suffixes;
} EContactName;

typedef struct {
	int length;
	char *data;
} EContactPhoto;

typedef struct {
	char *address_format; /* the two letter country code that
				 determines the format/meaning of the
				 following fields */
	char *po;
	char *ext;
	char *street;
	char *locality;
	char *region;
	char *code;
	char *country;
} EContactAddress;

typedef struct {
	int year;
	int month;
	int day;
} EContactDate;

struct _EContact {
	EVCard parent;

	EContactPrivate *priv;
};

struct _EContactClass {
	EVCardClass parent_class;

	/* Padding for future expansion */
	void (*_ebook_reserved0) (void);
	void (*_ebook_reserved1) (void);
	void (*_ebook_reserved2) (void);
	void (*_ebook_reserved3) (void);
	void (*_ebook_reserved4) (void);
};

GType                   e_contact_get_type (void);

EContact*               e_contact_new              (void);
EContact*               e_contact_new_from_vcard   (const char *vcard);

EContact*               e_contact_duplicate        (EContact *contact);

gpointer                e_contact_get              (EContact *contact, EContactField field_id);
const gpointer          e_contact_get_const        (EContact *contact, EContactField field_id);
void                    e_contact_set              (EContact *contact, EContactField field_id, gpointer value);

/* misc functions for structured values */
EContactDate           *e_contact_date_new         (void);
EContactDate           *e_contact_date_from_string (const char *str);
char                   *e_contact_date_to_string   (EContactDate *dt);

EContactName           *e_contact_name_new         (void);
char                   *e_contact_name_to_string   (const EContactName *name);
EContactName           *e_contact_name_from_string (const char *name_str);
EContactName           *e_contact_name_copy        (EContactName *name);


/* destructors for structured values */
void                    e_contact_date_free        (EContactDate *date);
void                    e_contact_name_free        (EContactName *name);
void                    e_contact_photo_free       (EContactPhoto *photo);
void                    e_contact_address_free     (EContactAddress *address);


const char*             e_contact_field_name       (EContactField field_id);
const char*             e_contact_pretty_name      (EContactField field_id);
EContactField           e_contact_field_id         (const char *field_name);

#endif /* __E_CONTACT_H__ */
