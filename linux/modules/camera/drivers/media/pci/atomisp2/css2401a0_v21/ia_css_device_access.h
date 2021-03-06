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

#ifndef _IA_CSS_DEVICE_ACCESS_H_
#define _IA_CSS_DEVICE_ACCESS_H_

#include <type_support.h> /* for uint*, size_t */
#include <system_types.h> /* for hrt_address */
#include <ia_css_env.h>   /* for ia_css_hw_access_env */

void
ia_css_device_access_init(const struct ia_css_hw_access_env *env);

uint8_t
ia_css_device_load_uint8(const hrt_address addr);

uint16_t
ia_css_device_load_uint16(const hrt_address addr);

uint32_t
ia_css_device_load_uint32(const hrt_address addr);

uint64_t
ia_css_device_load_uint64(const hrt_address addr);

void
ia_css_device_store_uint8(const hrt_address addr, const uint8_t data);

void
ia_css_device_store_uint16(const hrt_address addr, const uint16_t data);

void
ia_css_device_store_uint32(const hrt_address addr, const uint32_t data);

void
ia_css_device_store_uint64(const hrt_address addr, const uint64_t data);

void
ia_css_device_load(const hrt_address addr, void *data, const size_t size);

void
ia_css_device_store(const hrt_address addr, const void *data, const size_t size);

#endif /* _IA_CSS_DEVICE_ACCESS_H_ */
