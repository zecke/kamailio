/**
 * $Id$
 *
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "json_path.h"
#include "json_funcs.h"

int jsonmod_fail(json_t* json) {
    json_decref(json);
    return -1;
}

int jsonmod_path_get(struct sip_msg* msg, char* json_in, char* path_in, char* dst)
{
    str json_s;
    str path_s;
    pv_spec_t *dst_pv;
    pv_value_t dst_val;

    if (fixup_get_svalue(msg, (gparam_p)json_in, &json_s) != 0) {
        ERR("cannot get json string value\n");
        return -1;
    }

    if (fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
        ERR("cannot get path string value\n");
        return -1;
    }

    dst_pv = (pv_spec_t *)dst;

    json_t* json;
    json_error_t parsing_error;

    json = json_loads(json_s.s, json_s.len, &parsing_error);

    if(!json) {
        ERR("json error at line %d: %s\n", 
                parsing_error.line, parsing_error.text);
        return jsonmod_fail(json);
    }

    char* path = path_s.s;

    json_t* v = json_path_get(json, path);
    if(!v) {
        ERR("failed to find %s in json\n", path);
        return jsonmod_fail(json);
    }

    char* freeme = NULL;

    if(json_is_object(v) || json_is_array(v)) {
        const char* value = json_dumps(v, JSON_COMPACT);
        freeme = (char*)value;
        dst_val.rs.s = (char*)value;
        dst_val.rs.len = strlen(value);
        dst_val.flags = PV_VAL_STR;
    } else if(json_is_string(v)) {
        const char* value = json_string_value(v);
        dst_val.rs.s = (char*)value;
        dst_val.rs.len = strlen(value);
        dst_val.flags = PV_VAL_STR;
    }else if(json_is_boolean(v)) {
        dst_val.ri = json_is_true(v) ? 0 : 1;
        dst_val.flags |= PV_TYPE_INT|PV_VAL_INT;
    }else if(json_is_integer(v)) {
        int value = json_integer_value(v);
        dst_val.ri = value;
        dst_val.flags |= PV_TYPE_INT|PV_VAL_INT;
    } else if(json_is_null(v)) {
        dst_val.flags = PV_VAL_NULL;
    } else {
        ERR("unrecognized json value: %d\n", json_typeof(v));
        return jsonmod_fail(json);
    }

    dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

    if(freeme!=NULL) {
        free(freeme);
    }

    json_decref(json);
    return 1;

}

int jsonmod_array_size(struct sip_msg* msg, char* json_in, char* path_in, char* dst)
{
    str json_s;
    str path_s;
    pv_spec_t *dst_pv;
    pv_value_t dst_val;

    if (fixup_get_svalue(msg, (gparam_p)json_in, &json_s) != 0) {
        ERR("cannot get json string value\n");
        return -1;
    }

    if (fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
        ERR("cannot get path string value\n");
        return -1;
    }

    dst_pv = (pv_spec_t *)dst;

    json_t* json;
    json_error_t parsing_error;

    json = json_loads(json_s.s, json_s.len, &parsing_error);

    if(!json) {
        ERR("json error at line %d: %s\n", 
                parsing_error.line, parsing_error.text);
        return jsonmod_fail(json);
    }

    char* path = path_s.s;

    json_t* v = json_path_get(json, path);
    if(!v) {
        ERR("failed to find %s in json\n", path);
        return jsonmod_fail(json);
    }

    if(!json_is_array(v)) {
        ERR("%s is not an array\n", path);
        return jsonmod_fail(json);
    }

    int size = json_array_size(v);
    dst_val.ri = size;
    dst_val.flags |= PV_TYPE_INT|PV_VAL_INT;

    dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

    json_decref(json);
    return 1;
}
