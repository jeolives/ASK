/*    
 *
 *  Copyright (C) 2007 Mindspeed Technologies, Inc.
 *  Copyright 2014-2016 Freescale Semiconductor, Inc.
 *  Copyright 2017,2021 NXP
 *
 * SPDX-License-Identifier:    GPL-2.0+
 * The GPL-2.0+ license for this file can be found in the COPYING.GPL file
 * included with this distribution or at http://www.gnu.org/licenses/gpl-2.0.html
 *
 */

#ifndef _FCI_H
#define _FCI_H

#include <linux/atomic.h>

/*
* Prototypes
*/

/* FPP Forward Engine API*/
extern int comcerto_fpp_send_command(unsigned short fcode, unsigned short length, unsigned short *payload, unsigned short *, unsigned short *);
extern int comcerto_fpp_register_event_cb(void *cb);

/* Supported netlink protocol type NETLINK_FF */
#define FCI_NL_FF		0
#define FCI_MAX_PROTO		1

/* Netlink multicast groups supported by FCI */
#define NL_FF_GROUP	1

/* FCI message definitions*/
#define FCI_MSG_MAX_PAYLOAD	512
#define FCI_MSG_HDR_SIZE 	4 /* fcode + length */
#define FCI_MSG_SIZE		(FCI_MSG_MAX_PAYLOAD + FCI_MSG_HDR_SIZE)

/*
* Structures
*
*/
typedef struct t_FCI_MSG
{
	/* message data */
	u16 fcode;
	u16 length;
	u16 payload[(FCI_MSG_MAX_PAYLOAD / sizeof(u16))];
} FCI_MSG;


typedef struct t_FCI_SOCK_STATS
{
	atomic_long_t tx_msg;
	atomic_long_t rx_msg;
	atomic_long_t tx_msg_err;
	atomic_long_t rx_msg_err;
} FCI_SOCK_STATS;


typedef struct t_FCI_STATS
{
	/* Globals Statistics*/
	atomic_long_t tx_msg;
	atomic_long_t rx_msg;
	atomic_long_t tx_msg_err;
	atomic_long_t rx_msg_err;
	atomic_long_t mem_alloc_err;
	atomic_long_t kernel_create_err;
	atomic_long_t unknown_sock_type;
	/* Per socket type statistics*/
	FCI_SOCK_STATS sock_stats[FCI_MAX_PROTO];
} FCI_STATS;


typedef struct t_FCI
{
	struct sock *fci_nl_sock[FCI_MAX_PROTO];
	FCI_STATS stats;
} FCI;



#endif /* _FCI_H */
