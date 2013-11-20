/*
 * usrloc_db.c
 *
 *  Created on: Nov 11, 2013
 *      Author: carlos
 */

#include "../../lib/srdb1/db.h"
#include "usrloc.h"
#include "usrloc_db.h"

db1_con_t* lp_db_handle    = 0; /* database connection handle */
db_func_t lp_dbf;

str id_col	        	= str_init(ID_COL);
str domain_col			= str_init(DOMAIN_COL);
str aor_col		    	= str_init(AOR_COL);
str received_col    	= str_init(RECEIVED_COL);
str received_port_col	= str_init(RECEIVED_PORT_COL);
str received_proto_col	= str_init(RECEIVED_PROTO_COL);
str path_col			= str_init(PATH_COL);
str rx_session_id_col	= str_init(RX_SESSION_ID_COL);
str reg_state_col		= str_init(REG_STATE_COL);
str expires_col			= str_init(EXPIRES_COL);
str service_routes_col	= str_init(SERVICE_ROUTES_COL);
str socket_col			= str_init(SOCKET_COL);
str public_ids_col		= str_init(PUBLIC_IDS_COL);

str location_pcscf_table_name	= str_init(LOCATION_PCSCF_TABLE_NAME);

int connect_db(const str *db_url)
{
	if (lp_db_handle) {	/* we've obv already connected... */
		LM_WARN("DB connection already open... continuing\n");
		return 0;
	}

	if ((lp_db_handle = lp_dbf.init(db_url)) == 0)
		return -1;
	LM_DBG("Successfully connected to DB and returned DB handle ptr %p\n", lp_db_handle);
	return 0;
}

int init_db(const str *db_url, int db_update_period, int fetch_num_rows)
{
	/* Find a database module */
	if (db_bind_mod(db_url, &lp_dbf) < 0){
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (connect_db(db_url)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	if (!DB_CAPABILITY(lp_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement all functions needed by the module\n");
		return -1;
	}

	/*
	 	 if( (dlg_db_mode==DB_MODE_DELAYED) &&
        (register_timer( dialog_update_db, 0, db_update_period)<0 )) {
                LM_ERR("failed to register update db\n");
                return -1;
        }
	*/
	lp_dbf.close(lp_db_handle);
	lp_db_handle = 0;

	return 0;
}

void destroy_db()
{
	/* close the DB connection */
	if (lp_db_handle) {
		lp_dbf.close(lp_db_handle);
		lp_db_handle = 0;
	}
}

int use_location_pcscf_table()
{
	if(!lp_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (lp_dbf.use_table(lp_db_handle, &location_pcscf_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

int db_update_pcontact(pcontact_t* _c)
{
	char buf[2048];
	str impus;

	db_val_t match_values[1];
	db_key_t match_keys[1] = { &aor_col };
	db_key_t update_keys[8] = { &expires_col, &reg_state_col,
								&service_routes_col, &received_col,
								&received_port_col, &received_proto_col,
								&rx_session_id_col, &public_ids_col };
	db_val_t values[8];

	LM_DBG("updating pcontact: %.*s\n", _c->aor.len, _c->aor.s);

	VAL_TYPE(match_values) = DB1_STR;

	VAL_NULL(match_values) = 0;
	VAL_STR(match_values) = _c->aor;

	if (use_location_pcscf_table() < 0) {
		LM_ERR("Error trying to use table ["LOCATION_PCSCF_TABLE_NAME"]");
		return -1;
	}

	VAL_TYPE(values) 	= DB1_DATETIME;
	VAL_TIME(values)	= _c->expires;
	VAL_NULL(values) 	= 0;

	VAL_TYPE(values + 1)= DB1_INT;
	VAL_NULL(values + 1)= 0;
	VAL_INT(values + 1)	= _c->reg_state;

	str empty_str = str_init("");
	SET_STR_VALUE(values + 2, (_c->service_routes)?*_c->service_routes:empty_str);
	VAL_TYPE(values + 2) = DB1_STR;
	VAL_NULL(values + 2) = 0;

	SET_STR_VALUE(values + 3, _c->received_host);
	VAL_TYPE(values + 3) = DB1_STR;
	VAL_NULL(values + 3) = 0;

	VAL_TYPE(values + 4)= DB1_INT;
	VAL_NULL(values + 4)= 0;
	VAL_INT(values + 4)	= _c->received_port;

	VAL_TYPE(values + 5)= DB1_INT;
	VAL_NULL(values + 5)= 0;
	VAL_INT(values + 5)	= _c->received_proto;

	VAL_TYPE(values + 6) = DB1_STR;
	SET_PROPER_NULL_FLAG(_c->rx_session_id, values, 6);
	LM_DBG("Trying to set rx session id: %.*s\n", _c->rx_session_id.len, _c->rx_session_id.s);
	SET_STR_VALUE(values + 6, _c->rx_session_id);

	/* add the public identities */
//	impu = _c->head;
//	memset(buf, 0, 2048);
//	p = buf;
//	while (impu) {
//		*p++ = '<';
//		memcpy(p, impu->public_identity.s, impu->public_identity.len);
//		p += impu->public_identity.len;
//		*p++ = '>';
////		sprintf(buf + len, "<%s>", impu->public_identity.s);
////		len += impu->public_identity.len + 2 /* <> */;
//		impu = impu->next;
//	}
//	p = buf;
	impus.len = impus_as_string(_c, buf);//p - buf;
	impus.s = buf;//buf;
	VAL_TYPE(values + 7) = DB1_STR;
	SET_PROPER_NULL_FLAG(impus, values, 7);
	SET_STR_VALUE(values + 7, impus);

	if((lp_dbf.update(lp_db_handle, match_keys, NULL, match_values, update_keys,values, 1, 8)) !=0){
		LM_ERR("could not update database info\n");
	    return -1;
	}

	if (lp_dbf.affected_rows && lp_dbf.affected_rows(lp_db_handle) == 0) {
		LM_DBG("no existing rows for an update... doing insert\n");
		if (db_insert_pcontact(_c) != 0) {
			LM_ERR("Failed to insert a pcontact on update\n");
		}
	}

	return 0;
}

int db_delete_pcontact(pcontact_t* _c)
{
	LM_DBG("Trying to delete contact: %.*s\n", _c->aor.len, _c->aor.s);
	db_val_t values[1];
	db_key_t match_keys[1] = { &aor_col};

	VAL_TYPE(values) = DB1_STR;
	VAL_NULL(values) = 0;
	SET_STR_VALUE(values, _c->aor);

	if (use_location_pcscf_table() < 0) {
		LM_ERR("Error trying to use table ["LOCATION_PCSCF_TABLE_NAME"]");
		return -1;
	}

    if(lp_dbf.delete(lp_db_handle, match_keys, 0, values, 1) < 0) {
    	LM_ERR("Failed to delete database information: aor[%.*s], rx_session_id=[%.*s]\n",
    													_c->aor.len, _c->aor.s,
    													_c->rx_session_id.len, _c->rx_session_id.s);
        return -1;
    }

    return 0;
}

int db_insert_pcontact(struct pcontact* _c)
{
	str empty_str = str_init("");
	char buf[2048];
	str impus;

	db_key_t keys[LOCATION_PCSCF_COL_NO] = {
							&domain_col,
				&aor_col, 			&received_col,
				&received_port_col,	&received_proto_col,
				&path_col,			&rx_session_id_col,
				&reg_state_col,
				&expires_col,		&service_routes_col,
				&socket_col,		&public_ids_col
	};
	db_val_t values[LOCATION_PCSCF_COL_NO];

//	VAL_TYPE(GET_FIELD_IDX(values, LP_ID_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_DOMAIN_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_AOR_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_PORT_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_PROTO_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_PATH_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RX_SESSION_ID_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_REG_STATE_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_EXPIRES_IDX)) = DB1_DATETIME;
	VAL_TYPE(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_PUBLIC_IPS_IDX)) = DB1_STR;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_DOMAIN_IDX), (*_c->domain));
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_AOR_IDX), _c->aor);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_RECEIVED_IDX), _c->received_host);

	SET_PROPER_NULL_FLAG((*_c->domain), values, LP_DOMAIN_IDX);
	SET_PROPER_NULL_FLAG(_c->aor, values, LP_AOR_IDX);
	SET_PROPER_NULL_FLAG(_c->received_host, values, LP_RECEIVED_IDX);

	VAL_INT(GET_FIELD_IDX(values, LP_RECEIVED_PORT_IDX)) = _c->received_port;
	VAL_INT(GET_FIELD_IDX(values, LP_RECEIVED_PROTO_IDX)) = _c->received_proto;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_PATH_IDX), _c->path);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_RX_SESSION_ID_IDX), _c->rx_session_id);

	SET_PROPER_NULL_FLAG(_c->path, values, LP_PATH_IDX);
	SET_PROPER_NULL_FLAG(_c->rx_session_id, values, LP_RX_SESSION_ID_IDX);

	VAL_DOUBLE(GET_FIELD_IDX(values, LP_REG_STATE_IDX)) = _c->reg_state;
	VAL_TIME(GET_FIELD_IDX(values, LP_EXPIRES_IDX)) = _c->expires;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX), _c->service_routes?(*_c->service_routes):empty_str);
	VAL_NULL(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = 1;
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_SOCKET_IDX), _c->sock?_c->sock->sock_str:empty_str);
	VAL_NULL(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = 1;

	if (_c->service_routes) {
		SET_PROPER_NULL_FLAG((*_c->service_routes), values, LP_SERVICE_ROUTES_IDX);
	}
	else {
		VAL_NULL(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = 1;
	}

	if (_c->sock) {
		SET_PROPER_NULL_FLAG(_c->sock->sock_str, values, LP_SOCKET_IDX);
	} else {
		VAL_NULL(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = 1;
	}

	/* add the public identities */
//	impu = _c->head;
//	memset(buf, 0, 2048);
//	p = buf;
//	while (impu) {
//		*p++ = '<';
//		memcpy(p, impu->public_identity.s, impu->public_identity.len);
//		p += impu->public_identity.len;
//		*p++ = '>';
//	}
	impus.len = impus_as_string(_c, buf);//p - buf;
	impus.s = buf;
	SET_PROPER_NULL_FLAG(impus, values, LP_PUBLIC_IPS_IDX);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_PUBLIC_IPS_IDX), impus);

	if (use_location_pcscf_table() < 0) {
		LM_ERR("Error trying to use table ["LOCATION_PCSCF_TABLE_NAME"]");
		return -1;
	}

	if (lp_dbf.insert(lp_db_handle, keys, values, LOCATION_PCSCF_COL_NO) < 0) {
		LM_ERR("inserting contact in db failed\n");
		return -1;
	}

	return 0;
}

/* take a contact structure and a pointer to some memory and returns a list of public identities in the format
 * <impu1><impu2>....<impu(n)>
 * make sure p already has memory allocated
 * returns the length of the string (list)
 * the string list itself will be available in p
 */
int impus_as_string(struct pcontact* _c, char *p) {
	ppublic_t* impu;
	char *start = p;

	impu = _c->head;
	while (impu) {
		*p++ = '<';
		memcpy(p, impu->public_identity.s, impu->public_identity.len);
		p += impu->public_identity.len;
		*p++ = '>';
		impu = impu->next;
	}

	return (p - start);
}


