/*
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
