/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_FREE_FORM_EXP_H
#define E_MAIL_FREE_FORM_EXP_H

#include <glib.h>

#include <e-util/e-util.h>

G_BEGIN_DECLS

void		e_mail_free_form_exp_to_sexp	(EFilterElement *element,
						 GString *out,
						 EFilterPart *part);

G_END_DECLS

#endif /* E_MAIL_FREE_FORM_EXP_H */
