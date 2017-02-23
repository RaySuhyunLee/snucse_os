/* ASIX AX88796C SPI Fast Ethernet Linux driver */

/* Copyright (c) 2010 ASIX Electronics Corporation
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

/* INCLUDE FILE DECLARATIONS */
/* #define AX_DEBUG */

#ifdef CONFIG_USE_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/clk.h>
#include "ax88796c_main.h"
#include "ax88796c_ioctl.h"

/* NAMING CONSTANT DECLARATIONS */

/* GLOBAL VARIABLES DECLARATIONS */

/* LOCAL VARIABLES DECLARATIONS */

static int comp;
static int ps_level = AX_PS_D0;
static int msg_enable = NETIF_MSG_PROBE |
			NETIF_MSG_LINK |
			/* NETIF_MSG_TIMER | */
			NETIF_MSG_RX_ERR |
			NETIF_MSG_TX_ERR |
			/* NETIF_MSG_TX_QUEUED | */
			/* NETIF_MSG_INTR | */
			/* NETIF_MSG_TX_DONE | */
			/* NETIF_MSG_RX_STATUS | */
			/* NETIF_MSG_PKTDATA | */
			/* NETIF_MSG_HW | */
			NETIF_MSG_WOL;

module_param(comp, int, 0);
MODULE_PARM_DESC(comp, "0=Non-Compression Mode, 1=Compression Mode");

module_param(ps_level, int, 0);
MODULE_PARM_DESC(ps_level,
	"Power Saving Level (0:disable 1:level 1 2:level 2)");

module_param(msg_enable, int, 0);
MODULE_PARM_DESC(msg_enable, "Message level");

static char *macaddr;
module_param(macaddr, charp, 0);
MODULE_PARM_DESC(macaddr, "MAC address");

MODULE_AUTHOR("ASIX");
MODULE_DESCRIPTION("ASIX AX88796C SPI Ethernet driver");
MODULE_LICENSE("GPL");

/* LOCAL SUBPROGRAM DECLARATIONS */
/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_dump_regs
 * Purpose: Dump all MAC registers
 * ----------------------------------------------------------------------------
 */
static void ax88796c_dump_regs(struct ax88796c_device *ax_local)
{
	struct net_device *ndev = ax_local->ndev;
	u8 i, j;

	netdev_info(ndev,
		"       Page0   Page1   Page2   Page3   "
				"Page4   Page5   Page6   Page7\n");
	for (i = 0; i < 0x20; i += 2) {
		netdev_info(ndev, "0x%02x   ", i);
		for (j = 0; j < 8; j++) {
			netdev_info(ndev, "0x%04x  ",
				AX_READ(&ax_local->ax_spi, j * 0x20 + i));
		}
		netdev_info(ndev, "\n");
	}
	netdev_info(ndev, "\n");
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_dump_phy_regs
 * Purpose: Dump PHY register from MR0 to MR5
 * ----------------------------------------------------------------------------
 */
static void ax88796c_dump_phy_regs(struct ax88796c_device *ax_local)
{
	struct net_device *ndev = ax_local->ndev;
	int i;

	netdev_info(ndev, "Dump PHY registers:\n");
	for (i = 0; i < 6; i++) {
		netdev_info(ndev, "  MR%d = 0x%04x\n", i,
			ax88796c_mdio_read(ax_local->ndev,
			ax_local->mii.phy_id, i));
	}
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_watchdog
 * Purpose: Check media link status
 * ----------------------------------------------------------------------------
 */
static void ax88796c_watchdog(struct ax88796c_device *ax_local)
{
	struct net_device *ndev = ax_local->ndev;
	u16 phy_status;
	unsigned long time_to_chk = AX88796C_WATCHDOG_PERIOD;

	if (ax88796c_check_power(ax_local)) {
		mod_timer(&ax_local->watchdog, jiffies + time_to_chk);
		return;
	}

	ax88796c_set_power_saving(ax_local, AX_PS_D0);

	phy_status = AX_READ(&ax_local->ax_spi, P0_PSCR);
	if (phy_status & PSCR_PHYLINK) {

		ax_local->w_state = ax_nop;
		time_to_chk = 0;

	} else if (!(phy_status & PSCR_PHYCOFF)) {
		/* The ethernet cable has been plugged */
		if (ax_local->w_state == chk_cable) {
			if (netif_msg_timer(ax_local))
				netdev_info(ndev, "Cable connected\n");

			ax_local->w_state = chk_link;
			ax_local->w_ticks = 0;
		} else {
			if (netif_msg_timer(ax_local))
				netdev_info(ndev, "Check media status\n");

			if (++ax_local->w_ticks == AX88796C_WATCHDOG_RESTART) {
				if (netif_msg_timer(ax_local))
					netdev_info(ndev, "Restart autoneg\n");
				ax88796c_mdio_write(ndev,
					ax_local->mii.phy_id, MII_BMCR,
					(BMCR_SPEED100 | BMCR_ANENABLE |
					BMCR_ANRESTART));

				if (netif_msg_hw(ax_local))
					ax88796c_dump_phy_regs(ax_local);
				ax_local->w_ticks = 0;
			}
		}
	} else {
		if (netif_msg_timer(ax_local))
			netdev_info(ndev, "Check cable status\n");

		ax_local->w_state = chk_cable;
	}

	ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	if (time_to_chk)
		mod_timer(&ax_local->watchdog, jiffies + time_to_chk);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_watchdog
 * Purpose: Check media link status
 * ----------------------------------------------------------------------------
 */
static void ax88796c_watchdog_timer(unsigned long arg)
{
	struct net_device *ndev = (struct net_device *)(arg);
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);

	set_bit(EVENT_WATCHDOG, &ax_local->flags);
	queue_work(ax_local->ax_work_queue, &ax_local->ax_work);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_reset
 * Purpose: Reset the whol chip
 * ----------------------------------------------------------------------------
 */
static int ax88796c_reset(struct ax88796c_device *ax_local)
{
	unsigned long start;
	u16 temp;

	AX_WRITE(&ax_local->ax_spi, PSR_RESET, P0_PSR);
	AX_WRITE(&ax_local->ax_spi, PSR_RESET_CLR, P0_PSR);

	start = jiffies;
	while (!(AX_READ(&ax_local->ax_spi, P0_PSR) & PSR_DEV_READY)) {
		if (time_after(jiffies, start + (2 * HZ / 100))) {
			netdev_err(ax_local->ndev,
				"timeout waiting for reset completion\n");
			return -1;
		}
	}

	temp = AX_READ(&ax_local->ax_spi, P4_SPICR);
	if (ax_local->capabilities & AX_CAP_COMP) {
		AX_WRITE(&ax_local->ax_spi,
			(temp | SPICR_RCEN | SPICR_QCEN), P4_SPICR);
		ax_local->ax_spi.comp = 1;
	} else {
		AX_WRITE(&ax_local->ax_spi,
			(temp & ~(SPICR_RCEN | SPICR_QCEN)), P4_SPICR);
		ax_local->ax_spi.comp = 0;
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_reload_eeprom
 * Purpose: Load eeprom data from eeprom
 * ----------------------------------------------------------------------------
 */
static int ax88796c_reload_eeprom(struct ax88796c_device *ax_local)
{
	unsigned long start;

	AX_WRITE(&ax_local->ax_spi, EECR_RELOAD, P3_EECR);

	start = jiffies;
	while (!(AX_READ(&ax_local->ax_spi, P0_PSR) & PSR_DEV_READY)) {
		if (time_after(jiffies, start + (2 * HZ / 100))) {
			netdev_err(ax_local->ndev,
				"timeout waiting for reload eeprom\n");
			return -1;
		}
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_set_multicast
 * Purpose: Set receiving mode and multicast filter
 * ----------------------------------------------------------------------------
 */
static void ax88796c_set_hw_multicast(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	u16 rx_ctl = RXCR_AB;
	int mc_count = netdev_mc_count(ndev);

	memset(ax_local->multi_filter, 0, AX_MCAST_FILTER_SIZE);

	if (ndev->flags & IFF_PROMISC) {
		rx_ctl |= RXCR_PRO;

	} else if (ndev->flags & IFF_ALLMULTI
		   || mc_count > AX_MAX_MCAST) {
		rx_ctl |= RXCR_AMALL;

	} else if (mc_count == 0) {
		/* just broadcast and directed */
	} else {
		u32 crc_bits;
		int i;
		struct netdev_hw_addr *ha;
		netdev_for_each_mc_addr(ha, ndev) {
			crc_bits = ether_crc(ETH_ALEN, ha->addr);
			ax_local->multi_filter[crc_bits >> 29] |=
						(1 << ((crc_bits >> 26) & 7));
		}

		for (i = 0; i < 4; i++) {
			AX_WRITE(&ax_local->ax_spi,
				  ((ax_local->multi_filter[i*2+1] << 8) |
				  ax_local->multi_filter[i*2]), P3_MFAR(i));

		}
	}

	AX_WRITE(&ax_local->ax_spi, rx_ctl, P2_RXCR);

}

#if 0
/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_set_multicast
 * Purpose: Set receiving mode and multicast filter
 * ----------------------------------------------------------------------------
 */
static void ax88796c_set_multicast(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);

	set_bit(EVENT_SET_MULTI, &ax_local->flags);
	queue_work(ax_local->ax_work_queue, &ax_local->ax_work);
}
#endif

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_set_mac_addr
 * Purpose: Set up AX88796C MAC address
 * ----------------------------------------------------------------------------
 */
static void ax88796c_set_mac_addr(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);

	AX_WRITE(&ax_local->ax_spi, ((u16)(ndev->dev_addr[4] << 8) |
			(u16)ndev->dev_addr[5]), P3_MACASR0);
	AX_WRITE(&ax_local->ax_spi, ((u16)(ndev->dev_addr[2] << 8) |
			(u16)ndev->dev_addr[3]), P3_MACASR1);
	AX_WRITE(&ax_local->ax_spi, ((u16)(ndev->dev_addr[0] << 8) |
			(u16)ndev->dev_addr[1]), P3_MACASR2);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_set_mac_address
 * Purpose: Reset the whol chip
 * ----------------------------------------------------------------------------
 */
static int ax88796c_set_mac_address(struct net_device *ndev, void *p)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);

	down(&ax_local->spi_lock);

	ax88796c_set_mac_addr(ndev);

	up(&ax_local->spi_lock);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_load_mac_addr
 * Purpose: Read MAC address from AX88796C
 * ----------------------------------------------------------------------------
 */
static int ax88796c_load_mac_addr(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	u16 temp;

	/* Read the MAC address from AX88796C */
	temp = AX_READ(&ax_local->ax_spi, P3_MACASR0);
	ndev->dev_addr[5] = (u8)temp;
	ndev->dev_addr[4] = (u8)(temp >> 8);

	temp = AX_READ(&ax_local->ax_spi, P3_MACASR1);
	ndev->dev_addr[3] = (u8)temp;
	ndev->dev_addr[2] = (u8)(temp >> 8);

	temp = AX_READ(&ax_local->ax_spi, P3_MACASR2);
	ndev->dev_addr[1] = (u8)temp;
	ndev->dev_addr[0] = (u8)(temp >> 8);

	/* Supported for no EEPROM */
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		if (macaddr && mac_pton(macaddr, ndev->dev_addr))
			return 0;

		if (netif_msg_probe(ax_local))
			netdev_info(ndev, "Use random MAC address\n");

		random_ether_addr(ndev->dev_addr);
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_proc_tx_hdr
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void ax88796c_proc_tx_hdr(struct tx_pkt_info *info, u8 ip_summed)
{
	u16 pkt_len_bar = (~info->pkt_len & TX_HDR_SOP_PKTLENBAR);

	/* Prepare SOP header */
	info->sop.flags_len = info->pkt_len |
			(ip_summed == CHECKSUM_NONE ? TX_HDR_SOP_DICF : 0);

	info->sop.seq_lenbar = ((info->seq_num << 11) & TX_HDR_SOP_SEQNUM)
				| pkt_len_bar;
	cpu_to_be16s(&info->sop.flags_len);
	cpu_to_be16s(&info->sop.seq_lenbar);

	/* Prepare Segment header */
	info->seg.flags_seqnum_seglen = TX_HDR_SEG_FS | TX_HDR_SEG_LS
						| info->pkt_len;

	info->seg.eo_so_seglenbar = pkt_len_bar;

	cpu_to_be16s(&info->seg.flags_seqnum_seglen);
	cpu_to_be16s(&info->seg.eo_so_seglenbar);

	/* Prepare EOP header */
	info->eop.seq_len = ((info->seq_num << 11) &
			     TX_HDR_EOP_SEQNUM) | info->pkt_len;
	info->eop.seqbar_lenbar = ((~info->seq_num << 11) &
				   TX_HDR_EOP_SEQNUMBAR) | pkt_len_bar;

	cpu_to_be16s(&info->eop.seq_len);
	cpu_to_be16s(&info->eop.seqbar_lenbar);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_check_free_pages
 * Purpose: Check free pages of TX buffer
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_check_free_pages(struct ax88796c_device *ax_local, u8 need_pages)
{
	u8 free_pages;
	u16 tmp;

	free_pages = AX_READ(&ax_local->ax_spi, P0_TFBFCR) & TX_FREEBUF_MASK;
	if (free_pages < need_pages) {
		/* schedule free page interrupt */
		tmp = AX_READ(&ax_local->ax_spi, P0_TFBFCR)
				& TFBFCR_SCHE_FREE_PAGE;
		AX_WRITE(&ax_local->ax_spi, tmp | TFBFCR_TX_PAGE_SET |
				TFBFCR_SET_FREE_PAGE(need_pages),
				P0_TFBFCR);
		return -ENOMEM;
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_tx_fixup
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static struct sk_buff *
ax88796c_tx_fixup(struct net_device *ndev, struct sk_buff_head *q)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	struct sk_buff *skb, *tx_skb;
	struct tx_pkt_info *info;
	struct skb_data *entry;
	int headroom;
	int tailroom;
	u8 need_pages;
	u16 tol_len, pkt_len;
	u8 padlen, seq_num;
	u8 spi_len = ax_local->ax_spi.comp ? 1 : 4;

	if (skb_queue_empty(q))
		return NULL;

	skb = skb_peek(q);
	pkt_len = skb->len;
	need_pages = (pkt_len + TX_OVERHEAD + 127) >> 7;
	if (ax88796c_check_free_pages(ax_local, need_pages) != 0)
		return NULL;

	headroom = skb_headroom(skb);
	tailroom = skb_tailroom(skb);
	padlen = ((pkt_len + 3) & 0x7FC) - pkt_len;
	tol_len = ((pkt_len + 3) & 0x7FC) +
			TX_OVERHEAD + TX_EOP_SIZE + spi_len;
	seq_num = ++ax_local->seq_num & 0x1F;

	info = (struct tx_pkt_info *) skb->cb;
	info->pkt_len = pkt_len;

	if ((!skb_cloned(skb)) &&
	    (headroom >= (TX_OVERHEAD + spi_len)) &&
	    (tailroom >= (padlen + TX_EOP_SIZE))) {

		info->seq_num = seq_num;
		ax88796c_proc_tx_hdr(info, skb->ip_summed);

		/* SOP and SEG header */
		memcpy(skb_push(skb, TX_OVERHEAD), &info->sop, TX_OVERHEAD);

		/* Write SPI TXQ header */
		memcpy(skb_push(skb, spi_len), tx_cmd_buf, spi_len);

		/* Make 32-bit aligment */
		skb_put(skb, padlen);

		/* EOP header */
		memcpy(skb_put(skb, TX_EOP_SIZE), &info->eop, TX_EOP_SIZE);

		tx_skb = skb;
		skb_unlink(skb, q);

	} else {

		tx_skb = alloc_skb(tol_len, GFP_KERNEL);
		if (!tx_skb)
			return NULL;

		/* Write SPI TXQ header */
		memcpy(skb_put(tx_skb, spi_len), tx_cmd_buf, spi_len);

		info->seq_num = seq_num;
		ax88796c_proc_tx_hdr(info, skb->ip_summed);

		/* SOP and SEG header */
		memcpy(skb_put(tx_skb, TX_OVERHEAD),
				&info->sop, TX_OVERHEAD);

		/* Packet */
		memcpy(skb_put(tx_skb, ((pkt_len + 3) & 0xFFFC)),
				skb->data, pkt_len);

		/* EOP header */
		memcpy(skb_put(tx_skb, TX_EOP_SIZE),
				&info->eop, TX_EOP_SIZE);

		skb_unlink(skb, q);
		dev_kfree_skb(skb);
	}

	entry = (struct skb_data *) tx_skb->cb;
	memset(entry, 0, sizeof(*entry));
	entry->len = pkt_len;

	if (netif_msg_pktdata(ax_local)) {
		int loop;
		netdev_info(ndev, "TX packet len %d, total len %d, seq %d\n",
				pkt_len, tx_skb->len, seq_num);

		netdev_info(ndev, "  Dump SPI Header:\n    ");
		for (loop = 0; loop < 4; loop++)
			netdev_info(ndev, "%02x ", *(tx_skb->data + loop));

		netdev_info(ndev, "\n");

		netdev_info(ndev, "  Dump TX SOP:\n    ");
		for (loop = 0; loop < TX_OVERHEAD; loop++)
			netdev_info(ndev, "%02x ", *(tx_skb->data + 4 + loop));

		netdev_info(ndev, "\n");

		netdev_info(ndev, "  Dump TX packet:");
		for (loop = TX_OVERHEAD + 4;
		     loop < (tx_skb->len - TX_EOP_SIZE); loop++) {
			if (((loop + 8) % 16) == 0)
				netdev_info(ndev, "\n    ");
			netdev_info(ndev, "%02x ", *(tx_skb->data + loop));
		}
		netdev_info(ndev, "\n");

		netdev_info(ndev, "  Dump TX EOP:\n    %02x %02x %02x %02x\n",
			*(tx_skb->data + tx_skb->len - 4),
			*(tx_skb->data + tx_skb->len - 3),
			*(tx_skb->data + tx_skb->len - 2),
			*(tx_skb->data + tx_skb->len - 1));
	}

	return tx_skb;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_xmit
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_hard_xmit(struct ax88796c_device *ax_local)
{
	struct sk_buff *tx_skb;
	struct skb_data *entry;

	tx_skb = ax88796c_tx_fixup(ax_local->ndev, &ax_local->tx_wait_q);

	if (!tx_skb)
		return 0;

	entry = (struct skb_data *)tx_skb->cb;

	AX_WRITE(&ax_local->ax_spi,
			(TSNR_TXB_START | TSNR_PKT_CNT(1)), P0_TSNR);

	axspi_write_txq(&ax_local->ax_spi, tx_skb->data, tx_skb->len);

	if (((AX_READ(&ax_local->ax_spi, P0_TSNR) & TXNR_TXB_IDLE) == 0) ||
	    ((ISR_TXERR & AX_READ(&ax_local->ax_spi, P0_ISR)) != 0)) {

		/* Ack tx error int */
		AX_WRITE(&ax_local->ax_spi, ISR_TXERR, P0_ISR);

		ax_local->stats.tx_dropped++;

		if (netif_msg_tx_err(ax_local))
			netdev_err(ax_local->ndev,
				"TX FIFO error, re-initialize the TX bridge\n");

		/* Reinitial tx bridge */
		AX_WRITE(&ax_local->ax_spi, TXNR_TXB_REINIT |
			AX_READ(&ax_local->ax_spi, P0_TSNR), P0_TSNR);
		ax_local->seq_num = 0;
	} else {
		ax_local->stats.tx_packets++;
		ax_local->stats.tx_bytes += entry->len;
	}

	entry->state = tx_done;
	dev_kfree_skb(tx_skb);

	return 1;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_start_xmit
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);

	skb_queue_tail(&ax_local->tx_wait_q, skb);
	if (skb_queue_len(&ax_local->tx_wait_q) > TX_QUEUE_HIGH_WATER) {
		if (netif_msg_tx_queued(ax_local))
			netdev_err(ndev, "Too much TX packets in queue %d\n",
					skb_queue_len(&ax_local->tx_wait_q));

		netif_stop_queue(ndev);
	}

	set_bit(EVENT_TX, &ax_local->flags);
	queue_work(ax_local->ax_work_queue, &ax_local->ax_work);

	return NETDEV_TX_OK;

}


/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_skb_return
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static inline void
ax88796c_skb_return(struct ax88796c_device *ax_local, struct sk_buff *skb,
			struct rx_header *rxhdr)
{
	struct net_device *ndev = ax_local->ndev;
	int status;

	do {
		if (!(ax_local->checksum & AX_RX_CHECKSUM))
			break;

		/* checksum error bit is set */
		if ((rxhdr->flags & RX_HDR3_L3_ERR) ||
		    (rxhdr->flags & RX_HDR3_L4_ERR))
			break;

		if ((rxhdr->flags & RX_HDR3_L4_TYPE_TCP) ||
		    (rxhdr->flags & RX_HDR3_L4_TYPE_UDP)) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		}
	} while (0);

	ax_local->stats.rx_packets++;
	ax_local->stats.rx_bytes += skb->len;
	skb->dev = ndev;

	skb->truesize = skb->len + sizeof(struct sk_buff);
	skb->protocol = eth_type_trans(skb, ax_local->ndev);

	if (netif_msg_rx_status(ax_local))
		netdev_info(ndev, "< rx, len %zu, type 0x%x\n",
			skb->len + sizeof(struct ethhdr), skb->protocol);

	status = netif_rx(skb);
	if (status != NET_RX_SUCCESS && netif_msg_rx_err(ax_local))
		netdev_info(ndev, "netif_rx status %d\n", status);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_rx_fixup
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void
ax88796c_rx_fixup(struct ax88796c_device *ax_local, struct sk_buff *rx_skb)
{
	struct rx_header *rxhdr = (struct rx_header *) rx_skb->data;
	struct net_device *ndev = ax_local->ndev;
	u16 len;

	be16_to_cpus(&rxhdr->flags_len);
	be16_to_cpus(&rxhdr->seq_lenbar);
	be16_to_cpus(&rxhdr->flags);

	if ((((short)rxhdr->flags_len) & RX_HDR1_PKT_LEN) !=
			 (~((short)rxhdr->seq_lenbar) & 0x7FF)) {
		if (netif_msg_rx_err(ax_local)) {
			int i;
			netdev_err(ndev, "Header error\n");
			netdev_err(ndev, "Dump received frame\n");
			for (i = 0; i < rx_skb->len; i++) {
				netdev_err(ndev, "%02x ",
						*(rx_skb->data + i));
				if (((i + 1) % 16) == 0)
					netdev_err(ndev, "\n");
			}
			netdev_err(ndev, "\n");
		}
		ax_local->stats.rx_frame_errors++;
		kfree_skb(rx_skb);
		return;
	}

	if ((rxhdr->flags_len & RX_HDR1_MII_ERR) ||
			(rxhdr->flags_len & RX_HDR1_CRC_ERR)) {
		if (netif_msg_rx_err(ax_local))
			netdev_err(ndev, "CRC or MII error\n");

		ax_local->stats.rx_crc_errors++;
		kfree_skb(rx_skb);
		return;
	}

	len = rxhdr->flags_len & RX_HDR1_PKT_LEN;
	if (netif_msg_pktdata(ax_local)) {
		int loop;
		netdev_info(ndev, "RX data, total len %d, packet len %d\n",
				rx_skb->len, len);

		netdev_info(ndev, "  Dump RX packet header:\n    ");
		for (loop = 0; loop < sizeof(*rxhdr); loop++)
			netdev_info(ndev, "%02x ", *(rx_skb->data + loop));

		netdev_info(ndev, "\n  Dump RX packet:");
		for (loop = 0; loop < len; loop++) {
			if ((loop % 16) == 0)
				netdev_info(ndev, "\n    ");
			netdev_info(ndev, "%02x ",
				*(rx_skb->data + loop + sizeof(*rxhdr)));
		}
		netdev_info(ndev, "\n");
	}

	skb_pull(rx_skb, sizeof(*rxhdr));
	__pskb_trim(rx_skb, len);

	return ax88796c_skb_return(ax_local, rx_skb, rxhdr);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_receive
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_receive(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	struct sk_buff *skb;
	struct skb_data *entry;
	u16 w_count, pkt_len;
	u8 pkt_cnt;

	/* check rx packet and total word count */
	AX_WRITE(&ax_local->ax_spi, AX_READ(&ax_local->ax_spi, P0_RTWCR)
		  | RTWCR_RX_LATCH, P0_RTWCR);

	pkt_cnt = AX_READ(&ax_local->ax_spi, P0_RXBCR2) & RXBCR2_PKT_MASK;
	if (!pkt_cnt)
		return 0;

	pkt_len = AX_READ(&ax_local->ax_spi, P0_RCPHR) & 0x7FF;

	w_count = ((pkt_len + 6 + 3) & 0xFFFC) >> 1;

	skb = alloc_skb((w_count * 2), GFP_KERNEL);
	if (!skb) {
		if (netif_msg_rx_err(ax_local))
			netdev_err(ndev,
				"Couldn't allocate a sk_buff of size %d\n",
				w_count * 2);

		AX_WRITE(&ax_local->ax_spi, RXBCR1_RXB_DISCARD, P0_RXBCR1);
		return 0;
	}
	entry = (struct skb_data *) skb->cb;

	AX_WRITE(&ax_local->ax_spi, RXBCR1_RXB_START | w_count, P0_RXBCR1);

	axspi_read_rxq(&ax_local->ax_spi,
			skb_put(skb, w_count * 2), skb->len);

	/* Check if rx bridge is idle */
	if ((AX_READ(&ax_local->ax_spi, P0_RXBCR2) & RXBCR2_RXB_IDLE) == 0) {

		if (netif_msg_rx_err(ax_local))
			netdev_err(ndev, "Rx Bridge is not idle\n");
		AX_WRITE(&ax_local->ax_spi, RXBCR2_RXB_REINIT, P0_RXBCR2);

		entry->state = rx_err;

	} else {

		entry->state = rx_done;
	}

	AX_WRITE(&ax_local->ax_spi, ISR_RXPKT, P0_ISR);

	ax88796c_rx_fixup(ax_local, skb);

	return 1;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_check_media
 * Purpose: Process media link status
 * ----------------------------------------------------------------------------
 */
static void ax88796c_check_media(struct ax88796c_device *ax_local)
{
	struct net_device *ndev = ax_local->ndev;
	u16 bmsr, bmcr;

	if (netif_msg_hw(ax_local))
		ax88796c_dump_phy_regs(ax_local);

	bmsr = ax88796c_mdio_read(ndev,
			ax_local->mii.phy_id, MII_BMSR);

	if (!(bmsr & BMSR_LSTATUS) && netif_carrier_ok(ndev)) {

		netif_carrier_off(ndev);
		if (netif_msg_link(ax_local))
			netdev_info(ndev, "link down\n");

		ax_local->w_state = chk_cable;
		mod_timer(&ax_local->watchdog,
				jiffies + AX88796C_WATCHDOG_PERIOD);

	} else if ((bmsr & BMSR_LSTATUS) &&
		  !netif_carrier_ok(ndev)) {
		bmcr = ax88796c_mdio_read(ndev,
				ax_local->mii.phy_id, MII_BMCR);
		if (netif_msg_link(ax_local))
			netdev_info(ndev, "link up, %sMbps, %s-duplex\n",
				(bmcr & BMCR_SPEED100) ? "100" : "10",
				(bmcr & BMCR_FULLDPLX) ? "full" : "half");

		netif_carrier_on(ndev);
	}

	return;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_process_isr
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_process_isr(struct ax88796c_device *ax_local)
{
	u16 isr;
	u8 done = 0;
	struct net_device *ndev = ax_local->ndev;

	isr = AX_READ(&ax_local->ax_spi, P0_ISR);
	AX_WRITE(&ax_local->ax_spi, isr, P0_ISR);

	if (netif_msg_intr(ax_local))
		netdev_info(ndev, "  ISR 0x%04x\n", isr);

	if (isr & ISR_TXERR) {
		if (netif_msg_intr(ax_local))
			netdev_info(ndev, "  TXERR interrupt\n");
		AX_WRITE(&ax_local->ax_spi, TXNR_TXB_REINIT, P0_TSNR);
		ax_local->seq_num = 0x1f;
	}

	if (isr & ISR_TXPAGES) {

		if (netif_msg_intr(ax_local))
			netdev_info(ndev, "  TXPAGES interrupt\n");

		set_bit(EVENT_TX, &ax_local->flags);
	}

	if (isr & ISR_LINK) {

		if (netif_msg_intr(ax_local))
			netdev_info(ndev, "  Link change interrupt\n");

		ax88796c_check_media(ax_local);
	}

	if (isr & ISR_RXPKT) {

		if (netif_msg_intr(ax_local))
			netdev_info(ndev, "  RX interrupt\n");

		done = ax88796c_receive(ax_local->ndev);
	}

	return done;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_interrupt
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static irqreturn_t ax88796c_interrupt(int irq, void *dev_instance)
{
	struct net_device *ndev = dev_instance;
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);

	if (ndev == NULL) {
		pr_err("irq %d for unknown device.\n", irq);
		return IRQ_RETVAL(0);
	}

	disable_irq_nosync(irq);

	if (netif_msg_intr(ax_local))
		netdev_info(ndev, "Interrupt occurred\n");

	set_bit(EVENT_INTR, &ax_local->flags);
	queue_work(ax_local->ax_work_queue, &ax_local->ax_work);

	return IRQ_HANDLED;
}


/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_work
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void ax88796c_work(struct work_struct *work)
{
	struct ax88796c_device *ax_local =
			container_of(work, struct ax88796c_device, ax_work);
	u8 power = 0;

	down(&ax_local->spi_lock);

	if (test_bit(EVENT_WATCHDOG, &ax_local->flags)) {

		ax88796c_watchdog(ax_local);

		clear_bit(EVENT_WATCHDOG, &ax_local->flags);
	}

	if (test_bit(EVENT_SET_MULTI, &ax_local->flags)) {

		power = ax88796c_check_power_and_wake(ax_local);

		ax88796c_set_hw_multicast(ax_local->ndev);
		clear_bit(EVENT_SET_MULTI, &ax_local->flags);
	}

	if (test_bit(EVENT_INTR, &ax_local->flags)) {

		power = ax88796c_check_power_and_wake(ax_local);

		AX_WRITE(&ax_local->ax_spi, IMR_MASKALL, P0_IMR);

		while (1) {
			if (!ax88796c_process_isr(ax_local))
				break;
		}

		clear_bit(EVENT_INTR, &ax_local->flags);

		AX_WRITE(&ax_local->ax_spi, IMR_DEFAULT, P0_IMR);

		enable_irq(ax_local->ndev->irq);
	}

	if (test_bit(EVENT_TX, &ax_local->flags)) {

		power = ax88796c_check_power_and_wake(ax_local);

		while (skb_queue_len(&ax_local->tx_wait_q)) {
			if (!ax88796c_hard_xmit(ax_local))
				break;
		}

		clear_bit(EVENT_TX, &ax_local->flags);

		if (netif_queue_stopped(ax_local->ndev) &&
		    (skb_queue_len(&ax_local->tx_wait_q) < TX_QUEUE_LOW_WATER))
			netif_wake_queue(ax_local->ndev);
	}

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	up(&ax_local->spi_lock);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_get_stats
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static struct net_device_stats *ax88796c_get_stats(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	return &ax_local->stats;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_phy_init
 * Purpose:
 * ----------------------------------------------------------------------------
 */
void ax88796c_phy_init(struct ax88796c_device *ax_local)
{
	u16 advertise = ADVERTISE_ALL | ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP;

	/* Setup LED mode */
	AX_WRITE(&ax_local->ax_spi,
		  (LCR_LED0_EN | LCR_LED0_DUPLEX | LCR_LED1_EN |
		   LCR_LED1_100MODE), P2_LCR0);
	AX_WRITE(&ax_local->ax_spi,
		  (AX_READ(&ax_local->ax_spi, P2_LCR1) & LCR_LED2_MASK) |
		   LCR_LED2_EN | LCR_LED2_LINK, P2_LCR1);

	/* Enable PHY auto-polling */
	AX_WRITE(&ax_local->ax_spi,
		  POOLCR_PHYID(ax_local->mii.phy_id) | POOLCR_POLL_EN |
		  POOLCR_POLL_FLOWCTRL | POOLCR_POLL_BMCR, P2_POOLCR);

	ax88796c_mdio_write(ax_local->ndev,
			ax_local->mii.phy_id, MII_ADVERTISE, advertise);

	ax88796c_mdio_write(ax_local->ndev, ax_local->mii.phy_id, MII_BMCR,
			BMCR_SPEED100 | BMCR_ANENABLE | BMCR_ANRESTART);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_open
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_open(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	int ret;
	u8 power;
	unsigned long irq_flag = IRQF_SHARED;

	netif_carrier_off(ax_local->ndev);

	down(&ax_local->spi_lock);

	power = ax88796c_check_power_and_wake(ax_local);

	ax88796c_reset(ax_local);

	ret = request_irq(ndev->irq, ax88796c_interrupt,
			irq_flag, ndev->name, ndev);
	if (ret) {
		netdev_err(ndev, "unable to get IRQ %d (errno=%d).\n",
				ndev->irq, ret);
		return -ENXIO;
	}

	ax_local->seq_num = 0x1f;

	ax88796c_set_mac_addr(ndev);
	ax88796c_set_csums(ax_local);

	/* Disable stuffing packet */
	AX_WRITE(&ax_local->ax_spi,
		  AX_READ(&ax_local->ax_spi, P1_RXBSPCR)
		  & ~RXBSPCR_STUF_ENABLE, P1_RXBSPCR);

	/* Enable RX packet process */
	AX_WRITE(&ax_local->ax_spi, RPPER_RXEN, P1_RPPER);

	AX_WRITE(&ax_local->ax_spi, AX_READ(&ax_local->ax_spi, P0_FER)
		  | FER_RXEN | FER_TXEN | FER_BSWAP | FER_IRQ_PULL, P0_FER);

	ax88796c_phy_init(ax_local);

	netif_start_queue(ndev);

	AX_WRITE(&ax_local->ax_spi, IMR_DEFAULT, P0_IMR);

	if (netif_msg_hw(ax_local)) {
		netdev_info(ndev,
			"Dump all MAC registers after initialization:\n");
		ax88796c_dump_regs(ax_local);
		ax88796c_dump_phy_regs(ax_local);
	}

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	spi_message_init(&ax_local->ax_spi.rx_msg);

	up(&ax_local->spi_lock);

	init_timer(&ax_local->watchdog);
	ax_local->watchdog.function = &ax88796c_watchdog_timer;
	ax_local->watchdog.expires = jiffies + AX88796C_WATCHDOG_PERIOD;
	ax_local->watchdog.data = (unsigned long) ndev;
	ax_local->w_state = chk_cable;
	ax_local->w_ticks = 0;

	add_timer(&ax_local->watchdog);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_free_skb_queue
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void ax88796c_free_skb_queue(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	while (q->qlen) {
		skb = skb_dequeue(q);
		kfree_skb(skb);
	}
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_close
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_close(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	u8 power;

	netif_stop_queue(ndev);

	del_timer_sync(&ax_local->watchdog);

	free_irq(ndev->irq, ndev);

	down(&ax_local->spi_lock);

	power = ax88796c_check_power_and_wake(ax_local);

	AX_WRITE(&ax_local->ax_spi, IMR_MASKALL, P0_IMR);
	ax88796c_free_skb_queue(&ax_local->tx_wait_q);

	ax88796c_reset(ax_local);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	up(&ax_local->spi_lock);

	return 0;
}

static const struct net_device_ops ax88796c_netdev_ops = {
	.ndo_open		= ax88796c_open,
	.ndo_stop		= ax88796c_close,
	.ndo_start_xmit		= ax88796c_start_xmit,
	.ndo_get_stats		= ax88796c_get_stats,
	/* .ndo_set_multicast_list = ax88796c_set_multicast, */
	.ndo_do_ioctl		= ax88796c_ioctl,
	.ndo_set_mac_address	= ax88796c_set_mac_address,
};


static int ax88796c_gpio_reset(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int reset_gpio;

	/* reset info */
	reset_gpio = of_get_named_gpio(np, "ax88796c,reset", 0);

	/* set reset */
	gpio_request(reset_gpio, "ax88796c_reset");
	gpio_direction_output(reset_gpio, 0);
	msleep(100);
	gpio_direction_output(reset_gpio, 1);
	gpio_free(reset_gpio);
	msleep(10);

	return 0;
}

static int set_asix_sclk(struct spi_device *spi, unsigned long rate)
{
	struct device *dev = &spi->dev;
	struct clk *fout_mpll;
	struct clk *dout_mpll_pre;
	struct clk *mout_spi0;
	struct clk *sclk_spi0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	fout_mpll = clk_get(NULL, "fout_mpll");
	if (IS_ERR(fout_mpll)) {
		pr_err("%s: failed to get fout_mpll\n", __func__);
		return PTR_ERR(fout_mpll);
	}

	dout_mpll_pre = clk_get(dev, "dout_mpll_pre");
	if (IS_ERR(dout_mpll_pre)) {
		pr_err("%s: failed to get dout_mpll_pre\n", __func__);
		ret = -EINVAL;
		goto out1;
	}
	clk_set_parent(dout_mpll_pre, fout_mpll);

	mout_spi0 = clk_get(dev, "mout_spi0");
	if (IS_ERR(mout_spi0)) {
		pr_err("%s: failed to get mout_spi0\n", __func__);
		ret = -EINVAL;
		goto out2;
	}
	clk_set_parent(mout_spi0, dout_mpll_pre);

	sclk_spi0 = clk_get(dev, "sclk_spi0");
	if (IS_ERR(sclk_spi0)) {
		pr_err("%s: failed to get sclk_spi0\n", __func__);
		ret = -EINVAL;
		goto out3;
	}
	clk_set_parent(sclk_spi0, mout_spi0);

	ax_dprintk("[Before] sclk_spi rate ="
		" fout_mpll(%ld), dout_mpll_pre(%ld), mout_spi0(%ld), sclk_spi0(%ld)\n",
		clk_get_rate(fout_mpll),
		clk_get_rate(dout_mpll_pre),
		clk_get_rate(mout_spi0),
		clk_get_rate(sclk_spi0));

	clk_set_rate(sclk_spi0, rate);
	ax_dprintk("[After] sclk_spi rate ="
		" fout_mpll(%ld), dout_mpll_pre(%ld), mout_spi0(%ld), sclk_spi0(%ld)\n",
		clk_get_rate(fout_mpll),
		clk_get_rate(dout_mpll_pre),
		clk_get_rate(mout_spi0),
		clk_get_rate(sclk_spi0));

	clk_put(sclk_spi0);
out3:
	clk_put(mout_spi0);
out2:
	clk_put(dout_mpll_pre);
out1:
	clk_put(fout_mpll);

	return ret;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_probe
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_probe(struct spi_device *spi)
{
	struct net_device *ndev;
	struct ax88796c_device *ax_local;
	int ret;
	u16 temp;

	ndev = alloc_etherdev(sizeof(*ax_local));
	if (!ndev) {
		pr_err("AX88796C SPI: Could not allocate ethernet device\n");
		return -ENOMEM;
	}

	ax_local = to_ax88796c_device(ndev);
	memset(ax_local, 0, sizeof(*ax_local));

	dev_set_drvdata(&spi->dev, ax_local);
	ax_local->spi = spi;
	ax_local->ax_spi.spi = spi;

	ndev->irq = spi->irq;

	ax_local->msg_enable =  msg_enable;
	if (ps_level > AX_PS_D2 || ps_level < 0)
		ax_local->ps_level = 0;
	else
		ax_local->ps_level = ps_level;

	ax_local->capabilities |= comp ? AX_CAP_COMP : 0;

	if (netif_msg_probe(ax_local)) {
		netdev_info(ndev, "AX88796C-SPI Configuration:\n");
		netdev_info(ndev, "    Compression : %s\n",
			ax_local->capabilities & AX_CAP_COMP ? "ON" : "OFF");
		netdev_info(ndev, "    Power Saving Level: %d\n",
			ax_local->ps_level);
	}

	ndev->netdev_ops	= &ax88796c_netdev_ops;
	ndev->ethtool_ops	= &ax88796c_ethtool_ops;

	ax_local->ndev = ndev;

	/* Initialize MII structure */
	ax_local->mii.dev = ndev;
	ax_local->mii.mdio_read = ax88796c_mdio_read;
	ax_local->mii.mdio_write = ax88796c_mdio_write;
	ax_local->mii.phy_id_mask = 0x3f;
	ax_local->mii.reg_num_mask = 0x1f;
	ax_local->mii.phy_id = 0x10;

	/* ax88796c gpio reset */
	ax88796c_gpio_reset(&spi->dev);

	/* Reset AX88796C */
	ax88796c_reset(ax_local);

	temp = AX_READ(&ax_local->ax_spi, P0_BOR);
	if (temp == 0x1234) {
		ax_local->plat_endian = PLAT_LITTLE_ENDIAN;
	} else {
		AX_WRITE(&ax_local->ax_spi, 0xFFFF, P0_BOR);
		ax_local->plat_endian = PLAT_BIG_ENDIAN;
	}

	if (netif_msg_hw(ax_local)) {
		netdev_info(ndev,
			"Dump all MAC registers before initialization:\n");
		ax88796c_dump_regs(ax_local);
		ax88796c_dump_phy_regs(ax_local);
	}

	/*Reload EEPROM*/
	ax88796c_reload_eeprom(ax_local);

	ax88796c_load_mac_addr(ndev);

	ret = set_asix_sclk(spi, AX_MAX_CLK);
	if (ret)
		netdev_info(ndev, "Failed to setup spi sclk(%d)\n", ret);

	ax_dprintk("----> %s/%d\n", __func__, __LINE__);
	if (netif_msg_probe(ax_local))
		netdev_info(ndev,
			"addr 0x%lx, irq %d, MAC addr "
			"%02X:%02X:%02X:%02X:%02X:%02X\n",
			ndev->base_addr, ndev->irq,
			ndev->dev_addr[0], ndev->dev_addr[1],
			ndev->dev_addr[2], ndev->dev_addr[3],
			ndev->dev_addr[4], ndev->dev_addr[5]);

	ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	INIT_WORK(&ax_local->ax_work, ax88796c_work);

	ax_local->ax_work_queue =
			create_singlethread_workqueue("ax88796c_work");

	sema_init(&ax_local->spi_lock, 1);

	skb_queue_head_init(&ax_local->tx_wait_q);

	ndev->features |= NETIF_F_HW_CSUM;
	ax_local->checksum = AX_RX_CHECKSUM | AX_TX_CHECKSUM;
	ndev->hard_header_len += (TX_OVERHEAD + 4);

	ret = register_netdev(ndev);
	if (!ret)
		return ret;

	destroy_workqueue(ax_local->ax_work_queue);

	free_netdev(ndev);

	return ret;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_suspend
 * Purpose: Device suspend handling function
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_suspend(struct spi_device *spi, pm_message_t mesg)
{
	struct ax88796c_device *ax_local = dev_get_drvdata(&spi->dev);
	struct net_device *ndev = ax_local->ndev;
	u8 power;

	if (!ndev || !netif_running(ndev))
		return 0;

	netif_device_detach(ndev);

	netif_stop_queue(ndev);

	down(&ax_local->spi_lock);

	power = ax88796c_check_power_and_wake(ax_local);

	AX_WRITE(&ax_local->ax_spi, IMR_MASKALL, P0_IMR);
	ax88796c_free_skb_queue(&ax_local->tx_wait_q);

	if (ax_local->wol) {

		AX_WRITE(&ax_local->ax_spi, 0, P5_WFTR);

		if (ax_local->wol & WFCR_LINKCH) {	/* Link change */

			/* Disable wol power saving in link change mode */
			AX_WRITE(&ax_local->ax_spi,
				  (AX_READ(&ax_local->ax_spi, P0_PSCR)
				  & ~PSCR_WOLPS), P0_PSCR);

			if (netif_msg_wol(ax_local))
				netdev_info(ndev,
					"Enable link change wakeup\n");

			AX_WRITE(&ax_local->ax_spi, WFTR_8192MS, P5_WFTR);
		}
		if (ax_local->wol & WFCR_MAGICP) {	/* Magic packet */
			if (netif_msg_wol(ax_local))
				netdev_info(ndev,
					"Enable magic packet wakeup\n");
		}

		AX_WRITE(&ax_local->ax_spi,
			  ax_local->wol | WFCR_WAKEUP | WFCR_PMEEN, P0_WFCR);
	}

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	up(&ax_local->spi_lock);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_resume
 * Purpose: Device resume handling function
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_resume(struct spi_device *spi)
{
	struct ax88796c_device *ax_local = dev_get_drvdata(&spi->dev);
	struct net_device *ndev = ax_local->ndev;
	u16 pme;

	down(&ax_local->spi_lock);

	/* Wakeup AX88796C first */
	ax88796c_check_power_and_wake(ax_local);
	msleep(200);

	pme = AX_READ(&ax_local->ax_spi, P0_WFCR);
	if (ax_local->wol && ~(pme & WFCR_WAITEVENT)) {

		if (pme & WFCR_LINKCHS) {
			if (netif_msg_wol(ax_local))
				netdev_info(ndev,
					"Wakeuped from link change.\n");
		} else if (pme & WFCR_MAGICPS) {
			if (netif_msg_wol(ax_local))
				netdev_info(ndev,
					"Wakeuped from magic packet.\n");
		}

		AX_WRITE(&ax_local->ax_spi, WFCR_CLRWAKE, P0_WFCR);
	}

	netif_device_attach(ndev);

	/* Initialize all the local variables*/
	ax88796c_reset(ax_local);

	ax_local->seq_num = 0x1f;

	ax88796c_set_mac_addr(ndev);
	ax88796c_set_csums(ax_local);

	/* Disable stuffing packet */
	AX_WRITE(&ax_local->ax_spi,
		  AX_READ(&ax_local->ax_spi, P1_RXBSPCR)
		  & ~RXBSPCR_STUF_ENABLE, P1_RXBSPCR);

	/* Enable RX packet process */
	AX_WRITE(&ax_local->ax_spi, RPPER_RXEN, P1_RPPER);

	AX_WRITE(&ax_local->ax_spi,
		  AX_READ(&ax_local->ax_spi, P0_FER)
		  | FER_RXEN | FER_TXEN | FER_BSWAP, P0_FER);

	ax88796c_phy_init(ax_local);

	AX_WRITE(&ax_local->ax_spi, IMR_DEFAULT, P0_IMR);

	if (netif_msg_hw(ax_local)) {
		netdev_info(ndev,
			"Dump all MAC registers after initialization:\n");
		ax88796c_dump_regs(ax_local);
		ax88796c_dump_phy_regs(ax_local);
	}

	ax88796c_set_power_saving(ax_local, ax_local->ps_level);

	netif_start_queue(ndev);

	up(&ax_local->spi_lock);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_remove
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_remove(struct spi_device *spi)
{
	struct ax88796c_device *ax_local = dev_get_drvdata(&spi->dev);
	struct net_device *ndev = ax_local->ndev;

	destroy_workqueue(ax_local->ax_work_queue);

	unregister_netdev(ndev);

	free_netdev(ndev);

	netdev_info(ndev, "ax88796c-spi: released and freed device\n");

	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id ax88796c_dt_ids[] = {
	{ .compatible = "asix,ax88796c" },
	{},
};
MODULE_DEVICE_TABLE(of, ax88796c_dt_ids);
#endif

static struct spi_driver ax88796c_spi_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = of_match_ptr(ax88796c_dt_ids),
#endif
	},
	.probe = ax88796c_probe,
	.remove = ax88796c_remove,
	.suspend = ax88796c_suspend,
	.resume = ax88796c_resume,
};

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_spi_init
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static __init int ax88796c_spi_init(void)
{
	pr_info("Register AX88796C SPI Ethernet Driver.\n");
	return spi_register_driver(&ax88796c_spi_driver);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_spi_exit
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static __exit void ax88796c_spi_exit(void)
{
	spi_unregister_driver(&ax88796c_spi_driver);
}

module_init(ax88796c_spi_init);
module_exit(ax88796c_spi_exit);

