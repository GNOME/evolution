/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Arturo Espinosa
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_TYPES_H__
#define __E_CARD_TYPES_H__

/* IDENTIFICATION PROPERTIES */

typedef struct {
	gint  ref_count;
	char *prefix;        /* Mr. */
	char *given;         /* John */
	char *additional;    /* Quinlan */
	char *family;        /* Public */
	char *suffix;        /* Esq. */
} ECardName;

typedef struct {
	int year;
	int month;
	int day;
} ECardDate;

/* TELECOMMUNICATIONS ADDRESSING PROPERTIES */

typedef enum {
	E_CARD_PHONE_PREF      = 1 << 0,
	E_CARD_PHONE_WORK      = 1 << 1,
	E_CARD_PHONE_HOME      = 1 << 2,
	E_CARD_PHONE_VOICE     = 1 << 3,
	E_CARD_PHONE_FAX       = 1 << 4,
	E_CARD_PHONE_MSG       = 1 << 5,
	E_CARD_PHONE_CELL      = 1 << 6,
	E_CARD_PHONE_PAGER     = 1 << 7,
	E_CARD_PHONE_BBS       = 1 << 8,
	E_CARD_PHONE_MODEM     = 1 << 9,
	E_CARD_PHONE_CAR       = 1 << 10,
	E_CARD_PHONE_ISDN      = 1 << 11,
	E_CARD_PHONE_VIDEO     = 1 << 12,
	E_CARD_PHONE_ASSISTANT = 1 << 13,
	E_CARD_PHONE_CALLBACK  = 1 << 14,
	E_CARD_PHONE_RADIO     = 1 << 15,
	E_CARD_PHONE_TELEX     = 1 << 16,
	E_CARD_PHONE_TTYTDD    = 1 << 17,
} ECardPhoneFlags;

typedef struct {
	gint ref_count;
	ECardPhoneFlags  flags;
	char            *number;
} ECardPhone;

/* DELIVERY ADDRESSING PROPERTIES */

typedef enum {
	E_CARD_ADDR_HOME    = 1 << 0, 
	E_CARD_ADDR_WORK    = 1 << 1,
	E_CARD_ADDR_POSTAL  = 1 << 2, 
	E_CARD_ADDR_MASK    = 7,
	E_CARD_ADDR_PARCEL  = 1 << 3, 
	E_CARD_ADDR_DOM     = 1 << 4,
	E_CARD_ADDR_INTL    = 1 << 5, 
	E_CARD_ADDR_DEFAULT = 1 << 6
} ECardAddressFlags;

typedef struct {
	gint ref_count;
	ECardAddressFlags  flags;

	char     	  *po;
	char     	  *ext;
	char     	  *street;
	char     	  *city;
	char     	  *region;
	char     	  *code;
	char     	  *country;
} ECardDeliveryAddress;

typedef struct {
	gint ref_count;
	ECardAddressFlags  flags;
	char              *data;
} ECardAddrLabel;

/* ARBITRARY PROPERTIES */

typedef struct {
	gint ref_count;
	char *key;
	char *type;
	char *value;
} ECardArbitrary;

#endif /* __E_CARD_TYPES_H__ */
