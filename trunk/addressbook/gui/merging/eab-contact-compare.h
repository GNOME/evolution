/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-contact-compare.h
 *
 * Copyright (C) 2001,2002,2003 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifndef __EAB_CONTACT_COMPARE_H__
#define __EAB_CONTACT_COMPARE_H__

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

typedef enum {
	EAB_CONTACT_MATCH_NOT_APPLICABLE = 0,
	EAB_CONTACT_MATCH_NONE           = 1,
	EAB_CONTACT_MATCH_VAGUE          = 2,
	EAB_CONTACT_MATCH_PARTIAL        = 3,
	EAB_CONTACT_MATCH_EXACT          = 4
} EABContactMatchType;

typedef enum {
	EAB_CONTACT_MATCH_PART_NOT_APPLICABLE   = -1,
	EAB_CONTACT_MATCH_PART_NONE             = 0,
	EAB_CONTACT_MATCH_PART_GIVEN_NAME       = 1<<0,
	EAB_CONTACT_MATCH_PART_ADDITIONAL_NAME  = 1<<2,
	EAB_CONTACT_MATCH_PART_FAMILY_NAME      = 1<<3
} EABContactMatchPart;

typedef void (*EABContactMatchQueryCallback) (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure);

EABContactMatchType eab_contact_compare_name_to_string      (EContact *contact, const gchar *str);

EABContactMatchType eab_contact_compare_name_to_string_full (EContact *contact, const gchar *str,
							     gboolean allow_partial_matches,
							     gint *matched_parts, EABContactMatchPart *first_matched_part,
							     gint *matched_character_count);

EABContactMatchType eab_contact_compare_file_as   (EContact *contact1, EContact *contact2);
EABContactMatchType eab_contact_compare_name      (EContact *contact1, EContact *contact2);
EABContactMatchType eab_contact_compare_nickname  (EContact *contact1, EContact *contact2);
EABContactMatchType eab_contact_compare_email     (EContact *contact1, EContact *contact2);
EABContactMatchType eab_contact_compare_address   (EContact *contact1, EContact *contact2);
EABContactMatchType eab_contact_compare_telephone (EContact *contact1, EContact *contact2);

EABContactMatchType eab_contact_compare           (EContact *contact1, EContact *contact2);

void                eab_contact_locate_match      (EContact *contact, EABContactMatchQueryCallback cb, gpointer closure);
void                eab_contact_locate_match_full (EBook *book, EContact *contact, GList *avoid, EABContactMatchQueryCallback cb, gpointer closure);



#endif /* __E_CONTACT_COMPARE_H__ */

