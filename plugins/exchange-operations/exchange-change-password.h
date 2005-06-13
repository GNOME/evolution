/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2004 Novell, Inc. */

#ifndef __EXCHANGE_CHANGE_PASSWORD_H__
#define __EXCHANGE_CHANGE_PASSWORD_H__

#include <exchange/exchange-types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

char *exchange_get_new_password (const char *existing_password,
				 gboolean    voluntary);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_CHANGE_PASSWORD_H__ */
