/*
 * ECard field type definitions.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#ifndef __E_CARD_FIELDS_H__
#define __E_CARD_FIELDS_H__

typedef struct {
	int year;
	int month;
	int day;
} ECardDate;

typedef struct {
	char *addr;

	char *desc;
	char *id;
} ECardEmail;

typedef struct {
	char *addr1;
	char *addr2;
	char *city;
	char *postcode;
	char *region;
	char *country;

	char *desc;
	char *id;
} ECardAddress;

typedef struct {
	char *phone;

	char *desc;
	char *id;
} ECardPhone;

typedef struct {
	char *url;

	char *desc;
	char *id;
} ECardURL;

#endif /* ! __E_CARD_FIELDS_H__ */
 
