/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-searching-tokenizer.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
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
 * USA.
 */

#ifndef __E_SEARCHING_TOKENIZER_H__
#define __E_SEARCHING_TOKENIZER_H__

#include <glib.h>
#include <gtkhtml/htmltokenizer.h>

#define E_TYPE_SEARCHING_TOKENIZER        (e_searching_tokenizer_get_type ())
#define E_SEARCHING_TOKENIZER(o)          (GTK_CHECK_CAST ((o), E_TYPE_SEARCHING_TOKENIZER, ESearchingTokenizer))
#define E_SEARCHING_TOKENIZER_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_TYPE_SEARCHING_TOKENIZER, ESearchingTokenizerClass))
#define E_IS_SEARCHING_TOKENIZER(o)       (GTK_CHECK_TYPE ((o), E_TYPE_SEARCHING_TOKENIZER))
#define E_IS_SEARCHING_TOKENIZER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TYPE_SEARCHING_TOKENIZER))

typedef struct _ESearchingTokenizer ESearchingTokenizer;
typedef struct _ESearchingTokenizerClass ESearchingTokenizerClass;

struct _ESearchingTokenizerPrivate;

struct _ESearchingTokenizer {
	HTMLTokenizer parent;

	struct _ESearchingTokenizerPrivate *priv;
};

struct _ESearchingTokenizerClass {
	HTMLTokenizerClass parent_class;

	void (*match) (ESearchingTokenizer *);
};

GtkType e_searching_tokenizer_get_type (void);

HTMLTokenizer *e_searching_tokenizer_new (void);

/* For now, just a simple API */
void e_searching_tokenizer_set_search_string    (ESearchingTokenizer *, const gchar *);
void e_searching_tokenizer_set_case_sensitivity (ESearchingTokenizer *, gboolean is_case_sensitive);
gint e_searching_tokenizer_match_count          (ESearchingTokenizer *);


#endif /* __E_SEARCHING_TOKENIZER_H__ */

