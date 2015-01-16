/*
 * curl Module
 *
 * Based on part of the utils module and part
 * of the json-rpc-c module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2009 1&1 Internet AG
 * Copyright (C) 2013 Carsten Bock, ng-voice GmbH
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

/*! \file
 * \brief  Kamailio curl :: The module interface file
 * \ingroup curl
 */

/*! \defgroup curl Kamailio :: Module interface to Curl
 *
 * http://curl.haxx.se
 * A generic library for many protocols
 *
 *  curl_connect(connection, url, $avp)
 *  curl_connect(connection, url, content-type, data, $avp)
 *
 * 	$var(res) = curl_connect("anders", "/postlåda", "application/json", "{ ok, {200, ok}}", "$avp(gurka)");
 *
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../locking.h"
#include "../../script_cb.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "functions.h"
#include "curlcon.h"
#include "curlrpc.h"

MODULE_VERSION

/* Module parameter variables */
int default_connection_timeout = 4;

/* lock for configuration access */
static gen_lock_t *conf_lock = NULL;


/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_http_query_get(void** param, int param_no);
static int fixup_free_http_query_get(void** param, int param_no);
static int fixup_http_query_post(void** param, int param_no);
static int fixup_free_http_query_post(void** param, int param_no);

static int fixup_curl_connect(void** param, int param_no);
static int fixup_free_curl_connect(void** param, int param_no);
static int fixup_curl_connect_post(void** param, int param_no);
static int fixup_free_curl_connect_post(void** param, int param_no);

/* Wrappers for http_query to be defined later */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result);
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result);
static int w_curl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result);
static int w_curl_connect_post(struct sip_msg* _m, char* _con, char * _url, char* _result, char* _ctype, char* _data);

/* forward function */
static int curl_con_param(modparam_t type, void* val);
static int pv_parse_curlerror(pv_spec_p sp, str *in);
static int pv_get_curlerror(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/* Exported functions */
static cmd_export_t cmds[] = {
    {"http_query", (cmd_function)w_http_query, 2, fixup_http_query_get,
     fixup_free_http_query_get,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"http_query", (cmd_function)w_http_query_post, 3, fixup_http_query_post,
     fixup_free_http_query_post,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"curl_connect", (cmd_function)w_curl_connect, 3, fixup_curl_connect,
     fixup_free_curl_connect,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"curl_connect", (cmd_function)w_curl_connect_post, 5, fixup_curl_connect_post,
     fixup_free_curl_connect_post,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
};


/* Exported parameters */
static param_export_t params[] = {
    	{"default_connection_timeout", INT_PARAM, &default_connection_timeout},
	{"curlcon",  PARAM_STRING|USE_FUNC_PARAM, (void*)curl_con_param},
    	{0, 0, 0}
};

/*!
 * \brief Exported Pseudo variables
 */
static pv_export_t mod_pvs[] = {
    {{"curlerror", (sizeof("curlerror")-1)}, /* Curl error codes */
     PVT_OTHER, pv_get_curlerror, 0,
	pv_parse_curlerror, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "curl",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported functions */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    0,   	/* exported MI functions */
    mod_pvs,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    child_init /* per-child init function */
};

counter_handle_t connections;	/* Number of connection definitions */
counter_handle_t connok;	/* Successful Connection attempts */
counter_handle_t connfail;	/* Failed Connection attempts */



static int init_shmlock(void)
{
	return 0;
}


static void destroy_shmlock(void)
{
	;
}

/* Init counters */
static void curl_counter_init()
{
        counter_register(&connections, "curl", "connections", 0, 0, 0, "Counter of connection definitions (curlcon)", 0);
        counter_register(&connok, "curl", "connok", 0, 0, 0, "Counter of successful connections (200 OK)", 0);
        counter_register(&connfail, "curl", "connfail", 0, 0, 0, "Counter of failed connections (not 200 OK)", 0);
}


/* Module initialization function */
static int mod_init(void)
{
	LM_DBG("init curl module\n");

	/* Initialize curl */
	if (curl_global_init(CURL_GLOBAL_ALL)) {
		LM_ERR("curl_global_init failed\n");
		return -1;
	}

	if(curl_init_rpc() < 0)
        {
                LM_ERR("failed to register RPC commands\n");
                return -1;
        }

	if (init_shmlock() != 0) {
		LM_CRIT("cannot initialize shmlock.\n");
		return -1;
	}

	curl_counter_init();
	counter_add(connections, curl_connection_count());

	LM_DBG("init curl module done\n");
	return 0;
}


/* Child initialization function */
static int child_init(int rank)
{	
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN) {
		return 0; /* do nothing for the main process */
	}

    	return 0;
}


static void destroy(void)
{
	/* Cleanup curl */
	curl_global_cleanup();
	destroy_shmlock();
}



/**
 * parse curlcon module parameter
 */
int curl_con_param(modparam_t type, void *val)
{
	if(val == NULL) {
		goto error;
	}

	LM_DBG("**** CURL got modparam curlcon \n");
	return curl_parse_param((char*)val);
error:
	return -1;

}

/* Fixup functions */

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_get(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 2) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query_get(void** param, int param_no)
{
    if (param_no == 1) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 2) {
	return fixup_free_pvar_null(param, 1);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}


/*
 * Fix curl_connect params: connection(string/pvar) url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_curl_connect(void** param, int param_no)
{

    if ((param_no == 1) || (param_no == 2)) {
	/* We want char * strings */
	return 0;
	}
    if (param_no == 3) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Fix curl_connect params when posting (5 parameters): 
 *	connection, url, content-type, data, pvar
 */
static int fixup_curl_connect_post(void** param, int param_no)
{

    if (param_no == 1 || param_no == 2 || param_no == 3 || param_no == 4) {
	/* We want char * strings */
	/* At some point we need to allow pvars in the string. */
	return 0;
	}
    if (param_no == 5) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}


/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect_post(void** param, int param_no)
{
    if (param_no == 1 || param_no == 2 || param_no == 3 || param_no == 4) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 5) {
	return fixup_free_pvar_null(param, 5);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 5) {
	return fixup_free_pvar_null(param, 5);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Wrapper for Curl_connect (GET)
 */
static int w_curl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result) {
//	curl_con_query_url(struct sip_msg* _m, char *connection, char* _url, char* _result, const char *contenttype, char* _post)
	LM_DBG("**** Curl Connection %s URL %s Result var %s\n", _con, _url, _result);

	return curl_con_query_url(_m, _con, _url, _result, NULL, NULL);
}

/*
 * Wrapper for Curl_connect (POST)
 */
static int w_curl_connect_post(struct sip_msg* _m, char* _con, char * _url, char* _ctype, char* _data, char *_result) {
	return curl_con_query_url(_m, _con, _url, _result, _ctype, _data);
}


/*!
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 3) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*!
 * Free http_query params.
 */
static int fixup_free_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 3) {
	return fixup_free_pvar_null(param, 1);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*!
 * Wrapper for HTTP-Query (GET)
 */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result) {
	return http_query(_m, _url, _result, NULL);
}


/*!
 * Wrapper for HTTP-Query (POST-Variant)
 */
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result) {
	return http_query(_m, _url, _result, _post);
}

/*!
 * Parse arguments to PV
 */
static int pv_parse_curlerror(pv_spec_p sp, str *in)
{
	int cerr  = 0;
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	
	cerr = atoi(in->s);
	LM_DBG(" =====> CURL ERROR %d \n", cerr);
	sp->pvp.pvn.u.isname.name.n = cerr;

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;
}

/*
 * PV - return error explanation as string
 */
static int pv_get_curlerror(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str curlerr;
	char *err = NULL;
	CURLcode codeerr;

	if(param==NULL) {
		return -1;
	}

	/* cURL error codes does not collide with HTTP codes */
	if (param->pvn.u.isname.name.n < 0 || param->pvn.u.isname.name.n > 999 ) {
		err = "Bad CURL error code";
	}
	if (param->pvn.u.isname.name.n > 99) {
		err = "HTTP result code";
	}
	if (err == NULL) {
		err = (char *) curl_easy_strerror(param->pvn.u.isname.name.n);
	}
	curlerr.s = err;
	curlerr.len = strlen(err);

	return pv_get_strval(msg, param, res, &curlerr);
}
