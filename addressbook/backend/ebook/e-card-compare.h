/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-card-compare.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
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

#ifndef __E_CARD_COMPARE_H__
#define __E_CARD_COMPARE_H__

#include "e-book.h"
#include "e-card.h"

typedef enum {
	E_CARD_MATCH_NOT_APPLICABLE = 0,
	E_CARD_MATCH_NONE           = 1,
	E_CARD_MATCH_VAGUE          = 2,
	E_CARD_MATCH_PARTIAL        = 3,
	E_CARD_MATCH_EXACT          = 4
} ECardMatchType;

typedef enum {
	E_CARD_MATCH_PART_NOT_APPLICABLE   = -1,
	E_CARD_MATCH_PART_NONE             = 0,
	E_CARD_MATCH_PART_GIVEN_NAME       = 1<<0,
	E_CARD_MATCH_PART_ADDITIONAL_NAME  = 1<<2,
	E_CARD_MATCH_PART_FAMILY_NAME      = 1<<3
} ECardMatchPart;

typedef void (*ECardMatchQueryCallback) (ECard *card, ECard *match, ECardMatchType type, gpointer closure);

ECardMatchType e_card_compare_name_to_string      (ECard *card, const gchar *str);

ECardMatchType e_card_compare_name_to_string_full (ECard *card, const gchar *str,
						   gboolean allow_partial_matches,
						   gint *matched_parts, ECardMatchPart *first_matched_part,
						   gint *matched_character_count);

ECardMatchType e_card_compare_name      (ECard *card1, ECard *card2);
ECardMatchType e_card_compare_nickname  (ECard *card1, ECard *card2);
ECardMatchType e_card_compare_email     (ECard *card1, ECard *card2);
ECardMatchType e_card_compare_address   (ECard *card1, ECard *card2);
ECardMatchType e_card_compare_telephone (ECard *card1, ECard *card2);

ECardMatchType e_card_compare           (ECard *card1, ECard *card2);

void           e_card_locate_match      (ECard *card, ECardMatchQueryCallback cb, gpointer closure);
void           e_card_locate_match_full (EBook *book, ECard *card, GList *avoid, ECardMatchQueryCallback cb, gpointer closure);



#endif /* __E_CARD_COMPARE_H__ */

