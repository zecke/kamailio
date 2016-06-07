/*
 * imc module - RPC functions
 *
 * Copyright (C) 2016 Edvina AB, Olle E. Johansson
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "imc.h"
#include "imc_cmd.h"


static const char* imc_rpc_list_doc[2] = {
	"List all IMC conference members. Arg: <conf>",
	0
};

/*
 * RPC command to list conference members:
 * 	imc.list <conf>
 */
static void imc_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	return;
}

static const char* imc_rpc_listall_doc[2] = {
	"List all IMC conference members. Arg: <conf>",
	0
};

/*
 * RPC command to list conferences :
 * 	imc.listall <conf>
 */
static void imc_rpc_listall(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	return;
}

static const char* imc_rpc_create_doc[2] = {
	"Create IMC conference. Arg: <conf>",
	0
};

/*
 * RPC command to create conferences :
 * 	imc.create <conf> <flag>
 *		flag: "private"
 */
static void imc_rpc_create(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;
	str confname;
	imc_room_p room = 0;
	int flag_room = 0;
	int flag_member = 0;

	/* Get the name */
	if (rpc->scan(ctx, "S", &confname) != 1) {
		rpc->fault(c, 500, "Missing parameter confname (Parameters: confname [option])");
		return;
	}

	/* Check if room exists first */
		/* Destination host */
	room = imc_get_room(&confname, &dst->host);
	if(room != NULL) {
		/* Error */
		rpc->fault(c, 500, "Conference already exists");
		return;
	}

	return;
}

static const char* imc_rpc_destroy_doc[2] = {
	"Destroy IMC conference. Kicks all remaining members. Arg: <conf>",
	0
};

/*
 * RPC command to destroy conferences :
 * 	imc.destroy <conf>
 */
static void imc_rpc_destroy(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	return;
}

static const char* imc_rpc_addmember_doc[2] = {
	"Add member to IMC conference. Arg: <conf> <SIP uri> [<option>]",
	0
};

/*
 * RPC command to destroy conferences :
 * 	imc.addmember <conf> <uri> [<option>]
 *		Option can be "admin"
 *		Option can be "muted"
 */
static void imc_rpc_addmember(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	return;
}

static const char* imc_rpc_kickmember_doc[2] = {
	"Kick member from IMC conference. Arg: <conf> <member>",
	0
};

/*
 * RPC command to destroy conferences :
 * 	imc.kickmember <conf>
 */
static void imc_rpc_kickmember(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	return;
}

rpc_export_t imc_rpc_cmds[] = {
	{"imc.list",		imc_rpc_list,		imc_rpc_list_doc,	0}, 	/* List conference participants */
	{"imc.listall",		imc_rpc_listall,	imc_rpc_listall_doc,	0}, 	/* List all conferences */
	{"imc.create",		imc_rpc_create,		imc_rpc_create_doc,	0}, 	/* Create conference */
	{"imc.destroy",		imc_rpc_destroy,	imc_rpc_destroy_doc,	0}, 	/* Destroy conference */
	{"imc.addmember",	imc_rpc_addmember,	imc_rpc_addmember_doc,	0}, 	/* Add member to conference */
	{"imc.kickmember",	imc_rpc_kickmember,	imc_rpc_kickmember_doc,	0}, 	/* Kick member from conference */
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int imc_init_rpc(void)
{
	if (rpc_register_array(imc_rpc_cmds)!=0)
	{
		LM_ERR("IMC: Failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


