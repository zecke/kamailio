/*
 * charging_db.c
 *
 *  Created on: Nov 13, 2013
 *      Author: carlos
 */

#include "../../lib/srdb1/db.h"

#include "../dialog_ng/dlg_hash.h"

#include "charging_db.h"
#include "ims_ro.h"
#include "ro_session_hash.h"
#include "dialog.h"

extern struct dlg_binds dlgb;
extern struct cdp_binds cdpb;

#define GRACE_TIME_BEFORE_TERMINATION 	15

db1_con_t* charging_db_handle    = NULL; /* database connection handle */
db_func_t charging_dbf;

str cdp_session_id_col		= str_init(CDP_SESSION_ID_COL);
str direction_col		    = str_init(DIRECTION_COL);
str ro_session_id_col    	= str_init(RO_SESSION_ID_COL);
str callid_col				= str_init(CALLID_COL);
str from_uri_col			= str_init(FROM_URI_COL);
str to_uri_col				= str_init(TO_URI_COL);
str hop_by_hop_col			= str_init(HOP_BY_HOP_COL);
str reserved_secs_col		= str_init(RESERVED_SECS_COL);
str valid_for_col			= str_init(VALID_FOR_COL);
str dlg_h_entry_col			= str_init(DLG_H_ENTRY_COL);
str dlg_h_id_col			= str_init(DLG_H_ID_COL);
str h_entry_col				= str_init(H_ENTRY_COL);
str h_id_col				= str_init(H_ID_COL);
str start_time_col			= str_init(START_TIME_COL);
str last_event_timestamp_col= str_init(LAST_EVENT_TIMESTAMP_COL);
str event_type_col			= str_init(EVENT_TYPE_COL);
str auth_appid_col			= str_init(AUTH_APPID_COL);
str auth_session_type_col	= str_init(AUTH_SESSION_TYPE_COL);
str active_col				= str_init(ACTIVE_COL);
str avp_mac_col				= str_init(AVP_MAC_COL);

str ims_charging_table_name	= str_init(IMS_CHARGING_TABLE_NAME);
int max_fetch_num_rows;

static int use_ims_charging_table();
static int select_entire_ims_charging_table(db1_res_t ** res, int fetch_num_rows);
static int load_charging_info_from_db(int fetch_num_rows);
static int recreate_cdp_session(struct ro_session *ro_session);

int charging_connect_db(const str *db_url)
{
	if ((charging_db_handle = charging_dbf.init(db_url)) == 0)
		return -1;

	return 0;
}

int charging_init_db(const str *db_url, int fetch_num_rows)
{

	/* Find a database module */
	if (db_bind_mod(db_url, &charging_dbf) < 0){
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (charging_connect_db(db_url)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	if (!DB_CAPABILITY(charging_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement all functions needed by the module\n");
		return -1;
	}

	if (load_charging_info_from_db(fetch_num_rows) != 0) {
		LM_ERR("Sessions recovery has failed");
	}

	charging_dbf.close(charging_db_handle);
	charging_db_handle = NULL;

	max_fetch_num_rows	= fetch_num_rows;
	return 0;
}

void charging_destroy_db()
{
	/* close the DB connection */
	if (charging_db_handle) {
		charging_dbf.close(charging_db_handle);
		charging_db_handle = 0;
	}
}

static int use_ims_charging_table()
{
	if(!charging_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (charging_dbf.use_table(charging_db_handle, &ims_charging_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

int charging_delete_session(struct ro_session* session)
{
	db_key_t where_fields[2] = {
				&dlg_h_entry_col, 	&dlg_h_id_col };
	db_val_t where_values[2];

	VAL_TYPE(GET_FIELD_IDX(where_values, 0)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(where_values, 1)) = DB1_BITMAP;

	VAL_NULL(GET_FIELD_IDX(where_values, 0)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(where_values, 1)) = DB_NOT_NULL;

	VAL_BITMAP(GET_FIELD_IDX(where_values, 0)) = session->dlg_h_entry;
	VAL_BITMAP(GET_FIELD_IDX(where_values, 1)) = session->dlg_h_id;

	if (use_ims_charging_table() < 0) {
		LM_ERR("Error trying to use table ["IMS_CHARGING_TABLE_NAME"]");
		return -1;
	}

    if(charging_dbf.delete(charging_db_handle, where_fields, 0, where_values, 2) < 0) {
    	LM_ERR("Failed to delete database information");
        return -1;
    }

    return 0;
}

int charging_modify_db_session(struct ro_session* session, enum charging_db_action action)
{
	db_key_t fields[IMS_CHARGING_FIELD_NO] = {
				&cdp_session_id_col,
				&direction_col, 		&ro_session_id_col,
				&callid_col,			&from_uri_col,
				&to_uri_col,			&hop_by_hop_col,
				&reserved_secs_col,		&valid_for_col,
				&dlg_h_entry_col,		&dlg_h_id_col,
				&h_entry_col,			&h_id_col,
				&start_time_col,		&event_type_col,
				&last_event_timestamp_col,
				&auth_appid_col,		&auth_session_type_col,
				&active_col,			&avp_mac_col
	};
	db_val_t values[IMS_CHARGING_FIELD_NO];

	db_key_t where_fields[2] = {
				&dlg_h_entry_col, 	&dlg_h_id_col };
	db_val_t where_values[2];

	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_CDP_SESSION_ID_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_DIRECTION_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_RO_SESSION_ID_IDX)) = DB1_STR;

	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_CALLID_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_FROM_URI_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_TO_URI_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_HOP_BY_HOP_IDX)) = DB1_BITMAP;

	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_RESERVED_SECS_IDX)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_VALID_FOR_IDX)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_DLG_H_ID_IDX)) = DB1_BITMAP;

	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_H_ENTRY_IDX)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_H_ID_IDX)) = DB1_BITMAP;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_START_TIME_IDX)) = DB1_DATETIME;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_EVENT_TYPE_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_LAST_EVENT_TIMESTAMP_IDX)) = DB1_DATETIME;

	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_AUTH_APPID_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_AUTH_SESSION_TYPE_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_ACTIVE_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, CHARGING_AVP_MAX_IDX)) = DB1_STR;

	/*
	 * setting not nullable fields
	 */
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_DIRECTION_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_EVENT_TYPE_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_H_ENTRY_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_H_ID_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_DLG_H_ID_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_RESERVED_SECS_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_VALID_FOR_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_AUTH_APPID_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_AUTH_SESSION_TYPE_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_ACTIVE_IDX)) = DB_NOT_NULL;
	VAL_NULL(GET_FIELD_IDX(values, CHARGING_HOP_BY_HOP_IDX)) = DB_NOT_NULL;

	/*
	 * Setting up values
	 */
	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_CDP_SESSION_ID_IDX), session->cdp_session_id);
	SET_PROPER_NULL_FLAG(session->cdp_session_id, 	values, CHARGING_CDP_SESSION_ID_IDX);

	VAL_INT(GET_FIELD_IDX(values, CHARGING_DIRECTION_IDX)) = session->direction;

	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_RO_SESSION_ID_IDX), session->ro_session_id);
	SET_PROPER_NULL_FLAG(session->ro_session_id, 	values, CHARGING_RO_SESSION_ID_IDX);

	// keys for searching
	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX)) = session->dlg_h_entry;
	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ID_IDX)) = session->dlg_h_id;
	//

	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_H_ENTRY_IDX)) = session->h_entry;
	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_H_ID_IDX)) = session->h_id;
	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_EVENT_TYPE_IDX)) = session->event_type;

	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_CALLID_IDX), session->callid);
	SET_PROPER_NULL_FLAG(session->callid, 	values, CHARGING_CALLID_IDX);

	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_FROM_URI_IDX), session->from_uri);
	SET_PROPER_NULL_FLAG(session->from_uri, 	values, CHARGING_FROM_URI_IDX);

	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_TO_URI_IDX), session->to_uri);
	SET_PROPER_NULL_FLAG(session->to_uri, 	values, CHARGING_TO_URI_IDX);

	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_HOP_BY_HOP_IDX)) = session->hop_by_hop;

	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_RESERVED_SECS_IDX)) = session->reserved_secs;
	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_VALID_FOR_IDX)) = session->valid_for;

	VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_VALID_FOR_IDX)) = session->valid_for;
	VAL_TIME(GET_FIELD_IDX(values, CHARGING_START_TIME_IDX)) = session->start_time;
	VAL_TIME(GET_FIELD_IDX(values, CHARGING_LAST_EVENT_TIMESTAMP_IDX)) = session->last_event_timestamp;

	VAL_INT(GET_FIELD_IDX(values, CHARGING_AUTH_APPID_IDX)) = session->auth_appid;
	VAL_INT(GET_FIELD_IDX(values, CHARGING_AUTH_SESSION_TYPE_IDX)) = session->auth_session_type;
	VAL_INT(GET_FIELD_IDX(values, CHARGING_ACTIVE_IDX)) = session->active;

	SET_STR_VALUE(GET_FIELD_IDX(values, CHARGING_AVP_MAX_IDX), session->avp_value.mac);
	SET_PROPER_NULL_FLAG(session->avp_value.mac, 	values, CHARGING_AVP_MAX_IDX);

	if (use_ims_charging_table() < 0) {
		LM_ERR("Error trying to use table ["IMS_CHARGING_TABLE_NAME"]");
		return -1;
	}

	switch(action) {
		case CHARGING_ACTION_INSERT:

			if (charging_dbf.insert(charging_db_handle, fields, values, IMS_CHARGING_FIELD_NO) < 0) {
				LM_ERR("Inserting ro_session to db failed\n");
				return -1;
			}

			break;
		case CHARGING_ACTION_UPDATE:
			VAL_TYPE(GET_FIELD_IDX(where_values, 0)) = DB1_BITMAP;
			VAL_TYPE(GET_FIELD_IDX(where_values, 1)) = DB1_BITMAP;

			VAL_NULL(GET_FIELD_IDX(where_values, 0)) = DB_NOT_NULL;
			VAL_NULL(GET_FIELD_IDX(where_values, 1)) = DB_NOT_NULL;

			VAL_BITMAP(GET_FIELD_IDX(where_values, 0)) = session->dlg_h_entry;
			VAL_BITMAP(GET_FIELD_IDX(where_values, 1)) = session->dlg_h_id;

			// start-time and last-event-time shouldn't be null at this point
			VAL_NULL(GET_FIELD_IDX(values, CHARGING_START_TIME_IDX)) = DB_NOT_NULL;
			VAL_NULL(GET_FIELD_IDX(values, CHARGING_LAST_EVENT_TIMESTAMP_IDX)) = DB_NOT_NULL;

			if((charging_dbf.update(charging_db_handle, where_fields, NULL, where_values, fields, values, 2, IMS_CHARGING_FIELD_NO)) !=0){
				LM_ERR("Could not update database info\n");
			    return -1;
			}

			break;
		default:
			LM_ERR("Unrecognized operation [%d]", action);
			return -1;
	}

	return 0;
}

static int select_entire_ims_charging_table(db1_res_t ** res, int fetch_num_rows)
{
	db_key_t query_fields[IMS_CHARGING_FIELD_NO] = {
								&cdp_session_id_col,
								&direction_col, 		&ro_session_id_col,
								&callid_col,			&from_uri_col,
								&to_uri_col,			&hop_by_hop_col,
								&reserved_secs_col,		&valid_for_col,
								&dlg_h_entry_col,		&dlg_h_id_col,
								&h_entry_col,			&h_id_col,
								&start_time_col,		&event_type_col,
								&last_event_timestamp_col,
								&auth_appid_col,		&auth_session_type_col,
								&active_col,			&avp_mac_col
					};

	if(use_ims_charging_table() != 0) {
			return -1;
	}

	/* select the whole table and all the columns */
	if (DB_CAPABILITY(charging_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(charging_dbf.query(charging_db_handle,0,0,0,query_fields, 0,
							IMS_CHARGING_FIELD_NO, 0, 0) < 0) {
			LM_ERR("Error while querying (fetch) database\n");
			return -1;
		}

		if(charging_dbf.fetch_result(charging_db_handle, res, fetch_num_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	}
	else {
		if(charging_dbf.query(charging_db_handle, 0, 0, 0, query_fields, 0,
							IMS_CHARGING_FIELD_NO, 0, res) < 0) {
			LM_ERR("Error while querying database\n");
			return -1;
		}
	}

	return 0;
}

static int load_charging_info_from_db(int fetch_num_rows)
{
	db1_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	struct ro_session *ro_session = NULL;
	struct dlg_cell* dlg;
	int direction, hop_by_hop,
		auth_session_type,
		auth_appid, active,
		i, nr_rows, used_secs;

	str callid,
		from_uri, to_uri,
		/* cdp_session_id */
		ro_session_id, avp_mac;

	time_t start_time, last_event;

	unsigned int dlg_h_entry, dlg_h_id, h_entry, h_id,
				requested_secs, validity_timeout, event_type;

	res = 0;
	if((nr_rows = select_entire_ims_charging_table(&res, fetch_num_rows)) < 0)
		return 0;

	nr_rows = RES_ROW_N(res);

	LM_DBG("The database has information about [%i] ro sessions\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---dialog */
		for(i=0; i<nr_rows; i++) {
			values = ROW_VALUES(rows + i);

			if (VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX)) == 0 ||
				VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_H_ENTRY_IDX)) == 0) {
				LM_ERR("Columns [%.*s] or/and [%.*s] cannot be null -> skipping\n",
						dlg_h_id_col.len, dlg_h_id_col.s,
						h_entry_col.len, h_entry_col.s);
				continue;
			}

			/*restore the ro session info*/
			//GET_STR_VALUE(cdp_session_id, values, CHARGING_CDP_SESSION_ID_IDX);
			direction	= VAL_INT(GET_FIELD_IDX(values, CHARGING_DIRECTION_IDX));
			GET_STR_VALUE(ro_session_id, values, CHARGING_RO_SESSION_ID_IDX);
			GET_STR_VALUE(callid, values, CHARGING_CALLID_IDX);
			GET_STR_VALUE(from_uri, values, CHARGING_FROM_URI_IDX);
			GET_STR_VALUE(to_uri, values, CHARGING_TO_URI_IDX);
			hop_by_hop = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_HOP_BY_HOP_IDX));
			requested_secs = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_RESERVED_SECS_IDX));
			validity_timeout = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_VALID_FOR_IDX));
			dlg_h_entry = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX));
			dlg_h_id = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ID_IDX));
			h_entry	= VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ENTRY_IDX));
			h_id =  VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_DLG_H_ID_IDX));
			start_time = VAL_TIME(GET_FIELD_IDX(values, CHARGING_START_TIME_IDX));
			event_type = VAL_BITMAP(GET_FIELD_IDX(values, CHARGING_EVENT_TYPE_IDX));
			last_event = VAL_TIME(GET_FIELD_IDX(values, CHARGING_LAST_EVENT_TIMESTAMP_IDX));
			auth_appid = VAL_INT(GET_FIELD_IDX(values, CHARGING_AUTH_APPID_IDX));
			auth_session_type = VAL_INT(GET_FIELD_IDX(values, CHARGING_AUTH_SESSION_TYPE_IDX));
			active = VAL_INT(GET_FIELD_IDX(values, CHARGING_ACTIVE_IDX));
			GET_STR_VALUE(avp_mac, values, CHARGING_AVP_MAX_IDX);

			ro_session = build_new_ro_session(direction, auth_appid, auth_session_type, &ro_session_id, &callid,
											  &from_uri, &to_uri, &avp_mac, dlg_h_entry, dlg_h_id,
											  requested_secs, validity_timeout);

			ro_session->ro_session_id.s = (char*) shm_malloc(ro_session_id.len);
			ro_session->ro_session_id.len = ro_session_id.len;
			memcpy(ro_session->ro_session_id.s, ro_session_id.s, ro_session_id.len);
			ro_session->hop_by_hop = hop_by_hop;
			ro_session->h_id = h_id;
			ro_session->h_entry = h_entry;
			ro_session->start_time = start_time;
			ro_session->active	= active;
			ro_session->last_event_timestamp = last_event;
			ro_session->event_type = (enum ro_session_event_type) event_type;
			ro_session->reserved_secs = requested_secs;
			ro_session->valid_for = validity_timeout;

			link_ro_session(ro_session, 1);

			if (hop_by_hop <= 1) {
				LM_ERR("Initial CCR cannot be resumed because I don't (and can't) have the cfg action pointers. Sorry.");
				charging_delete_session(ro_session);
//				destroy_ro_session(ro_session);
				unref_ro_session(ro_session, 1);
				continue;
			}

			if (!(dlg = dlgb.lookup_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id))) {
				LM_ERR("Couldn't find dialog entry using h_entry[%u] and h_id[%u]", ro_session->dlg_h_entry, ro_session->dlg_h_id);
				continue;
			}

			used_secs = time(0) - ro_session->last_event_timestamp;
			if (used_secs > ro_session->reserved_secs) {
				LM_WARN("Couldn't recover session when reserved seconds timed out. "
							"Call started at [%u], now it's [%u], we reserved [%u] secs, "
							"and we overpassed to [%d] secs. Terminating call in [%d] secs",
							(unsigned int) ro_session->start_time, (unsigned int) time(0),
							ro_session->reserved_secs, used_secs,
							GRACE_TIME_BEFORE_TERMINATION);


				charging_delete_session(ro_session);

				//dlgb.lookup_terminate_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id, NULL);
				//
				// Can't terminate a call when the module is loading and before forking because
				// no network connectivity is available at this point. We will instead wait for
				// GRACE_TIME_BEFORE_TERMINATION till everything is up and using the Ro timer we will tear the call
				// down.
				// TODO make this configurable, or listen to A. Tanenbaum who suggested to use a semaphore instead.

				ro_session->force_termination = 1;
				if (insert_ro_timer(&ro_session->ro_tl, GRACE_TIME_BEFORE_TERMINATION) != 0) {
					LM_ERR("Error inserting ro_timer");
				}

				continue;
			}

			LM_INFO("Recovering call to [%.*s] where [%d] secs were reserved and [%d] were consumed",
						ro_session->to_uri.len, ro_session->to_uri.s,
						ro_session->reserved_secs,
						used_secs);

			if (recreate_cdp_session(ro_session) != 0) {
				LM_ERR("Error trying to recreate ro session after recovery");
				continue;
			}

			if (insert_ro_timer(&ro_session->ro_tl, ro_session->reserved_secs - used_secs) != 0) {
				LM_ERR("Error inserting ro_timer for recovered call.");
				continue;
			}

			if (setup_dialog_handlers(dlg, ro_session) != 0) {
				LM_ERR("Couldn't setup dialog handler for restored ro session");
			}
		}

		/* any more data to be fetched ?*/
		if (DB_CAPABILITY(charging_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(charging_dbf.fetch_result(charging_db_handle, &res, fetch_num_rows) < 0) {
				LM_ERR("re-fetching rows failed\n");
				return -1;
			}

			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		}
		else
			nr_rows = 0;
	}
	while (nr_rows>0);

	return 0;
}

static int recreate_cdp_session(struct ro_session *ro_session) {
	AAASession *auth = cdpb.AAAMakeSession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id);

	if (!auth)
		return -1;

	auth->u.cc_acc.state = ACC_CC_ST_OPEN;
	cdpb.AAASessionsUnlock(auth->hash);

	return 0;
}




