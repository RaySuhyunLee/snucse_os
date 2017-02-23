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
#include "ax88796c_main.h"
#include "ax88796c_spi.h"
#include "ax88796c_ioctl.h"

/* NAMING CONSTANT DECLARATIONS */

/* GLOBAL VARIABLES DECLARATIONS */

/* LOCAL VARIABLES DECLARATIONS */

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_check_power
 * Purpose: Check AX88796C power saving status
 * ----------------------------------------------------------------------------
 */
u8 ax88796c_check_power(struct ax88796c_device *ax_local)
{
	struct spi_status ax_status;

	/* Check media link status first */
	if (netif_carrier_ok(ax_local->ndev) ||
	    (ax_local->ps_level == AX_PS_D0)  ||
	    (ax_local->ps_level == AX_PS_D1)) {
		return 0;
	}

	AX_READ_STATUS(&ax_local->ax_spi, &ax_status);
	if (!(ax_status.status & AX_STATUS_READY))
		return 1;

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_check_power_and_wake
 * Purpose: Check AX88796C power saving status
 * ----------------------------------------------------------------------------
 */
u8 ax88796c_check_power_and_wake(struct ax88796c_device *ax_local)
{
	struct spi_status ax_status;
	unsigned long start_time;

	/* Check media link status first */
	if (netif_carrier_ok(ax_local->ndev) ||
	    (ax_local->ps_level == AX_PS_D0) ||
	    (ax_local->ps_level == AX_PS_D1)) {
		return 0;
	}

	AX_READ_STATUS(&ax_local->ax_spi, &ax_status);
	if (!(ax_status.status & AX_STATUS_READY)) {

		/* AX88796C in power saving mode */
		AX_WAKEUP(&ax_local->ax_spi);

		/* Check status */
		start_time = jiffies;
		do {
			if (time_after(jiffies, start_time + HZ/2)) {
				netdev_err(ax_local->ndev,
					"timeout waiting for wakeup"
					" from power saving\n");
				break;
			}

			AX_READ_STATUS(&ax_local->ax_spi, &ax_status);

		} while (!(ax_status.status & AX_STATUS_READY));

		ax88796c_set_power_saving(ax_local, AX_PS_D0);

		return 1;
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_set_power_saving
 * Purpose: Set up AX88796C power saving
 * ----------------------------------------------------------------------------
 */
void ax88796c_set_power_saving(struct ax88796c_device *ax_local, u8 ps_level)
{
	u16 pmm;

	if (ps_level == AX_PS_D1)
		pmm = PSCR_PS_D1;
	else if (ps_level == AX_PS_D2)
		pmm = PSCR_PS_D2;
	else
		pmm = PSCR_PS_D0;

	AX_WRITE(&ax_local->ax_spi, (AX_READ(&ax_local->ax_spi, P0_PSCR)
				      & PSCR_PS_MASK) | pmm, P0_PSCR);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_mdio_read
 * Purpose:
 * ----------------------------------------------------------------------------
 */
int ax88796c_mdio_read(struct net_device *ndev, int phy_id, int loc)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	unsigned long start_time;

	AX_WRITE(&ax_local->ax_spi, MDIOCR_RADDR(loc)
			| MDIOCR_FADDR(phy_id) | MDIOCR_READ, P2_MDIOCR);

	start_time = jiffies;
	while ((AX_READ(&ax_local->ax_spi, P2_MDIOCR) & MDIOCR_VALID) == 0) {
		if (time_after(jiffies, start_time + HZ/100))
			return -EBUSY;
	}

	return AX_READ(&ax_local->ax_spi, P2_MDIODR);
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_mdio_write
 * Purpose:
 * ----------------------------------------------------------------------------
 */
void
ax88796c_mdio_write(struct net_device *ndev, int phy_id, int loc, int val)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	unsigned long start_time;

	AX_WRITE(&ax_local->ax_spi, val, P2_MDIODR);

	AX_WRITE(&ax_local->ax_spi,
			MDIOCR_RADDR(loc) | MDIOCR_FADDR(phy_id)
			| MDIOCR_WRITE, P2_MDIOCR);

	start_time = jiffies;
	while ((AX_READ(&ax_local->ax_spi, P2_MDIOCR) & MDIOCR_VALID) == 0) {
		if (time_after(jiffies, start_time + HZ/100))
			return;
	}

	if (loc == MII_ADVERTISE) {
		AX_WRITE(&ax_local->ax_spi, (BMCR_FULLDPLX | BMCR_ANRESTART |
			  BMCR_ANENABLE | BMCR_SPEED100), P2_MDIODR);
		AX_WRITE(&ax_local->ax_spi, (MDIOCR_RADDR(MII_BMCR) |
			  MDIOCR_FADDR(phy_id) | MDIOCR_WRITE),
			  P2_MDIOCR);

		start_time = jiffies;
		while ((AX_READ(&ax_local->ax_spi, P2_MDIOCR)
					& MDIOCR_VALID) == 0) {
			if (time_after(jiffies, start_time + HZ/100))
				return;
		}
	}
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_set_csums
 * Purpose:
 * ----------------------------------------------------------------------------
 */
void ax88796c_set_csums(struct ax88796c_device *ax_local)
{
	if (ax_local->checksum & AX_RX_CHECKSUM) {
		AX_WRITE(&ax_local->ax_spi, COERCR0_DEFAULT, P4_COERCR0);
		AX_WRITE(&ax_local->ax_spi, COERCR1_DEFAULT, P4_COERCR1);
	} else {
		AX_WRITE(&ax_local->ax_spi, 0, P4_COERCR0);
		AX_WRITE(&ax_local->ax_spi, 0, P4_COERCR1);
	}

	if (ax_local->checksum & AX_TX_CHECKSUM) {
		AX_WRITE(&ax_local->ax_spi, COETCR0_DEFAULT, P4_COETCR0);
		AX_WRITE(&ax_local->ax_spi, COETCR1_TXPPPE, P4_COETCR1);
	} else {
		AX_WRITE(&ax_local->ax_spi, 0, P4_COETCR0);
		AX_WRITE(&ax_local->ax_spi, 0, P4_COETCR1);
	}
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_get_drvinfo
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void ax88796c_get_drvinfo(struct net_device *ndev,
				 struct ethtool_drvinfo *info)
{
	/* Inherit standard device info */
	strncpy(info->driver, DRV_NAME, sizeof(info->driver));
	strncpy(info->version, DRV_VERSION, sizeof(info->version));
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_get_link
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static u32 ax88796c_get_link(struct net_device *ndev)
{
	u32 link;
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	u8 power;

	down(&ax_local->spi_lock);
	power = ax88796c_check_power_and_wake(ax_local);

	link = mii_link_ok(&ax_local->mii);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);
	up(&ax_local->spi_lock);

	return link;


}

/* ----------------------------------------------------------------------------
 * Function Name: ax_get_msglevel
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static u32 ax88796c_get_msglevel(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	return ax_local->msg_enable;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_set_msglevel
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static void ax88796c_set_msglevel(struct net_device *ndev, u32 level)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	ax_local->msg_enable = level;
}


/* ----------------------------------------------------------------------------
 * Function Name: ax_get_settings
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_get_settings(struct net_device *ndev, struct ethtool_cmd *cmd)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	int ret;
	u8 power;

	down(&ax_local->spi_lock);
	power = ax88796c_check_power_and_wake(ax_local);

	ret = mii_ethtool_gset(&ax_local->mii, cmd);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);
	up(&ax_local->spi_lock);

	return ret;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax_set_settings
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int
ax88796c_set_settings(struct net_device *ndev, struct ethtool_cmd *cmd)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	int ret;
	u8 power;

	down(&ax_local->spi_lock);
	power = ax88796c_check_power_and_wake(ax_local);

	ret = mii_ethtool_sset(&ax_local->mii, cmd);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);
	up(&ax_local->spi_lock);
	return ret;

}

/* ----------------------------------------------------------------------------
 * Function Name: ax_nway_reset
 * Purpose:
 * ----------------------------------------------------------------------------
 */
static int ax88796c_nway_reset(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	int ret;
	u8 power;

	down(&ax_local->spi_lock);
	power = ax88796c_check_power_and_wake(ax_local);

	ret = mii_nway_restart(&ax_local->mii);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);
	up(&ax_local->spi_lock);
	return ret;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_ethtool_getmsglevel
 * Purpose: Exported for Ethtool to query driver message level
 * ----------------------------------------------------------------------------
 */
static u32 ax88796c_ethtool_getmsglevel(struct net_device *ndev)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	return ax_local->msg_enable;
}

/* ----------------------------------------------------------------------------
 * Function Name: ax88796c_ethtool_setmsglevel
 * Purpose: Exported for Ethtool to set driver message level
 * ----------------------------------------------------------------------------
 */
static void ax88796c_ethtool_setmsglevel(struct net_device *ndev, u32 level)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	ax_local->msg_enable = level;
}

struct ethtool_ops ax88796c_ethtool_ops = {
	.get_drvinfo		= ax88796c_get_drvinfo,
	.get_link		= ax88796c_get_link,
	.get_msglevel		= ax88796c_get_msglevel,
	.set_msglevel		= ax88796c_set_msglevel,
	.get_settings		= ax88796c_get_settings,
	.set_settings		= ax88796c_set_settings,
	.nway_reset		= ax88796c_nway_reset,
	.get_msglevel		= ax88796c_ethtool_getmsglevel,
	.set_msglevel		= ax88796c_ethtool_setmsglevel,
};

/* ----------------------------------------------------------------------------
 * Function Name: ax_ioctl
 * Purpose:
 * ----------------------------------------------------------------------------
 */
int ax88796c_ioctl(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	struct ax88796c_device *ax_local = to_ax88796c_device(ndev);
	int ret;
	u8 power;

	down(&ax_local->spi_lock);
	power = ax88796c_check_power_and_wake(ax_local);

	ret = generic_mii_ioctl(&ax_local->mii, if_mii(ifr), cmd, NULL);

	if (power)
		ax88796c_set_power_saving(ax_local, ax_local->ps_level);
	up(&ax_local->spi_lock);

	return ret;
}

