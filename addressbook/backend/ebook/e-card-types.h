/*
 * Authors:
 *   Arturo Espinosa
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_TYPES_H__
#define __E_CARD_TYPES_H__
#if 0
typedef enum
{
	PROP_NONE = 0, /* Must always be the first, with value = 0. */
	PROP_CARD = 1,
	PROP_FNAME = 2,
	PROP_NAME = 3,
	PROP_PHOTO = 4,
	PROP_BDAY = 5,
	PROP_DELADDR_LIST = 6,
	PROP_DELADDR = 7,
	PROP_DELLABEL_LIST = 8,
	PROP_DELLABEL = 9,
	PROP_PHONE_LIST = 10,
	PROP_PHONE = 11,
	PROP_EMAIL_LIST = 12,
	PROP_EMAIL = 13,
	PROP_MAILER = 14,
	PROP_TIMEZN = 15,
	PROP_GEOPOS = 16,
	PROP_TITLE = 17,
	PROP_ROLE = 18,
	PROP_LOGO = 19,
	PROP_AGENT = 20,
	PROP_ORG = 21,
	PROP_COMMENT = 22,
	PROP_REV = 23,
	PROP_SOUND = 24,
	PROP_URL = 25,
	PROP_UID = 26,
	PROP_VERSION = 27,
	PROP_KEY = 28,
	PROP_CATEGORIES = 29,
	PROP_XTENSION_LIST = 30,
	PROP_VALUE = 31,
	PROP_ENCODING = 32,
	PROP_QUOTED_PRINTABLE = 33,
	PROP_8BIT = 34,
	PROP_BASE64 = 35,
	PROP_LANG = 36,
	PROP_CHARSET = 37,
	PROP_LAST = 38 /* Must always be the last, with the gratest value. */
} ECardPropertyType;

typedef enum
{
	ENC_NONE = 0,
	ENC_BASE64 = 1,
	ENC_QUOTED_PRINTABLE = 2,
	ENC_8BIT = 3,
	ENC_7BIT = 4,
	ENC_LAST = 5
} ECardEncodeType;

typedef enum 
{
	VAL_NONE = 0,
	VAL_INLINE = 1,
	VAL_CID = 2,
	VAL_URL = 3,
	VAL_LAST = 4
} ECardValueType;

typedef enum {
	PHOTO_GIF,  PHOTO_CGM,   PHOTO_WMF,  PHOTO_BMP, PHOTO_MET, PHOTO_PMB, 
	PHOTO_DIB,  PHOTO_PICT,  PHOTO_TIFF, PHOTO_PS,  PHOTO_PDF, PHOTO_JPEG,
	PHOTO_MPEG, PHOTO_MPEG2, PHOTO_AVI,  PHOTO_QTIME
} ECardPhotoType;

typedef struct {
	gboolean           used;
	ECardPropertyType  type;
	ECardEncodeType    encode;
	ECardValueType     value;
	char              *charset;
	char              *lang;
	GList             *xtension;
	
	void              *user_data;
} CardProperty;

typedef struct {
	char *name;
	char *data;
} CardXAttribute;

typedef struct {
	CardProperty      prop;
	
	char             *name;
	char             *data;
} ECardXProperty;

typedef struct {
	CardProperty      prop;
	
	GList            *l;
} ECardList;

#endif

/* IDENTIFICATION PROPERTIES */


typedef struct {
	char            *prefix;        /* Mr. */
	char            *given;         /* John */
	char            *additional;    /* Quinlan */
	char            *family;        /* Public */
	char            *suffix;        /* Esq. */
} ECardName;

#if 0
typedef struct {
	CardProperty prop;
	
	ECardPhotoType type;
	guint size;
	char *data;

} ECardPhoto;
#endif

typedef struct {
	int year;
	int month;
	int day;
} ECardDate;

/* TELECOMMUNICATIONS ADDRESSING PROPERTIES */

typedef enum {
	E_CARD_PHONE_PREF  = 1 << 0,
	E_CARD_PHONE_WORK  = 1 << 1,
	E_CARD_PHONE_HOME  = 1 << 2,
	E_CARD_PHONE_VOICE = 1 << 3,
	E_CARD_PHONE_FAX   = 1 << 4,
	E_CARD_PHONE_MSG   = 1 << 5,
	E_CARD_PHONE_CELL  = 1 << 6,
	E_CARD_PHONE_PAGER = 1 << 7,
	E_CARD_PHONE_BBS   = 1 << 8,
	E_CARD_PHONE_MODEM = 1 << 9,
	E_CARD_PHONE_CAR   = 1 << 10,
	E_CARD_PHONE_ISDN  = 1 << 11,
	E_CARD_PHONE_VIDEO = 1 << 12 
} ECardPhoneFlags;

typedef struct {
	ECardPhoneFlags  flags;
	char            *number;
} ECardPhone;

#if 0

typedef struct {
	int sign;      /* 1 or -1 */
	int hours;     /* Mexico General is at -6:00 UTC */
	int mins;      /* sign -1, hours 6, mins 0 */
} ECardTimeZone;

typedef struct {
	CardProperty prop;
	
	float lon;
	float lat;
} ECardGeoPos;

#endif
/* DELIVERY ADDRESSING PROPERTIES */

typedef enum {
	ADDR_HOME   = 1 << 0, 
	ADDR_WORK   = 1 << 1,
	ADDR_POSTAL = 1 << 2, 
	ADDR_PARCEL = 1 << 3, 
	ADDR_DOM    = 1 << 4,
	ADDR_INTL   = 1 << 5 
} ECardAddressFlags;

typedef struct {
	ECardAddressFlags  flags;

	char           *po;
	char           *ext;
	char           *street;
	char           *city;
	char           *region;
	char           *code;
	char           *country;
} ECardDeliveryAddress;

#if 0
typedef struct {
	ECardAddressFlags  flags;
	char           *data;
} ECardAddrLabel;

/* ORGANIZATIONAL PROPERTIES */

typedef struct {
	char *name;
	char *unit1;
	char *unit2;
	char *unit3;
	char *unit4;
} ECardOrg;

typedef enum {
	SOUND_AIFF,
	SOUND_PCM, 
	SOUND_WAVE, 
	SOUND_PHONETIC
} ECardSoundType;

typedef enum {
	KEY_X509, 
	KEY_PGP
} ECardKeyType;

typedef struct {
	int       utc;
	struct tm tm;
} ECardRev;


typedef struct {
	ECardSoundType  type;
	unsigned int    size;
	char           *data;
} ECardSound;

typedef struct {
	CardProperty  prop;
	
	ECardKeyType  type;
	char         *data;
} ECardKey;

#endif /* 0 */
#endif /* __E_CARD_TYPES_H__ */
