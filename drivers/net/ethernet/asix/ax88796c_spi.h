#ifndef _AX88796C_SPI_H
#define _AX88796C_SPI_H

#include <linux/spi/spi.h>

/* Definition of SPI command */
#define AX_SPICMD_WRITE_TXQ		0x02
#define AX_SPICMD_READ_REG		0x03
#define AX_SPICMD_READ_STATUS		0x05
#define AX_SPICMD_READ_RXQ		0x0B
#define AX_SPICMD_BIDIR_WRQ		0xB2
#define AX_SPICMD_WRITE_REG		0xD8
#define AX_SPICMD_EXIT_PWD		0xAB

static const u8 rx_cmd_buf[5] = {AX_SPICMD_READ_RXQ, 0xFF, 0xFF, 0xFF, 0xFF};
static const u8 tx_cmd_buf[4] = {AX_SPICMD_WRITE_TXQ, 0xFF, 0xFF, 0xFF};

struct axspi_data {
	struct spi_device	*spi;
	struct spi_message	rx_msg;
	struct spi_transfer	spi_rx_xfer[2];
	u8			cmd_buf[6];
	u8			comp;
};

struct spi_status {
	u16 isr;
	u8 status;
#	define AX_STATUS_READY		0x80
};

int axspi_read_rxq(struct axspi_data *ax_spi, void *data, int len);
int axspi_write_txq(struct axspi_data *ax_spi, void *data, int len);
u16 axspi_read_reg(struct axspi_data *ax_spi, u8 reg);
void axspi_write_reg(struct axspi_data *ax_spi, u8 reg, u16 value);
void axspi_read_status(struct axspi_data *ax_spi, struct spi_status *status);
void axspi_wakeup(struct axspi_data *ax_spi);

static inline u16 AX_READ(struct axspi_data *ax_spi, u8 offset)
{
	return axspi_read_reg(ax_spi, offset);
}

static inline void AX_WRITE(struct axspi_data *ax_spi, u16 value, u8 offset)
{
	axspi_write_reg(ax_spi, offset, value);
}

static inline void AX_READ_STATUS(struct axspi_data *ax_spi,
		struct spi_status *status)
{
	axspi_read_status(ax_spi, status);
}

static inline void AX_WAKEUP(struct axspi_data *ax_spi)
{
	return axspi_wakeup(ax_spi);
}
#endif

