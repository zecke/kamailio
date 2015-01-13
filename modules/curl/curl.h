/*
 * header file of curl.c
 *
 * Copyright (C) 2008 Juha Heinanen
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Kamailio curl :: Core include file
 * \ingroup curl
 * Module: \ref curl
 */

#ifndef CURL_H
#define CURL_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"

extern int default_connection_timeout;

/* Curl  stream object  */
typedef struct {
	char		*buf;
	size_t		curr_size;
	size_t		pos;
} http_res_stream_t;


/*! Predefined connection objects */
typedef struct _curl_con
{
	str name;			/*!< Connection name */
	unsigned int conid;		/*!< Connection ID */
	str url;			/*!< The URL without schema (host + base URL)*/
	str schema;			/*!< The URL schema */
	str username;			/*!< The username to use for auth */
	str password;			/*!< The password to use for auth */
	unsigned int port;		/*!< The port to connect to */
	http_res_stream_t *stream;	/*!< Curl stream */
	struct _curl_con *next;		/*!< next connection */
} curl_con_t;

#endif /* CURL_H */
