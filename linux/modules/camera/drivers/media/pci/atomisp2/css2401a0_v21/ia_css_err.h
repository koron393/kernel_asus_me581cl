/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_ERR_H
#define __IA_CSS_ERR_H

/** Errors, these values are used as the return value for most
 *  functions in this API.
 */
enum ia_css_err {
	IA_CSS_SUCCESS,
	IA_CSS_ERR_INTERNAL_ERROR,
	IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY,
	IA_CSS_ERR_INVALID_ARGUMENTS,
	IA_CSS_ERR_SYSTEM_NOT_IDLE,
	IA_CSS_ERR_MODE_HAS_NO_VIEWFINDER,
	IA_CSS_ERR_QUEUE_IS_FULL,
	IA_CSS_ERR_QUEUE_IS_EMPTY,
	IA_CSS_ERR_RESOURCE_NOT_AVAILABLE,
	IA_CSS_ERR_RESOURCE_LIST_TO_SMALL,
	IA_CSS_ERR_RESOURCE_ITEMS_STILL_ALLOCATED,
	IA_CSS_ERR_RESOURCE_EXHAUSTED,
	IA_CSS_ERR_RESOURCE_ALREADY_ALLOCATED,
	IA_CSS_ERR_VERSION_MISMATCH,
	IA_CSS_ERR_NOT_SUPPORTED
};

enum sh_css_rx_irq_info {
	IA_CSS_RX_IRQ_INFO_SINGLE_BIT_PACKET_HEADER_ERR	= 1U << 0,
	/**< Single bit packet header error corrected */
	IA_CSS_RX_IRQ_MULTI_BIT_PACKET_HEADER_ERR	= 1U << 1,
	/**< Multiple bit packet header errors detected */
	IA_CSS_RX_IRQ_PAYLOAD_CRC_ERR	= 1U << 2,
	/**< Payload checksum (CRC) error */
	IA_CSS_RX_IRQ_FIFO_OVERFLOW	= 1U << 3,
	/**< FIFO overflow */
	IA_CSS_RX_IRQ_RESERVED_SHORT_PACKET_DATA_TYPE	= 1U << 4,
	/**< Reserved short packet data type detected */
	IA_CSS_RX_IRQ_RESERVED_LONG_PACKET_DATA_TYPE	= 1U << 5,
	/**< Reserved long packed data type detected */
	IA_CSS_RX_IRQ_INCOMPLETE_LONG_PACKET_ERR	= 1U << 6,
	/**< Incomplete long packet detected */
	IA_CSS_RX_IRQ_FRAME_SYNC_ERROR	= 1U << 7,
	/**< Frame sync error */
	IA_CSS_RX_IRQ_LINE_SYNC_ERROR	= 1U << 8,
	/**< Line sync error */
	IA_CSS_RX_IRQ_DPHY_SOT_ERR	= 1U << 9,
	/**< DPHY start of transmission error */
	IA_CSS_RX_IRQ_DPHY_SYNC_ERR	= 1U << 10,
	/**< DPHY synchronization error */
	IA_CSS_RX_IRQ_ESC_MODE_ERR	= 1U << 11,
	/**< Escape mode error */
	IA_CSS_RX_IRQ_ESC_MODE_TRIGGER_EVENT	= 1U << 12,
	/**< Escape mode trigger event */
	IA_CSS_RX_IRQ_ESC_MODE_ULPS_FOR_DATA	= 1U << 13,
	/**< Escape mode ultra-low power state for data lane(s) */
	IA_CSS_RX_IRQ_ESC_MODE_ULPS_EXIT_FOR_CLOCK	= 1U << 14,
	/**< Escape mode ultra-low power state exit for clock lane */
};


#endif /* __IA_CSS_ERR_H */
