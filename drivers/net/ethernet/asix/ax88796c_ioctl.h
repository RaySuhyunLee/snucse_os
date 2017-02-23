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

#ifndef _AX88796C_IOCTL_H
#define _AX88796C_IOCTL_H

extern struct ethtool_ops ax88796c_ethtool_ops;

u8 ax88796c_check_power(struct ax88796c_device *ax_local);
u8 ax88796c_check_power_and_wake(struct ax88796c_device *ax_local);
void ax88796c_set_power_saving(struct ax88796c_device *ax_local, u8 ps_level);
int ax88796c_mdio_read(struct net_device *dev, int phy_id, int loc);
void ax88796c_mdio_write(struct net_device *dev, int phy_id, int loc, int val);
void ax88796c_set_csums(struct ax88796c_device *ax_local);
int ax88796c_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

#endif
