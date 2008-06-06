/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2002-2004 Novell, Inc. */

#ifndef __EXCHANGE_DELEGATES_H__
#define __EXCHANGE_DELEGATES_H__

#include <exchange-types.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void exchange_delegates (ExchangeAccount *account, GtkWidget *parent);
const char *email_look_up (const char *delegate_legacy, ExchangeAccount *account);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_DELEGATES_CONTROL_H__ */
