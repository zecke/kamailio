/* Module option to use display name, SIP uri or both in messages */

#!define IMC_MSG_PREFIX		"***"				/* Prefix before every message */
#!define IMC_THEROOM		"Room"
#!define IMC_WASCREATED		"Room was created"		/* Room was created */
#!define IMC_ATTEMPTJOIN	"attempted to join the room"	/* attempted to join the room */
#!define IMC_MEMBERNAME		"member name"			/* member name */
#!define IMC_TRUNCATED		"truncated"			/* truncated */

#!define IMC_INVITEFROM		"INVITE from"			/* INVITE from <id> */
#!define IMC_ACC_DENY		"(Type: '#accept' or '#deny')"	/* Accept or deny */
#!define IMC_JOINED		"has joined the room"		/* <id> Has joined the room */
#!define IMC_REMOVED		"You have been removed from this room"	/* imc_handle_remove: You have been removed from this room */
#!define IMC_LEFT		"has left the room"		/* imc_handle_remove: <id> has left the room */
#!define IMC_DENIED		"declined invitation in room"	/* imc_handle_deny: <id> declined invitation in room */
#!define IMC_NOTINVITED		"was not invited in room"	/* imc_handle_deny: <id> was not invited in room <room> */

#!define IMC_MEMBERS		"Members"			/* imc_handle_list: Members */

#!define IMC_ROOMDESTROYED	"The room has been destroyed"	/* imc_handle_exit: imc_handle_destroy: The room has been destroyed */
#!define IMC_USERLEFT		"has left the room"		/* imc_handle_exit: <id> has left the room */

#!define IMC_INVALIDCMD		"invalid command"		/* imc_handle_unknown: invalid command */
#!define IMC_HELP		"help for assistance"		/* imc_handle_unknown: help for assistance */

/* imc_handle_message */


/* Commands: imc_cmd.c */
#!define IMC_CMD_CREATE		"create"
#!define IMC_CMD_JOIN		"join"
#!define IMC_CMD_INVITE		"invite"
#!define IMC_CMD_ACCEPT		"accept"
#!define IMC_CMD_DENY		"deny"
#!define IMC_CMD_REMOVE		"remove"
#!define IMC_CMD_EXIT		"exit"
#!define IMC_CMD_LIST		"list"
#!define IMC_CMD_DESTROY	"destroy"
#!define IMC_CMD_HELP		"help"

#!define IMC_CMD_C_PRIVATE	"private"	/* Create private room */
