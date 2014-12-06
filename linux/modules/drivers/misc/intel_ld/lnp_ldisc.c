/*
 * ST Driver
 *
 * Copyright (C) 2013 Intel Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/lbf_ldisc.h>
#include <linux/poll.h>

#define LL_SLEEP_ACK    0xF1
#define LL_WAKE_UP_ACK  0xF0

/*---- HCI data types ------------*/
#define HCI_COMMAND_PKT 0x01
#define HCI_ACLDATA_PKT 0x02
#define HCI_SCODATA_PKT 0x03
#define HCI_EVENT_PKT   0x04

/* ---- HCI Packet structures ---- */
#define HCI_COMMAND_HDR_SIZE    3
#define HCI_EVENT_HDR_SIZE      2
#define HCI_ACL_HDR_SIZE	4
#define HCI_SCO_HDR_SIZE	3

#define INVALID -1
#define HCI_COMMAND_COMPLETE_EVENT      0x0E
#define HCI_INTERRUPT_EVENT	     0xFF
#define HCI_COMMAND_STATUS_EVT	  0x0F
#define FMR_DEBUG_EVENT		 0x2B

/*----------- FMR Command ----*/
#define FMR_WRITE       0xFC58
#define FMR_READ	0xFC59
#define FMR_SET_POWER   0xFC5A
#define FMR_SET_AUDIO   0xFC5B
#define FMR_IRQ_CONFIRM 0xFC5C
#define FMR_TOP_WRITE   0xFC5D
#define FMR_TOP_READ    0xFC5E

#define STREAM_TO_UINT16(u16, p) { (p) += 4; u16 = ((unsigned int)(*(p)) + \
					(((unsigned int)(*((p) + 1))) << 8)); }
#define MAX_BT_CHNL_IDS 4

/* ----- HCI receiver states ---- */
#define LBF_W4_H4_HDR   0
#define LBF_W4_PKT_HDR  1
#define LBF_W4_DATA     2

#define N_INTEL_LDISC_BUF_SIZE	  4096
#define TTY_THRESHOLD_THROTTLE	  128 /* now based on remaining room */
#define TTY_THRESHOLD_UNTHROTTLE	128

enum proto_type {
	LNP_BT,
	LNP_FM = 5,
};

static unsigned int lbf_ldisc_running = -1;

/*------ Forward Declaration-----*/
struct sk_buff *lbf_dequeue(void);
static int lbf_tty_write(struct tty_struct *tty, const unsigned char *data,
				int count);
static void lbf_enqueue(struct sk_buff *skb);

struct st_proto_s {
	enum proto_type type;
	unsigned char chnl_id;
	unsigned char hdr_len;
	unsigned char offset_len_in_hdr;
	unsigned char len_size;
	unsigned char reserve;
};

static struct st_proto_s lnp_st_proto[6] = {
		{ .chnl_id = INVALID, /* ACL */
		.hdr_len = INVALID, .offset_len_in_hdr = INVALID,
				.len_size = INVALID, .reserve = INVALID, },
		{ .chnl_id = INVALID, /* ACL */
		.hdr_len = INVALID, .offset_len_in_hdr = INVALID,
				.len_size = INVALID, .reserve = INVALID, },
		{ .chnl_id = HCI_ACLDATA_PKT, /* ACL */
		.hdr_len = HCI_ACL_HDR_SIZE, .offset_len_in_hdr = 3,
				.len_size = 2, .reserve = 8, },
		{ .chnl_id = HCI_SCODATA_PKT, /* SCO */
		.hdr_len = HCI_SCO_HDR_SIZE, .offset_len_in_hdr = 3,
				.len_size = 1, .reserve = 8, },
		{ .chnl_id = HCI_EVENT_PKT, /* HCI Events */
		.hdr_len = HCI_EVENT_HDR_SIZE, .offset_len_in_hdr = 2,
				.len_size = 1, .reserve = 8, },
		{ .chnl_id = HCI_EVENT_PKT, /* HCI Events */
		.hdr_len = HCI_EVENT_HDR_SIZE, .offset_len_in_hdr = 2,
				.len_size = 1, .reserve = 8, }, };

struct lbf_q_tx {
	struct sk_buff_head txq, tx_waitq;
	struct sk_buff *tx_skb;
	spinlock_t lock;
	struct mutex writelock;
	struct tty_struct *tty;
} lbf_q_tx;

static struct lbf_q_tx *lbf_tx;

static struct fm_ld_drv_register *fm[1];

static int fw_isregister;

struct lnp_uart {

	const struct firmware *fw;
	unsigned int ld_installed;
	unsigned int lbf_rcv_state; /* Packet receive state information */
	unsigned int fmr_evt_rcvd;
	unsigned int lbf_ldisc_running;

	char *read_buf;
	int read_head;
	int read_cnt;
	int read_tail;
	wait_queue_t wait;
	wait_queue_head_t read_wait;
	struct mutex atomic_read_lock;
	struct mutex  lock;

	unsigned long rx_state;
	struct tty_struct *tty;
	spinlock_t rx_lock;
	struct sk_buff *rx_skb;
	unsigned long rx_count;
	struct fm_ld_drv_register *pfm_ld_drv_register;

	unsigned short rx_chnl;
	unsigned short type;
	unsigned short bytes_pending;
	unsigned short len;

};

static void n_tty_set_room(struct tty_struct *tty);

static void check_unthrottle(struct tty_struct *tty)
{
	if (tty->count)
		tty_unthrottle(tty);
}

static void reset_buffer_flags(struct tty_struct *tty)
{
	struct lnp_uart *plbf_ldisc;
	pr_info("-> %s\n", __func__);
	plbf_ldisc = tty->disc_data;
	mutex_lock(&plbf_ldisc->lock);
	plbf_ldisc->read_head = plbf_ldisc->read_cnt = plbf_ldisc->read_tail
			= 0;
	mutex_unlock(&plbf_ldisc->lock);
	n_tty_set_room(tty);
	check_unthrottle(tty);

}

int lbf_tx_wakeup(struct lnp_uart *plnp_uart)
{
	struct sk_buff *skb;
	int len = 0;
	pr_info("-> %s\n", __func__);
	while ((skb = lbf_dequeue())) {

		mutex_lock(&lbf_tx->writelock);
		if (lbf_tx->tty) {
			len = lbf_tty_write((void *) lbf_tx->tty, skb->data,
					skb->len);
			skb_pull(skb, len);
			if (skb->len) {
				lbf_tx->tx_skb = skb;
				mutex_unlock(&lbf_tx->writelock);
				break;
			}
			kfree_skb(skb);
			mutex_unlock(&lbf_tx->writelock);
		} else
			mutex_unlock(&lbf_tx->writelock);
	}

	return len;
}

/* This is the internal write function - a wrapper
 * to tty->ops->write
 */
int lbf_tty_write(struct tty_struct *tty, const unsigned char *data, int count)
{
	struct lnp_uart *plnp_uart;
	plnp_uart = tty->disc_data;
	return plnp_uart->tty->ops->write(tty, data, count);
}

long lbf_write(struct sk_buff *skb)
{
	long len = skb->len;
	pr_info("-> %s\n", __func__);

	/*ST to decide where to enqueue the skb */
	lbf_enqueue(skb);
	/* wake up */
	lbf_tx_wakeup(NULL);

	pr_info("<- %s\n", __func__);

	/* return number of bytes written */
	return len;
}

/* for protocols making use of shared transport */

long unregister_fmdrv_from_ld_driv(struct fm_ld_drv_register *fm_ld_drv_reg)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&lbf_tx->lock, flags);
	fw_isregister = 0;
	spin_unlock_irqrestore(&lbf_tx->lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(unregister_fmdrv_from_ld_driv);

long register_fmdrv_to_ld_driv(struct fm_ld_drv_register *fm_ld_drv_reg)
{
	unsigned long flags;
	long err = 0;
	fm[0] = NULL;
	pr_info("-> %s\n", __func__);

	if (lbf_ldisc_running == 1) {

		if (fm_ld_drv_reg == NULL || fm_ld_drv_reg->fm_cmd_handler
				== NULL
				|| fm_ld_drv_reg->ld_drv_reg_complete_cb
						== NULL) {
			pr_info("fm_ld_drv_register/fm_cmd_handler or "
				"reg_complete_cb not ready");
			return -EINVAL;
		}
		spin_lock_irqsave(&lbf_tx->lock, flags);
		fm_ld_drv_reg->fm_cmd_write = lbf_write;
		fm[0] = fm_ld_drv_reg;
		fw_isregister = 1;
		spin_unlock_irqrestore(&lbf_tx->lock, flags);

		if (fm[0]) {
			if (likely(fm[0]->ld_drv_reg_complete_cb != NULL)) {
				fm[0]->ld_drv_reg_complete_cb(fm[0]->priv_data,
						0);
				pr_info("Registration called");

			}
		}
	} else
		err = -EINVAL;

	return err;
}
EXPORT_SYMBOL_GPL(register_fmdrv_to_ld_driv);

static void lbf_ldisc_flush_buffer(struct tty_struct *tty)
{
	/* clear everything and unthrottle the driver */

	unsigned long flags;
	pr_info("-> %s\n", __func__);
	reset_buffer_flags(tty);
	n_tty_set_room(tty);

	if (tty->link) {
		spin_lock_irqsave(&tty->ctrl_lock, flags);
		if (tty->link->packet) {
			tty->ctrl_status |= TIOCPKT_FLUSHREAD;
			wake_up_interruptible(&tty->link->read_wait);
		}
		spin_unlock_irqrestore(&tty->ctrl_lock, flags);
	}
	pr_info("<- %s\n", __func__);
}

/**
 * lbf_dequeue - internal de-Q function.
 * If the previous data set was not written
 * completely, return that skb which has the pending data.
 * In normal cases, return top of txq.
 */
struct sk_buff *lbf_dequeue(void)
{
	struct sk_buff *returning_skb;

	if (lbf_tx->tx_skb != NULL) {
		returning_skb = lbf_tx->tx_skb;
		lbf_tx->tx_skb = NULL;
		return returning_skb;
	}
	return skb_dequeue(&lbf_tx->txq);
}

/**
 * lbf_enqueue - internal Q-ing function.
 * Will either Q the skb to txq or the tx_waitq
 * depending on the ST LL state.
 * txq needs protection since the other contexts
 * may be sending data, waking up chip.
 */
void lbf_enqueue(struct sk_buff *skb)
{
	mutex_lock(&lbf_tx->writelock);
	skb_queue_tail(&lbf_tx->txq, skb);
	mutex_unlock(&lbf_tx->writelock);
}

/* lbf_ldisc_open
 *
 * Called when line discipline changed to HCI_UART.
 *
 * Arguments:
 * tty pointer to tty info structure
 * Return Value:
 * 0 if success, otherwise error code
 */
static int lbf_ldisc_open(struct tty_struct *tty)
{

	struct lnp_uart *lnp_uart;
	pr_info("-> %s\n", __func__);
	if (tty->disc_data) {
		pr_info("%s ldiscdata exist\n ", __func__);
		return -EEXIST;
	}
	/* Error if the tty has no write op instead of leaving an exploitable
	 hole */
	if (tty->ops->write == NULL) {
		pr_info("%s write = NULL\n ", __func__);
		return -EOPNOTSUPP;
	}

	lnp_uart = kzalloc(sizeof(struct lnp_uart), GFP_KERNEL);
	if (!lnp_uart) {
		pr_info(" kzalloc for lnp_uart failed\n ");
		tty_unregister_ldisc(N_INTEL_LDISC);
		return -ENOMEM;
	}

	lbf_tx = kzalloc(sizeof(struct lbf_q_tx), GFP_KERNEL);
	if (!lbf_tx) {
		pr_info(" kzalloc for lbf_tx failed\n ");
		kfree(lnp_uart);

		tty_unregister_ldisc(N_INTEL_LDISC);
		return -ENOMEM;
	}

	tty->disc_data = lnp_uart;
	lnp_uart->tty = tty;
	lbf_tx->tty = tty;
	skb_queue_head_init(&lbf_tx->txq);
	skb_queue_head_init(&lbf_tx->tx_waitq);
	/* don't do an wakeup for now */
	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	if (likely(!lnp_uart->read_buf)) {
		lnp_uart->read_buf
				= kzalloc(N_INTEL_LDISC_BUF_SIZE, GFP_KERNEL);

		if (!lnp_uart->read_buf) {
			kfree(lnp_uart);

			kfree(lbf_tx);

			tty_unregister_ldisc(N_INTEL_LDISC);
			return -ENOMEM;
		}
	}

	tty->receive_room = 65536;
	spin_lock_init(&lnp_uart->rx_lock);
	spin_lock_init(&lbf_tx->lock);
	mutex_init(&lnp_uart->lock);
	mutex_init(&lbf_tx->writelock);

	set_bit(TTY_DO_WRITE_WAKEUP, &lnp_uart->tty->flags);
	init_waitqueue_head(&lnp_uart->read_wait);
	mutex_init(&lnp_uart->atomic_read_lock);
	lbf_ldisc_running = 1;
	/* Flush any pending characters in the driver and line discipline. */
	reset_buffer_flags(tty);

	if (tty->ldisc->ops->flush_buffer)
		tty->ldisc->ops->flush_buffer(tty);
	tty_driver_flush_buffer(tty);

	return 0;
}

/* lbf_ldisc_close()
 *
 * Called when the line discipline is changed to something
 * else, the tty is closed, or the tty detects a hangup.
 */

static void lbf_ldisc_close(struct tty_struct *tty)
{
	struct lnp_uart *plnp_uart = (void *) tty->disc_data;
	tty->disc_data = NULL; /* Detach from the tty */
	pr_info("-> %s\n", __func__);

	skb_queue_purge(&lbf_tx->txq);
	skb_queue_purge(&lbf_tx->tx_waitq);
	plnp_uart->rx_count = 0;
	plnp_uart->rx_state = LBF_W4_H4_HDR;

	kfree_skb(plnp_uart->rx_skb);
	kfree(plnp_uart->read_buf);
	plnp_uart->rx_skb = NULL;
	lbf_ldisc_running = -1;

	kfree(lbf_tx);
	kfree(plnp_uart);
}

/* lbf_ldisc_wakeup()
 *
 * Callback for transmit wakeup. Called when low level
 * device driver can accept more send data.
 *
 * Arguments: tty pointer to associated tty instance data
 * Return Value: None
 */
static void lbf_ldisc_wakeup(struct tty_struct *tty)
{

	struct lnp_uart *plnp_uart = (void *) tty->disc_data;
	pr_info("--> lnp_ldisc_wakeup");

	clear_bit(TTY_DO_WRITE_WAKEUP, &plnp_uart->tty->flags);

	/*lbf_tx_wakeup(NULL);*/

}

/* select_proto()
 * Note the HCI Header type
 * Arguments : type: the 1st byte in the packet
 * Return Type: The Corresponding type in the array defined
 * for all headers
 */

static int select_proto(int type)
{
	int j, proto = INVALID;
	/*The structure lnp_st_proto we have allocated has 6 member
	according to H4 Header except 0 & 1. 5 is for FM Packet */
	for (j = 2; j < 6; j++) {
		if (type == (j)) {
			proto = j;
			break;
		}
	}

	return proto;
}

/* st_send_frame()
 * push the skb received to relevant
 * protocol stacks
 * Arguments : tty pointer to associated tty instance data
 lnp_uart : Disc data for tty
 chnl_id : id to either BT or FMR
 * Return Type: void
 */
static void st_send_frame(struct tty_struct *tty, struct lnp_uart *lnp_uart)
{
	unsigned int i = 0;
	int chnl_id1 = LNP_BT;
	unsigned char *buff;
	unsigned long flags = 0;
	unsigned int opcode = 0;
	unsigned int count = 0;

	if (unlikely(lnp_uart == NULL || lnp_uart->rx_skb == NULL)) {
		pr_info(" No channel registered, no data to send?");
		kfree_skb(lnp_uart->rx_skb);
		return;
	}
	buff = &lnp_uart->rx_skb->data[0];
	count = lnp_uart->rx_skb->len;
	STREAM_TO_UINT16(opcode, buff);
	pr_info("opcode : 0x%x event code: 0x%x registered", opcode,
			 lnp_uart->rx_skb->data[1]);
	/* for (i = lnp_uart->rx_skb->len, j = 0; i > 0; i--, j++)
	 printk (KERN_ERR " --%d : 0x%x " ,j ,lnp_uart->rx_skb->data[j]);*/

	if (HCI_COMMAND_COMPLETE_EVENT == lnp_uart->rx_skb->data[1]) {
		switch (opcode) {
		case FMR_IRQ_CONFIRM:
		case FMR_SET_POWER:
		case FMR_READ:
		case FMR_WRITE:
		case FMR_SET_AUDIO:
		case FMR_TOP_READ:
		case FMR_TOP_WRITE:
			chnl_id1 = LNP_FM;
			pr_info("Its FM event ");
			break;
		default:
			chnl_id1 = LNP_BT;
			pr_info("Its BT event ");
			break;
		}
	}
	if (HCI_INTERRUPT_EVENT == lnp_uart->rx_skb->data[1]
			&& (lnp_uart->rx_skb->data[3] == FMR_DEBUG_EVENT))
		chnl_id1 = LNP_FM;

	if (chnl_id1 == LNP_FM) {
		if (likely(fm[0])) {
			if (likely(fm[0]->fm_cmd_handler != NULL)) {
				if (unlikely(fm[0]->fm_cmd_handler(fm[0]->
					priv_data, lnp_uart->rx_skb) != 0))
					pr_info("proto stack %d recv failed"
						, chnl_id1);
			}
		}
	else
		pr_info("fm is NULL ");
	} else if (chnl_id1 == LNP_BT) {
		pr_info(" Added in buffer inside count %d readhead %d ", count
			, lnp_uart->read_head);
		spin_lock_irqsave(&lbf_tx->lock, flags);
		i = min3(N_INTEL_LDISC_BUF_SIZE - lnp_uart->read_cnt,
				N_INTEL_LDISC_BUF_SIZE - lnp_uart->read_head,
				(int)lnp_uart->rx_skb->len);
		memcpy(lnp_uart->read_buf + lnp_uart->read_head,
			 &lnp_uart->rx_skb->data[0], i);
		lnp_uart->read_head =
			(lnp_uart->read_head + i) & (N_INTEL_LDISC_BUF_SIZE-1);
		lnp_uart->read_cnt += i;
		spin_unlock_irqrestore(&lbf_tx->lock, flags);
		count = count - i;

		if (unlikely(count)) {
			pr_info(" Added in buffer inside count %d readhead %d"
				, count , lnp_uart->read_head);
			spin_lock_irqsave(&lbf_tx->lock, flags);
			i = min3(N_INTEL_LDISC_BUF_SIZE - lnp_uart->read_cnt,
				N_INTEL_LDISC_BUF_SIZE - lnp_uart->read_head,
				(int)count);
			memcpy(lnp_uart->read_buf + lnp_uart->read_head,
			&lnp_uart->rx_skb->data[lnp_uart->rx_skb->len - count],
				i);
			lnp_uart->read_head =
			(lnp_uart->read_head + i) & (N_INTEL_LDISC_BUF_SIZE-1);
			lnp_uart->read_cnt += i;
			spin_unlock_irqrestore(&lbf_tx->lock, flags);
		}
	/*printk(KERN_DEBUG " After Added in buffer lnp_uart->read_cnt %d
	 readhead %d ", lnp_uart->read_cnt , lnp_uart->read_head);*/
	}

	if (lnp_uart->rx_skb)
		kfree_skb(lnp_uart->rx_skb);
	lnp_uart->rx_state = LBF_W4_H4_HDR;
	lnp_uart->rx_skb = NULL;
	lnp_uart->rx_count = 0;
	lnp_uart->rx_chnl = 0;
	lnp_uart->bytes_pending = 0;
}

/* st_check_data_len()
 * push the skb received to relevant
 * protocol stacks
 * Arguments : tty pointer to associated tty instance data
 lnp_uart : Disc data for tty
 len : lenn of data
 * Return Type: void
 */

static inline int st_check_data_len(struct lnp_uart *lnp_uart, int len)
{
	int room = skb_tailroom(lnp_uart->rx_skb);

	if (!len) {
		/* Received packet has only packet header and
		 * has zero length payload. So, ask ST CORE to
		 * forward the packet to protocol driver (BT/FM/GPS)
		 */
		st_send_frame(lnp_uart->tty, lnp_uart);

	} else if (len > room) {
		/* Received packet's payload length is larger.
		 * We can't accommodate it in created skb.
		 */
		pr_info("Data length is too large len %d room %d", len, room);
		kfree_skb(lnp_uart->rx_skb);
	} else {
		/* Packet header has non-zero payload length and
		 * we have enough space in created skb. Lets read
		 * payload data */

		lnp_uart->rx_state = LBF_W4_DATA;
		lnp_uart->rx_count = len;

		return len;
	}

	/* Change LDISC state to continue to process next
	 * packet */
	lnp_uart->rx_state = LBF_W4_H4_HDR;
	lnp_uart->rx_skb = NULL;
	lnp_uart->rx_count = 0;
	lnp_uart->rx_chnl = 0;
	lnp_uart->bytes_pending = 0;

	return 0;
}

/**
 * n_tty_set__room - receive space
 * @tty: terminal
 *
 * Called by the driver to find out how much data it is
 * permitted to feed to the line discipline without any being lost
 * and thus to manage flow control. Not serialized. Answers for the
 * "instant".
 */

static void n_tty_set_room(struct tty_struct *tty)
{
	/* tty->read_cnt is not read locked ? */

	struct lnp_uart *p_lnp_uart = (struct lnp_uart *) tty->disc_data;
	int left = N_INTEL_LDISC_BUF_SIZE - p_lnp_uart->read_cnt - 1;
	int old_left;

	old_left = tty->receive_room;
	tty->receive_room = left;

	/* Did this open up the receive buffer? We may need to flip */
	if (left && !old_left) {
		WARN_RATELIMIT(tty->port->itty == NULL,
				"scheduling with invalid itty\n");
		/* see if ldisc has been killed - if so, this means that
		 * even though the ldisc has been halted and ->buf.work
		 * cancelled, ->buf.work is about to be rescheduled
		 */
		WARN_RATELIMIT(test_bit(TTY_LDISC_HALTED, &tty->flags),
				"scheduling buffer work for halted ldisc\n");
		schedule_work(&tty->port->buf.work);
	}
}

static int packet_state(int exp_count, int recv_count)
{
	int status = 0;
	if (exp_count > recv_count)
		status = exp_count - recv_count;

	return status;
}

/* lbf_ldisc_receive()
 *
 * Called by tty low level driver when receive data is
 * available.
 *
 * Arguments: tty pointer to tty isntance data
 * data pointer to received data
 * flags pointer to flags for data
 * count count of received data in bytes
 *
 * Return Value: None
 */
static void lbf_ldisc_receive(struct tty_struct *tty, const u8 *cp, char *fp,
		int count)
{

	unsigned char *ptr;
	int proto = INVALID;
	struct sk_buff *prx_skb;
	int pkt_status;
	unsigned short payload_len = 0;
	int len = 0, type = 0, i = 0;
	unsigned char *plen;
	struct lnp_uart *p_lnp_uart = (struct lnp_uart *) tty->disc_data;
	unsigned long flags;

	pr_info("-> %s\n", __func__);
	print_hex_dump(KERN_DEBUG, ">in>", DUMP_PREFIX_NONE, 16, 1, cp, count,
			0);
	ptr = (unsigned char *) cp;
	prx_skb = alloc_skb(255, GFP_ATOMIC);
	/* tty_receive sent null ?*/
	if (unlikely(ptr == NULL) || unlikely(p_lnp_uart == NULL)) {
		pr_info(" received null from TTY ");
		kfree_skb(prx_skb);
		return;
	}
	spin_lock_irqsave(&p_lnp_uart->rx_lock, flags);

	pr_info("-> %s count: %d p_lnp_uart->rx_state: %ld\n", __func__ , count,
		 p_lnp_uart->rx_state);

	while (count) {
		if (likely(p_lnp_uart->bytes_pending)) {
			len = min_t(unsigned int, p_lnp_uart->bytes_pending,
				 count);
			memcpy(skb_put(prx_skb, len), ptr, len);
			p_lnp_uart->rx_count = p_lnp_uart->bytes_pending;
			memcpy(skb_put(p_lnp_uart->rx_skb, len),
					&prx_skb->data[0], len);
		} else if ((p_lnp_uart->rx_count)) {
			len = min_t(unsigned int, p_lnp_uart->rx_count, count);
			memcpy(skb_put(prx_skb, len), ptr, len);
			memcpy(skb_put(p_lnp_uart->rx_skb, len),
					&prx_skb->data[0], len);
		}

		switch (p_lnp_uart->rx_state) {
		case LBF_W4_H4_HDR:
			if (*ptr == 0xFF) {
				pr_info(" Discard a byte 0x%x\n" , *ptr);
				ptr++;
				count = count - 1;
				continue;
			}
			switch (*ptr) {
			case LL_SLEEP_ACK:
				pr_debug("PM packet");
				continue;
			case LL_WAKE_UP_ACK:
				pr_debug("PM packet");
				continue;
			default:
				type = *ptr;
				proto = select_proto(type);
				if (proto != INVALID) {
					p_lnp_uart->rx_skb = alloc_skb(1700,
						GFP_ATOMIC);
					p_lnp_uart->rx_chnl = type;
					p_lnp_uart->rx_state = LBF_W4_PKT_HDR;
					p_lnp_uart->rx_count =
				lnp_st_proto[p_lnp_uart->rx_chnl].hdr_len + 1;
				} else {
					pr_info("Invalid header type: 0x%x\n",
					 type);
					count = 0;
					break;
				}
				continue;
			}
			break;

		case LBF_W4_DATA:
			pkt_status = packet_state(p_lnp_uart->rx_count, count);
			count -= len;
			ptr += len;
			i = 0;
			p_lnp_uart->rx_count -= len;
			if (!pkt_status) {
				pr_info("\n---------Complete pkt received-----"
						"--------\n");
				p_lnp_uart->rx_state = LBF_W4_H4_HDR;
				st_send_frame(tty, p_lnp_uart);
				p_lnp_uart->bytes_pending = 0;
				p_lnp_uart->rx_count = 0;
				skb_pull(prx_skb, prx_skb->len);
				p_lnp_uart->rx_skb = NULL;
			} else {
				p_lnp_uart->bytes_pending = pkt_status;
				count = 0;
			}
			continue;

		case LBF_W4_PKT_HDR:
			pkt_status = packet_state(p_lnp_uart->rx_count, count);
			pr_info("%s: p_lnp_uart->rx_count %ld\n", __func__,
				p_lnp_uart->rx_count);
			count -= len;
			ptr += len;
			p_lnp_uart->rx_count -= len;
			if (pkt_status) {
				p_lnp_uart->bytes_pending = pkt_status;
				count = 0;
			} else {
				plen = &p_lnp_uart->rx_skb->data
			[lnp_st_proto[p_lnp_uart->rx_chnl].offset_len_in_hdr];
				p_lnp_uart->bytes_pending = pkt_status;
				if (lnp_st_proto[p_lnp_uart->rx_chnl].len_size
					== 1)
					payload_len = *(unsigned char *)plen;
				else if (lnp_st_proto[p_lnp_uart->rx_chnl].len_size == 2)
					payload_len =
					__le16_to_cpu(*(unsigned short *)plen);
				else
					pr_info("%s: invalid length "
						"for id %d\n", __func__, proto);

				st_check_data_len(p_lnp_uart, payload_len);
				skb_pull(prx_skb, prx_skb->len);
				}
			continue;
		} /* end of switch rx_state*/
	} /* end of while*/

	if (count == 0)
		if (prx_skb != NULL) {
			kfree_skb(prx_skb);
			prx_skb = NULL;
		}

	spin_unlock_irqrestore(&p_lnp_uart->rx_lock, flags);
	n_tty_set_room(tty);

	if (waitqueue_active(&p_lnp_uart->read_wait)) {
		pr_info("-> %s waitqueue_active\n", __func__);
		wake_up_interruptible(&p_lnp_uart->read_wait);
	}

	if (tty->receive_room < TTY_THRESHOLD_THROTTLE)
		tty_throttle(tty);

	pr_info("<- %s\n", __func__);
}

/* lbf_ldisc_ioctl()
 *
 * Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * cmd IOCTL command code
 * arg argument for IOCTL call (cmd dependent)
 *
 * Return Value: Command dependent
 */
static int lbf_ldisc_ioctl(struct tty_struct *tty, struct file *file,
		unsigned int cmd, unsigned long arg)
{

	int err = 0;
	pr_info("-> %s\n", __func__);
	switch (cmd) {
	default:
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
	}
	return err;
}

/* copy_from_read_buf()
 *
 * Internal function for lbf_ldisc_read().
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * buf buf to copy to user space
 * nr number of bytes to be read
 *
 * Return Value: number of bytes copy to user spcae succesfully
 */
static int copy_from_read_buf(struct tty_struct *tty,
		unsigned char __user **b,
		size_t *nr)
{
	int retval;
	size_t n;
	unsigned long flags;
	struct lnp_uart *p_lnp_uart = (struct lnp_uart *)tty->disc_data;
	retval = 0;
	spin_lock_irqsave(&lbf_tx->lock, flags);
	n = min3(p_lnp_uart->read_cnt,
		 N_INTEL_LDISC_BUF_SIZE - p_lnp_uart->read_tail, (int)*nr);
	spin_unlock_irqrestore(&lbf_tx->lock, flags);
	if (n) {
		retval =
		copy_to_user(*b, &p_lnp_uart->read_buf[p_lnp_uart->read_tail],
			n);
		n -= retval;
		spin_lock_irqsave(&lbf_tx->lock, flags);
		p_lnp_uart->read_tail =
		(p_lnp_uart->read_tail + n) & (N_INTEL_LDISC_BUF_SIZE-1);
		p_lnp_uart->read_cnt -= n;
		spin_unlock_irqrestore(&lbf_tx->lock, flags);
		*b += n;
	}
	return n;
}

/* lbf_ldisc_read()
 *
 * Process read system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * buf buf to copy to user space
 * nr number of bytes to be read
 *
 * Return Value: number of bytes read copied to user space
 * succesfully
 */
static ssize_t lbf_ldisc_read(struct tty_struct *tty, struct file *file,
		unsigned char __user *buf, size_t nr)
{
	unsigned char __user *b = buf;
	ssize_t size;
	int retval;
	int state = -1;
	struct lnp_uart *p_lnp_uart = (struct lnp_uart *)tty->disc_data;
	pr_info("-> %s\n", __func__);
	init_waitqueue_entry(&p_lnp_uart->wait, current);

	if (!p_lnp_uart->read_cnt) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else {
			if (mutex_lock_interruptible(
				&p_lnp_uart->atomic_read_lock))
				return -ERESTARTSYS;
			else
				state = 0;
			}
	}

	n_tty_set_room(tty);

	add_wait_queue(&p_lnp_uart->read_wait, &p_lnp_uart->wait);
	while (nr) {
		if (p_lnp_uart->read_cnt > 0) {
			int copied;
			__set_current_state(TASK_RUNNING);
			/* The copy function takes the read lock and handles
			 locking internally for this case */
			copied = copy_from_read_buf(tty, &b, &nr);
			copied += copy_from_read_buf(tty, &b, &nr);
			if (copied) {
				retval = 0;
				if (p_lnp_uart->read_cnt <=
					TTY_THRESHOLD_UNTHROTTLE) {
					n_tty_set_room(tty);
					check_unthrottle(tty);
				}
			break;
			}
		} else {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}
	}

	if (state == 0)
	       mutex_unlock(&p_lnp_uart->atomic_read_lock);


	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&p_lnp_uart->read_wait, &p_lnp_uart->wait);

	size = b - buf;
	if (size)
		retval = size;
	n_tty_set_room(tty);
	pr_info("<- %s\n", __func__);
	return retval;
}

/* lbf_ldisc_write()
 *
 * Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * buf data to pass down
 * nr number of bytes to be written down
 *
 * Return Value: no of bytes written succesfully
 */
static ssize_t lbf_ldisc_write(struct tty_struct *tty, struct file *file,
		const unsigned char *data, size_t count)
{
	struct sk_buff *skb = alloc_skb(count + 1, GFP_ATOMIC);
	int len = 0;
	pr_info("-> %s\n", __func__);

	print_hex_dump(KERN_DEBUG, "<out<", DUMP_PREFIX_NONE, 16, 1, data,
			count, 0);

	memcpy(skb_put(skb, count), data, count);
	lbf_enqueue(skb);

	len = lbf_tx_wakeup(NULL);

	pr_info("<- %s\n", __func__);
	return len;
}

static unsigned int lbf_ldisc_poll(struct tty_struct *tty, struct file *file,
		poll_table *wait)
{
	unsigned int mask = 0;
	struct lnp_uart *p_lnp_uart = (struct lnp_uart *) tty->disc_data;

	pr_info("-> %s\n", __func__);

	if (p_lnp_uart->read_cnt > 0)
		mask |= POLLIN | POLLRDNORM;
	else {
		poll_wait(file, &p_lnp_uart->read_wait, wait);
		if (p_lnp_uart->read_cnt > 0)
			mask |= POLLIN | POLLRDNORM;
		if (tty->packet && tty->link->ctrl_status)
			mask |= POLLPRI | POLLIN | POLLRDNORM;
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
			mask |= POLLHUP;
		if (tty_hung_up_p(file))
			mask |= POLLHUP;
		if (tty->ops->write && !tty_is_writelocked(tty)
				&& tty_chars_in_buffer(tty) < 256
				&& tty_write_room(tty) > 0)
			mask |= POLLOUT | POLLWRNORM;
	}
	pr_info("<- %s mask : %d\n", __func__ , mask);
	return mask;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void lbf_ldisc_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	unsigned int cflag;
	cflag = tty->termios.c_cflag;
	pr_info("-> %s\n", __func__);

	if (cflag & CRTSCTS)
		pr_info(" - RTS/CTS is enabled\n");
	else
		pr_info(" - RTS/CTS is disabled\n");
}

static int __init lnp_uart_init(void)
{
	static struct tty_ldisc_ops lbf_ldisc;
	int err;

	pr_info("-> %s\n", __func__);

	/* Register the tty discipline */

	memset(&lbf_ldisc, 0, sizeof(lbf_ldisc));
	lbf_ldisc.magic = TTY_LDISC_MAGIC;
	lbf_ldisc.name = "n_ld";
	lbf_ldisc.open = lbf_ldisc_open;
	lbf_ldisc.close = lbf_ldisc_close;
	lbf_ldisc.read = lbf_ldisc_read;
	lbf_ldisc.write = lbf_ldisc_write;
	lbf_ldisc.ioctl = lbf_ldisc_ioctl;
	lbf_ldisc.poll = lbf_ldisc_poll;
	lbf_ldisc.receive_buf = lbf_ldisc_receive;
	lbf_ldisc.write_wakeup = lbf_ldisc_wakeup;
	lbf_ldisc.flush_buffer = lbf_ldisc_flush_buffer;
	lbf_ldisc.set_termios = lbf_ldisc_set_termios;

	lbf_ldisc.owner = THIS_MODULE;

	err = tty_register_ldisc(N_INTEL_LDISC, &lbf_ldisc);
	if (err)
		pr_info("registration failed. (%d)", err);
	else
		pr_info("%s: N_INTEL_LDISC registration succeded\n", __func__);

	pr_info("<- %s\n", __func__);
	return err;
}

static void __exit lnp_uart_exit(void)
{
	int err;
	pr_info("-> %s\n", __func__);
	/* Release tty registration of line discipline */
	err = tty_unregister_ldisc(N_INTEL_LDISC);
	if (err)
		pr_info("Can't unregister N_INTEL_LDISC line discipline (%d)",
			 err);
		pr_info("<- %s\n", __func__);
}

module_init(lnp_uart_init);
module_exit(lnp_uart_exit);
MODULE_LICENSE("GPL");
