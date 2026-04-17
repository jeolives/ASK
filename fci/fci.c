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

#define pr_fmt(fmt) "FCI: " fmt

#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include "fci.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mindspeed Technologies");
MODULE_DESCRIPTION("Fast Control Interface");

static const char fci_version[] = "0.04";

static FCI this_fci_storage;
static FCI * const this_fci = &this_fci_storage;

static int fci_fe_inbound_parser(FCI_MSG *fci_msg, FCI_MSG *fci_rep);
static int fci_fe_register(void);
static void fci_fe_unregister(void);
static void __fci_fe_inbound_data(struct sk_buff *skb);
static int fci_fe_init(void);
static void fci_fe_exit(void);
static int fci_outbound_fe_data(u16 fcode, u16 len, u16 *payload);

/*
 * fci_open_netlink - create NETLINK socket for the given protocol.
 */
static int fci_open_netlink(int proto)
{
	struct netlink_kernel_cfg cfg = {
		.input	= __fci_fe_inbound_data,
		.groups	= 1,
	};

	if (proto != FCI_NL_FF) {
		atomic_long_inc(&this_fci->stats.unknown_sock_type);
		return -ESOCKTNOSUPPORT;
	}

	this_fci->fci_nl_sock[FCI_NL_FF] = netlink_kernel_create(&init_net, NETLINK_FF, &cfg);
	if (!this_fci->fci_nl_sock[FCI_NL_FF]) {
		atomic_long_inc(&this_fci->stats.kernel_create_err);
		return -ENOMEM;
	}
	return 0;
}

static void fci_close_netlink(int proto)
{
	if (proto < 0 || proto >= FCI_MAX_PROTO)
		return;
	netlink_kernel_release(this_fci->fci_nl_sock[proto]);
	this_fci->fci_nl_sock[proto] = NULL;
}

/*
 * fci_outbound_unicast - send a unicast netlink message to userspace.
 */
static void fci_outbound_unicast(int nl_type, struct sk_buff *skb, u32 pid)
{
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;

	netlink_unicast(this_fci->fci_nl_sock[nl_type], skb, pid, MSG_DONTWAIT);

	atomic_long_inc(&this_fci->stats.tx_msg);
	atomic_long_inc(&this_fci->stats.sock_stats[nl_type].tx_msg);
}

/*
 * fci_outbound_multicast - broadcast to listeners of the given group.
 */
static int fci_outbound_multicast(int nl_type, struct sk_buff *skb, int group)
{
	gfp_t allocation = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	int rc = 0;

	NETLINK_CB(skb).portid = 0;

	if (netlink_has_listeners(this_fci->fci_nl_sock[nl_type], group)) {
		NETLINK_CB(skb).dst_group = group;
		rc = netlink_broadcast(this_fci->fci_nl_sock[nl_type], skb, 0, group, allocation);
		if (rc < 0) {
			if (printk_ratelimit())
				pr_err("netlink_broadcast failed: %d\n", rc);
			goto err_exit;
		}
	} else {
		kfree_skb(skb);
	}

	atomic_long_inc(&this_fci->stats.tx_msg);
	atomic_long_inc(&this_fci->stats.sock_stats[nl_type].tx_msg);
	return 0;

err_exit:
	atomic_long_inc(&this_fci->stats.tx_msg_err);
	atomic_long_inc(&this_fci->stats.sock_stats[nl_type].tx_msg_err);
	return rc;
}

/*
 * fci_outbound_err - send an nlmsgerr reply.
 */
static void fci_outbound_err(int nl_type, struct sk_buff *skb, u32 pid,
			     struct nlmsghdr *nlh, int err)
{
	struct nlmsgerr *errmsg;
	struct nlmsghdr *rep;

	rep = __nlmsg_put(skb, pid, nlh->nlmsg_seq,
			  NLMSG_ERROR, sizeof(struct nlmsgerr), 0);

	errmsg = nlmsg_data(rep);
	errmsg->error = err;
	memcpy(&errmsg->msg, nlh, err ? nlh->nlmsg_len : sizeof(*nlh));

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	netlink_unicast(this_fci->fci_nl_sock[nl_type], skb, pid, MSG_DONTWAIT);

	atomic_long_inc(&this_fci->stats.tx_msg);
	atomic_long_inc(&this_fci->stats.sock_stats[nl_type].tx_msg);
}

/****************************** Fast Forward Support ********************************/

static int fci_fe_init(void)
{
	int rc;

	rc = fci_open_netlink(FCI_NL_FF);
	if (rc < 0) {
		pr_err("fci_open_netlink failed (FCI type %d): %d\n", FCI_NL_FF, rc);
		return rc;
	}

	rc = fci_fe_register();
	if (rc < 0) {
		pr_err("fci_fe_register failed: %d\n", rc);
		fci_close_netlink(FCI_NL_FF);
		return rc;
	}
	return 0;
}

static void fci_fe_exit(void)
{
	fci_fe_unregister();
	fci_close_netlink(FCI_NL_FF);
}

static int fci_fe_register(void)
{
	int rc;

	rc = comcerto_fpp_register_event_cb((void *)fci_outbound_fe_data);
	if (rc < 0) {
		pr_err("fpp_register_event_cb failed: %d\n", rc);
		return rc;
	}
	return 0;
}

static void fci_fe_unregister(void)
{
	comcerto_fpp_register_event_cb(NULL);
}

/*
 * fci_alloc_msg - allocate an skb sized for a max-length FCI netlink message.
 */
static struct sk_buff *fci_alloc_msg(void)
{
	gfp_t flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	struct sk_buff *skb;

	skb = nlmsg_new(FCI_MSG_SIZE, flags);
	if (!skb) {
		atomic_long_inc(&this_fci->stats.mem_alloc_err);
		return NULL;
	}
	return skb;
}

/*
 * fci_outbound_fe_data - called by the forward engine to push an event toward userspace.
 */
static int fci_outbound_fe_data(u16 fcode, u16 len, u16 *payload)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	FCI_MSG *fci_msg;

	if (len > FCI_MSG_MAX_PAYLOAD) {
		if (printk_ratelimit())
			pr_err("FPP payload %u exceeds max %u\n", len, FCI_MSG_MAX_PAYLOAD);
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return -EMSGSIZE;
	}

	skb = fci_alloc_msg();
	if (!skb) {
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, len + FCI_MSG_HDR_SIZE, 0);
	if (!nlh) {
		kfree_skb(skb);
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return -EMSGSIZE;
	}

	fci_msg = nlmsg_data(nlh);
	fci_msg->fcode = fcode;
	fci_msg->length = len;
	memcpy(fci_msg->payload, payload, len);

	return fci_outbound_multicast(FCI_NL_FF, skb, NL_FF_GROUP);
}

/*
 * __fci_fe_inbound_data - handle a netlink message from userspace.
 */
static void __fci_fe_inbound_data(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	struct nlmsghdr *rep;
	struct sk_buff *nskb;
	FCI_MSG *fci_msg, *fci_rep;
	int rc;

	/* Limit direct FPP access to privileged callers. */
	if (!netlink_capable(skb, CAP_NET_ADMIN)) {
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return;
	}

	/* Basic skb sanity: header + full FCI_MSG header must fit. */
	if (!NLMSG_OK(nlh, skb->len) ||
	    nlh->nlmsg_len < NLMSG_LENGTH(FCI_MSG_HDR_SIZE)) {
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return;
	}

	fci_msg = nlmsg_data(nlh);

	/* Ensure the declared payload length actually fits in the skb. */
	if (fci_msg->length > FCI_MSG_MAX_PAYLOAD ||
	    nlh->nlmsg_len < NLMSG_LENGTH(FCI_MSG_HDR_SIZE + fci_msg->length)) {
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return;
	}

	atomic_long_inc(&this_fci->stats.rx_msg);
	atomic_long_inc(&this_fci->stats.sock_stats[FCI_NL_FF].rx_msg);

	nskb = fci_alloc_msg();
	if (!nskb) {
		atomic_long_inc(&this_fci->stats.rx_msg_err);
		return;
	}

	rep = nlmsg_put(nskb, NETLINK_CB(skb).portid, nlh->nlmsg_seq, 0, 0, 0);
	fci_rep = nlmsg_data(rep);

	rc = fci_fe_inbound_parser(fci_msg, fci_rep);
	if (rc < 0) {
		nlmsg_cancel(nskb, rep);
		fci_outbound_err(FCI_NL_FF, nskb, NETLINK_CB(skb).portid, nlh, rc);
		atomic_long_inc(&this_fci->stats.rx_msg_err);
	} else {
		skb_put(nskb, FCI_MSG_HDR_SIZE + fci_rep->length);
		nlmsg_end(nskb, rep);
		fci_outbound_unicast(FCI_NL_FF, nskb, NETLINK_CB(skb).portid);
	}
}

static int fci_fe_inbound_parser(FCI_MSG *fci_msg, FCI_MSG *fci_rep)
{
	int rc;

	fci_rep->length = 0;
	rc = comcerto_fpp_send_command(fci_msg->fcode, fci_msg->length, fci_msg->payload,
				       &fci_rep->length, fci_rep->payload);

	if (fci_rep->length > FCI_MSG_MAX_PAYLOAD)
		fci_rep->length = FCI_MSG_MAX_PAYLOAD;

	fci_rep->fcode = fci_msg->fcode;
	return rc;
}

/***************************** MISC FUNCTIONS ********************************/

static int fci_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "\nFCI Messages:\n");
	seq_printf(m, "Sent:%lu\n", atomic_long_read(&this_fci->stats.tx_msg));
	seq_printf(m, "Received:%lu\n", atomic_long_read(&this_fci->stats.rx_msg));
	seq_printf(m, "Sent errors:%lu\n", atomic_long_read(&this_fci->stats.tx_msg_err));
	seq_printf(m, "Received errors:%lu\n", atomic_long_read(&this_fci->stats.rx_msg_err));

	seq_printf(m, "\nFast Forward Messages:\n");
	seq_printf(m, "Sent:%lu\n", atomic_long_read(&this_fci->stats.sock_stats[FCI_NL_FF].tx_msg));
	seq_printf(m, "Received:%lu\n", atomic_long_read(&this_fci->stats.sock_stats[FCI_NL_FF].rx_msg));
	seq_printf(m, "Sent errors:%lu\n", atomic_long_read(&this_fci->stats.sock_stats[FCI_NL_FF].tx_msg_err));
	seq_printf(m, "Received errors:%lu\n", atomic_long_read(&this_fci->stats.sock_stats[FCI_NL_FF].rx_msg_err));

	seq_printf(m, "\nErrors:\n");
	seq_printf(m, "Memory allocation errors:%lu\n", atomic_long_read(&this_fci->stats.mem_alloc_err));
	seq_printf(m, "Kernel socket creation errors:%lu\n", atomic_long_read(&this_fci->stats.kernel_create_err));
	seq_printf(m, "Unknown socket type:%lu\n", atomic_long_read(&this_fci->stats.unknown_sock_type));
	return 0;
}

static int fci_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fci_proc_show, NULL);
}

static const struct proc_ops fci_proc_fops = {
	.proc_open	= fci_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int __init fci_module_init(void)
{
	int rc;

	pr_debug("initializing fast control interface v%s\n", fci_version);

	rc = fci_fe_init();
	if (rc < 0) {
		pr_err("fci init failed: %d\n", rc);
		return rc;
	}

	if (!proc_create("fci", 0, NULL, &fci_proc_fops)) {
		pr_err("failed to create /proc/fci\n");
		fci_fe_exit();
		return -ENOMEM;
	}
	return 0;
}

static void __exit fci_module_exit(void)
{
	pr_debug("unloading fast control interface\n");
	remove_proc_entry("fci", NULL);
	fci_fe_exit();
}

module_init(fci_module_init);
module_exit(fci_module_exit);
