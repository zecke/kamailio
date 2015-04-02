/*
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
 *
 */

/*! \file
 * \brief Support for transformations
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h" 
#include "../../trim.h" 
#include "../../pvapi.h"
#include "../../dset.h"
#include "../../basex.h"

#include "../../lib/kcore/strcommon.h"
#include "../../parser/parse_content.h" 


#include "smsops_impl.h"

// Types of RP-DATA
typedef enum _rp_message_type {
	RP_DATA_MS_TO_NETWORK = 0x00,
	RP_DATA_NETWORK_TO_MS = 0x01,
	RP_ACK_MS_TO_NETWORK  = 0x02,
	RP_ACK_NETWORK_TO_MS  = 0x03,
} rp_message_type_t;

// Types of the PDU-Message
typedef enum _pdu_message_type {
	DELIVER = 0x00,
	SUBMIT = 0x01,
	COMMAND = 0x02,
	ANY = 0x03,
} pdu_message_type_t;

#define TP_RD 0x4;
#define TP_VPF 0x16;
#define TP_SRR 0x32;
#define TP_UDHI 0x64;
#define TP_RP 0x128;

// PDU (GSM 03.40) of the SMS
typedef struct _sms_pdu {
	pdu_message_type_t msg_type;
	char reference;
	char flags;
	str source;
	str destination;
	str payload;
} sms_pdu_t;

// RP-Data of the message
typedef struct _sms_rp_data {
	rp_message_type_t msg_type;
	char reference;
	str originator;
	str destination;
	int pdu_len;
	sms_pdu_t pdu;	
} sms_rp_data_t;

// Pointer to current contact_t
static sms_rp_data_t * rp_data = NULL;

// ID of current message
static unsigned int current_msg_id = 0;

/*******************************************************************
 * Helper Functions
 *******************************************************************/

// Frees internal pointers within the SMS-PDU-Type:
void freeRP_DATA() {
	if (rp_data) {
		if (rp_data->originator.s) pkg_free(rp_data->originator.s);
		if (rp_data->destination.s) pkg_free(rp_data->destination.s);
		if (rp_data->pdu.source.s) pkg_free(rp_data->pdu.source.s);
		if (rp_data->pdu.destination.s) pkg_free(rp_data->pdu.destination.s);
		if (rp_data->pdu.payload.s) pkg_free(rp_data->pdu.payload.s);
	}
}

#define BITMASK_7BITS 0x7F
#define BITMASK_8BITS 0xFF
#define BITMASK_HIGH_4BITS 0xF0
#define BITMASK_LOW_4BITS 0x0F

// Encode SMS-Message by merging 7 bit ASCII characters into 8 bit octets.
static int EncodeSMSMessage(str sms, char * output_buffer, int buffer_size) {
	// Check if output buffer is big enough.
	if ((sms.len * 7 + 7) / 8 > buffer_size)
		return -1;

	int output_buffer_length = 0;
	int carry_on_bits = 1;
	int i = 0;

	for (; i < sms.len; ++i) {
		output_buffer[output_buffer_length++] =
			((sms.s[i] & BITMASK_7BITS) >> (carry_on_bits - 1)) |
			((sms.s[i + 1] & BITMASK_7BITS) << (8 - carry_on_bits));
		carry_on_bits++;
		if (carry_on_bits == 8) {
			carry_on_bits = 1;
			++i;
		}
	}

	if (i <= sms.len)
		output_buffer[output_buffer_length++] =	(sms.s[i] & BITMASK_7BITS) >> (carry_on_bits - 1);

	return output_buffer_length;
}

// Decode SMS-Message by splitting 8 bit encoded buffer into 7 bit ASCII characters.
static int DecodeSMSMessage(char* buffer, str sms)
{
	int output_text_length = 0;
	if (sms.len > 0)
		sms.s[output_text_length++] = BITMASK_7BITS & buffer[0];

	int carry_on_bits = 1;
	int i = 1;
	for (; i < sms.len; ++i) {
		sms.s[output_text_length++] = BITMASK_7BITS &	((buffer[i] << carry_on_bits) | (buffer[i - 1] >> (8 - carry_on_bits)));

		carry_on_bits++;
		if (carry_on_bits == 8) {
			carry_on_bits = 1;
			sms.s[output_text_length++] = buffer[i] & BITMASK_7BITS;
		}
	}

	return output_text_length;
}

// Swap decimal digits of a number (e.g. 12 -> 21).
static unsigned char SwapDecimalNibble(char x) {
	return (x / 16) + ((x % 16) * 10);
}

// Encode a digit based phone number for SMS based format.
static int EncodePhoneNumber(str phone, char * output_buffer, int buffer_size) {
	int output_buffer_length = 0;
	// Check if the output buffer is big enough.
	if ((phone.len + 1) / 2 > buffer_size)
		return -1;

	int i = 0;
	for (; i < phone.len; ++i) {
		if (phone.s[i] < '0' && phone.s[i] > '9')
			return -1;
		if (i % 2 == 0) {
			output_buffer[output_buffer_length++] =	BITMASK_HIGH_4BITS | (phone.s[i] - '0');
		} else {
			output_buffer[output_buffer_length - 1] = (output_buffer[output_buffer_length - 1] & BITMASK_LOW_4BITS) | ((phone.s[i] - '0') << 4); 
		}
	}

	return output_buffer_length;
}

// Decode a digit based phone number for SMS based format.
static int DecodePhoneNumber(char* buffer, str phone) {
	int i = 0;
	for (; i < phone.len; ++i) {
		if (i % 2 == 0)
			phone.s[i] = (buffer[i / 2] & BITMASK_LOW_4BITS) + '0';
	        else
			phone.s[i] = ((buffer[i / 2] & BITMASK_HIGH_4BITS) >> 4) + '0';
	}
	return phone.len;
}

// Decode SMS-Body into the given structure:
int decode_3gpp_sms(struct sip_msg *msg) {
	str body;
	// Parse only the body again, if the mesage differs from the last call:
	if (msg->id != current_msg_id) {
		// Extract Message-body and length: taken from RTPEngine's code
		body.s = get_body(msg);
		if (body.s == 0) {
			LM_ERR("failed to get the message body\n");
			return -1;
		}

		/*
		 * Better use the content-len value - no need of any explicit
		 * parcing as get_body() parsed all headers and Conten-Length
		 * body header is automaticaly parsed when found.
		 */
		if (msg->content_length==0) {
			LM_ERR("failed to get the content length in message\n");
			return -1;
		}

		body.len = get_content_length(msg);
		if (body.len==0) {
			LM_ERR("message body has length zero\n");
			return -1;
		}

		if (body.len + body.s > msg->buf + msg->len) {
			LM_ERR("content-length exceeds packet-length by %d\n",
					(int)((body.len + body.s) - (msg->buf + msg->len)));
			return -1;
		}

		// Get structure for RP-DATA:
		if (!rp_data) {
			rp_data = (sms_rp_data_t*)pkg_malloc(sizeof(struct _sms_rp_data));
			if (!rp_data) {
				LM_ERR("Error allocating %lu bytes!\n", sizeof(struct _sms_rp_data));
				return -1;
			}
		} else {
			freeRP_DATA();
		}

		// Initialize structure:
		memset(rp_data, 0, sizeof(struct _sms_rp_data));

		////////////////////////////////////////////////
		// RP-Data
		////////////////////////////////////////////////
		int p = 0;
		rp_data->msg_type = body.s[p++];
		rp_data->reference = body.s[p++];
		if ((rp_data->msg_type == RP_DATA_MS_TO_NETWORK) || (rp_data->msg_type == RP_DATA_NETWORK_TO_MS)) {
			rp_data->originator.len = body.s[p++];
			if (rp_data->originator.len > 0) {
				p++; // Type of Number, we assume E164, thus ignored
				rp_data->originator.s = pkg_malloc(rp_data->originator.len);
				DecodePhoneNumber(&body.s[p], rp_data->originator);
				p += rp_data->originator.len;
			}
			rp_data->destination.len = body.s[p++];
			if (rp_data->destination.len > 0) {
				p++; // Type of Number, we assume E164, thus ignored
				rp_data->destination.s = pkg_malloc(rp_data->destination.len);
				DecodePhoneNumber(&body.s[p], rp_data->destination);
				p += rp_data->destination.len;
			}

			////////////////////////////////////////////////
			// TPDU
			////////////////////////////////////////////////
			rp_data->pdu_len = body.s[p++];
			if (rp_data->pdu_len > 0) {
				rp_data->pdu.flags = body.s[p++];
				rp_data->pdu.msg_type = rp_data->pdu.flags & 0x03;
				rp_data->pdu.flags = body.s[p++];
				rp_data->pdu.reference = body.s[p++];
				// TP-DA
				rp_data->pdu.destination.len = body.s[p++];
				if (rp_data->pdu.destination.len > 0) {
					p++; // Type of Number, we assume E164, thus ignored
					rp_data->pdu.destination.s = pkg_malloc(rp_data->pdu.destination.len);
					DecodePhoneNumber(&body.s[p], rp_data->pdu.destination);
					p += rp_data->pdu.destination.len;	
				}
				p++; // TP-Protocol Identifier, ignored
				p++; // TP-Data-Coding-Scheme, ignored (default 7 Bit assumed)
				p++; // TP-Validity Period, ignored for now, relative assumes
				rp_data->pdu.payload.len = body.s[p++];
				if (rp_data->pdu.payload.len > 0) {
					rp_data->pdu.payload.s = pkg_malloc(rp_data->pdu.payload.len);
					DecodeSMSMessage(&body.s[p], rp_data->pdu.payload);
					p += rp_data->pdu.payload.len;	
				}
			}				
		}
	}

	return 1;	
}

int dumpRPData(struct sip_msg *msg) {
	// Decode message:
	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}

	LM_INFO("SMS-Message\r\n");
	LM_INFO("------------------------\r\n");
	LM_INFO("RP-Data\r\n");
	LM_INFO("  Type:         %x\r\n", rp_data->msg_type);
	LM_INFO("  Reference:    %x (%i)\r\n", rp_data->reference, rp_data->reference);
	LM_INFO("  Originator:   %.*s\r\n", rp_data->originator.len, rp_data->originator.s);
	LM_INFO("  Destination:  %.*s\r\n", rp_data->destination.len, rp_data->destination.s);
	LM_INFO("T-PDU\r\n");
	LM_INFO("  Type:         %x\r\n", rp_data->pdu.msg_type);
	LM_INFO("  Flags:        %x (%i)\r\n", rp_data->pdu.flags, rp_data->pdu.flags);
	LM_INFO("  Reference:    %x (%i)\r\n", rp_data->pdu.reference, rp_data->pdu.reference);
	LM_INFO("  Destination:  %.*s\r\n", rp_data->pdu.destination.len, rp_data->pdu.destination.s);
	LM_INFO("  Payload:      %.*s\r\n", rp_data->pdu.payload.len, rp_data->pdu.payload.s);
	return 1;
}

/*******************************************************************
 * Implementation
 *******************************************************************/

/*
 * Creates the body for SMS-ACK from the current message
 */
int pv_sms_ack(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	str rp_data_ack = {0, 0};

	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}

	// RP-Type (1) + RP-Ref (1) + RP-User-Data (Element-ID (1) + Length (1 => 9 => Msg-Type (1) + Parameter (1) + Service-Centre-Time (7)) = 13;
	rp_data_ack.len = 13;
	rp_data_ack.s = (char*)pkg_malloc(rp_data_ack.len);
	if (!rp_data_ack.s) {
		LM_ERR("Error allocating %d bytes!\n", rp_data_ack.len);
		return -1;
	}

	// Encode the data (RP-Data)
	// Always ACK NETWORK to MS
	rp_data_ack.s[0] = RP_ACK_NETWORK_TO_MS;
	// Take the reference from request:
	rp_data_ack.s[1] = rp_data->reference;
	
	rp_data_ack.s[2] = 0x41;
	// Length
	rp_data_ack.s[3] = 9;
	// PDU
	// SMS-SUBMIT-Report
	rp_data_ack.s[4] = SUBMIT;
	// Parameters (none)
	rp_data_ack.s[5] = 0x0;

	return pv_get_strval(msg, param, res, &rp_data_ack);
}

/*
 * Dumps the content of the SMS-Message:
 */
int smsdump(struct sip_msg *msg, char *str1, char *str2) {
	return dumpRPData(msg);
}

int isRPDATA(struct sip_msg *msg, char *str1, char *str2) {
	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}
	if ((rp_data->msg_type == RP_DATA_MS_TO_NETWORK) || (rp_data->msg_type == RP_DATA_NETWORK_TO_MS))
		return 1;
	else
		return -1;
}

