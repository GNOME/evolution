/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

/* IDENTIFICATION PROPERTIES */

typedef struct {
	char            *prefix;        /* Mr. */
	char            *given;         /* John */
	char            *additional;    /* Quinlan */
	char            *family;        /* Public */
	char            *suffix;        /* Esq. */
} ECardName;

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

/* DELIVERY ADDRESSING PROPERTIES */

typedef enum {
	E_CARD_ADDR_HOME   = 1 << 0, 
	E_CARD_ADDR_WORK   = 1 << 1,
	E_CARD_ADDR_POSTAL = 1 << 2, 
	E_CARD_ADDR_PARCEL = 1 << 3, 
	E_CARD_ADDR_DOM    = 1 << 4,
	E_CARD_ADDR_INTL   = 1 << 5 
} ECardAddressFlags;

typedef struct {
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
	ECardAddressFlags  flags;
	char              *data;
} ECardAddrLabel;

/* ARBITRARY PROPERTIES */

typedef struct {
	char *key;
	char *type;
	char *value;
} ECardArbitrary;

#endif /* __E_CARD_TYPES_H__ */
