/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * Author:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
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
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "camel-arg.h"

int camel_argv_build(CamelArgV *tv)
{
	register guint32 tag;
	register int i;
	register CamelArg *a;
	int more = TRUE;

	for (i=0;i<CAMEL_ARGV_MAX;i++) {
		a = &tv->argv[i];

		if ( (tag = va_arg(tv->ap, guint32)) == 0) {
			more = FALSE;
			break;
		}

		a->tag = tag;

		switch((tag & CAMEL_ARG_TYPE)) {
		case CAMEL_ARG_OBJ:
			a->ca_object = va_arg(tv->ap, void *);
			break;
		case CAMEL_ARG_INT:
			a->ca_int = va_arg(tv->ap, int);
			break;
		case CAMEL_ARG_DBL:
			a->ca_double = va_arg(tv->ap, double);
			break;
		case CAMEL_ARG_STR:
			a->ca_str = va_arg(tv->ap, char *);
			break;
		case CAMEL_ARG_PTR:
			a->ca_ptr = va_arg(tv->ap, void *);
			break;
		case CAMEL_ARG_BOO:
			a->ca_int = va_arg(tv->ap, int) != 0;
			break;
		default:
			printf("Error, unknown type, truncating result\n");
			more = FALSE;
			goto fail;
		}

	}
fail:
	tv->argc = i;

	return more;
}

int camel_arggetv_build(CamelArgGetV *tv)
{
	register guint32 tag;
	register int i;
	register CamelArgGet *a;
	int more = TRUE;

	for (i=0;i<CAMEL_ARGV_MAX;i++) {
		a = &tv->argv[i];

		if ( (tag = va_arg(tv->ap, guint32)) == 0) {
			more = FALSE;
			break;
		}

		a->tag = tag;

		switch((tag & CAMEL_ARG_TYPE)) {
		case CAMEL_ARG_OBJ:
			a->ca_object = va_arg(tv->ap, void **);
			*a->ca_object = NULL;
			break;
		case CAMEL_ARG_INT:
		case CAMEL_ARG_BOO:
			a->ca_int = va_arg(tv->ap, int *);
			*a->ca_int = 0;
			break;
		case CAMEL_ARG_DBL:
			a->ca_double = va_arg(tv->ap, double *);
			*a->ca_double = 0.0;
			break;
		case CAMEL_ARG_STR:
			a->ca_str = va_arg(tv->ap, char **);
			*a->ca_str = NULL;
			break;
		case CAMEL_ARG_PTR:
			a->ca_ptr = va_arg(tv->ap, void **);
			*a->ca_ptr = NULL;
			break;
		default:
			printf("Error, unknown type, truncating result\n");
			more = FALSE;
			goto fail;
		}

	}
fail:
	tv->argc = i;

	return more;
}

