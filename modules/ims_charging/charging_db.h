/*
 * charging_db.h
 *
 *  Created on: Nov 13, 2013
 *      Author: carlos
 */

#ifndef CHARGING_DB_H_
#define CHARGING_DB_H_

#include "ro_session_hash.h"

//#define ID_COL						"id"
#define CDP_SESSION_ID_COL			"cdp_session_id"
#define DIRECTION_COL				"direction"
#define RO_SESSION_ID_COL			"ro_session_id"
#define CALLID_COL					"callid"
#define FROM_URI_COL				"from_uri"
#define TO_URI_COL					"to_uri"
#define HOP_BY_HOP_COL				"hop_by_hop"
#define RESERVED_SECS_COL			"reserved_secs"
#define VALID_FOR_COL				"valid_for"
#define DLG_H_ENTRY_COL				"dlg_h_entry"
#define DLG_H_ID_COL				"dlg_h_id"
#define H_ENTRY_COL					"h_entry"
#define H_ID_COL					"h_id"
#define START_TIME_COL				"start_time"
#define LAST_EVENT_TIMESTAMP_COL	"last_event_timestamp"
#define	EVENT_TYPE_COL				"event_type"
#define AUTH_APPID_COL				"auth_appid"
#define AUTH_SESSION_TYPE_COL		"auth_session_type"
#define ACTIVE_COL					"active"
#define AVP_MAC_COL					"avp_mac"

#define IMS_CHARGING_TABLE_NAME		"ims_charging"
#define IMS_CHARGING_FIELD_NO		20

#define	DB_MODE_NONE				0
#define	DB_MODE_REALTIME			1

#define DB_NOT_NULL					0
#define DB_NULL						1

enum charging_fields_idx {
	CHARGING_CDP_SESSION_ID_IDX = 0,
	CHARGING_DIRECTION_IDX,
	CHARGING_RO_SESSION_ID_IDX,
	CHARGING_CALLID_IDX,
	CHARGING_FROM_URI_IDX,
	CHARGING_TO_URI_IDX,
	CHARGING_HOP_BY_HOP_IDX,
	CHARGING_RESERVED_SECS_IDX,
	CHARGING_VALID_FOR_IDX,
	CHARGING_DLG_H_ENTRY_IDX,
	CHARGING_DLG_H_ID_IDX,
	CHARGING_H_ENTRY_IDX,
	CHARGING_H_ID_IDX,
	CHARGING_START_TIME_IDX,
	CHARGING_EVENT_TYPE_IDX,
	CHARGING_LAST_EVENT_TIMESTAMP_IDX,
	CHARGING_AUTH_APPID_IDX,
	CHARGING_AUTH_SESSION_TYPE_IDX,
	CHARGING_ACTIVE_IDX,
	CHARGING_AVP_MAX_IDX

};

enum charging_db_action {
	CHARGING_ACTION_INSERT,
	CHARGING_ACTION_UPDATE,
};

#define GET_FIELD_IDX(_val, _idx)\
						(_val + _idx)

#define SET_PROPER_NULL_FLAG(_str, _vals, _index)\
	do{\
		if( (_str).len == 0)\
			VAL_NULL( (_vals)+(_index) ) = 1;\
		else\
			VAL_NULL( (_vals)+(_index) ) = 0;\
	}while(0);

#define SET_STR_VALUE(_val, _str)\
	do{\
			VAL_STR((_val)).s 		= (_str).s;\
			VAL_STR((_val)).len 	= (_str).len;\
	}while(0);

#define SET_NULL_FLAG(_vals, _i, _max, _flag)\
	do{\
		for((_i) = 0;(_i)<(_max); (_i)++)\
			VAL_NULL((_vals)+(_i)) = (_flag);\
	}while(0);

#define GET_STR_VALUE(_res, _values, _index)\
	do{\
		if (VAL_NULL((_values) + (_index))) { \
				(_res).s = 0; \
				(_res).len = 0; \
		} else { \
			(_res).s = VAL_STR((_values)+ (_index)).s;\
			(_res).len = strlen(VAL_STR((_values)+ (_index)).s);\
		} \
	}while(0);

int charging_modify_db_session(struct ro_session* session, enum charging_db_action action);
int charging_delete_session(struct ro_session* session);
int charging_connect_db(const str *db_url);
int charging_init_db(const str *db_url, int fetch_num_rows);
void charging_destroy_db();

#endif /* CHARGING_DB_H_ */
