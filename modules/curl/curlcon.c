/*
 * Copyright (C) 2015 Olle E. Johansson, Edvina AB
 *
 * Based on code from sqlops:
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "curl.h"
#include "curlcon.h"
#include "../../hashes.h"
#include "../../dprint.h"

curl_con_t *_curl_con_root = NULL;

/* Forward declaration */
int curl_init_con(str *name, str *schema, str *username, str *password, str *url);


/*! Find CURL connection by name
 */
curl_con_t* curl_get_connection(str *name)
{
	curl_con_t *cc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);

	cc = _curl_con_root;
	while(cc)
	{
		if(conid==cc->conid && cc->name.len==name->len
				&& strncmp(cc->name.s, name->s, name->len)==0)
			return cc;
		cc = cc->next;
	}
	return NULL;
}


/*! Parse the curlcon module parameter
 *
 *	Syntax:
 *		name => proto://user:password@server/url/url
 *		name => proto://server/url/url
 *
 *		the url is very much like CURLs syntax
 *		the url is a base url where you can add local address
 *
 *
 */
int curl_parse_param(char *val)
{
	str name;
	str schema;
	str url;
	str username;
	str secret;
	str in;
	char *p;
	char *u;

	username.len = 0;
	secret.len = 0;
	LM_INFO("curl modparam parsing starting\n");
	LM_DBG("modparam curlcon: %s\n", val);

	/* parse: name=>http_url*/
	in.s = val;
	in.len = strlen(in.s);
	p = in.s;

	/* Skip white space */
	while(p < in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r')) {
		p++;
	}
	if(p > in.s+in.len || *p=='\0') {
		goto error;
	}

	/* This is the connection name */
	name.s = p;
	/* Skip to whitespace */
	while(p < in.s + in.len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r') {
			break;
		}
		p++;
	}
	if(p > in.s+in.len || *p=='\0') {
		goto error;
	}
	name.len = p - name.s;
	if(*p != '=')
	{
		/* Skip whitespace */
		while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r')) {
			p++;
		}
		if(p>in.s+in.len || *p=='\0' || *p!='=') {
			goto error;
		}
	}
	p++;
	if(*p != '>') {
		goto error;
	}
	p++;
	/* Skip white space again */
	while(p < in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r')) {
		p++;
	}
	schema.s = p;
	/* Skip to colon ':' */
	while(p < in.s + in.len)
	{
		if(*p == ':') {
			break;
		}
		p++;
	}
	if(*p != ':') {
		goto error;
	}
	schema.len = p - schema.s;
	p++;	/* Skip the colon */
	/* Skip two slashes */
	if(*p != '/') {
		goto error;
	}
	p++;
	if(*p != '/') {
		goto error;
	}
	p++;
	/* We are now at the first character after :// */
	url.s = p;
	url.len = in.len + (int)(in.s - p);
	u = p;

	/* Now check if there is a @ character. If so, we need to parse the username
	   and password */
	/* Skip to colon '@' */
	while(p < in.s + in.len)
	{
		if(*p == '@') {
			break;
		}
		p++;
	}
	if (*p == '@') {
		/* We have a username and possibly password - parse them out */
		username.s = u;
		while (u < p) {
			if (*u == ':') {
				break;
			}
			u++;
		}
		username.len = u - username.s;

		/* We either have a : or a @ */
		if (*u == ':') {
			u++;
			/* Go look for password */
			secret.s = u;
			while (u < p) {
				u++;
			}
			secret.len = u - secret.s;
		}
		p++;	/* Skip the colon */
		url.s = p;
		url.len = in.len + (int)(in.s - p);
	}

	LM_DBG("cname: [%.*s] url: [%.*s] username [%.*s] secret [%.*s]\n", name.len, name.s, url.len, url.s, username.len, username.s, secret.len, secret.s);

	return curl_init_con(&name, &schema, &username, &secret, &url);
	return 0;
error:
	LM_ERR("invalid curl parameter [%.*s] at [%d]\n", in.len, in.s,
			(int)(p-in.s));
	return -1;
}

/*! Init connection structure 
 */
int curl_init_con(str *name, str *schema, str *username, str *password, str *url)
{
	curl_con_t *cc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);

	cc = _curl_con_root;
	while(cc)
	{
		if(conid==cc->conid && cc->name.len == name->len
				&& strncmp(cc->name.s, name->s, name->len)==0)
		{
			LM_ERR("duplicate Curl connection name\n");
			return -1;
		}
		cc = cc->next;
	}

	cc = (curl_con_t*) pkg_malloc(sizeof(curl_con_t));
	if(cc == NULL)
	{
		LM_ERR("no pkg memory\n");
		return -1;
	}
	memset(cc, 0, sizeof(curl_con_t));
	cc->conid = conid;
	cc->name = *name;
	cc->username = *username;
	cc->password = *password;
	cc->schema = *schema;
	cc->url = *url;
	cc->next = _curl_con_root;
	_curl_con_root = cc;

	LM_ERR("CURL: Added connection [%.*s]\n", name->len, name->s);
	return 0;
}

