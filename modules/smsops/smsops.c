/**
 * Copyright (C) 2015 Carsten Bock, ng-voice GmbH
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../pvar.h"
#include "../../mod_fix.h"

#include "smsops_impl.h"

MODULE_VERSION

static pv_export_t mod_pvs[] = {
	{ {"smsack", sizeof("smsack")-1}, PVT_OTHER, pv_smsack, 0, 0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"smsdump",   (cmd_function)smsdump, 0, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

/** module exports */
struct module_exports exports= {
	"smsops",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	0,
	0,          /* exported statistics */
	0,    /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	0,   /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};

