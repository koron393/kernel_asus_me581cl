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

#ifndef _ISP_TYPES_H_
#define _ISP_TYPES_H_

/* Binary independent types */

#include <ia_css_binary.h>
#include "isp_const.h"
#ifdef __ISP
#include "isp_formats.isp.h"
#endif

//-------------------


/* ISP binary identifiers.
   These determine the order in which the binaries are looked up, do not change
   this!
   Also, the SP firmware uses this same order (isp_loader.hive.c).
   Also, gen_firmware.c uses this order in its firmware_header.
*/
/* The binary id is used in pre-processor expressions so we cannot
 * use an enum here. */
#define SH_CSS_BINARY_ID_COPY                 0
#define SH_CSS_BINARY_ID_BAYER_DS             1
#define SH_CSS_BINARY_ID_VF_PP_FULL           2
#define SH_CSS_BINARY_ID_VF_PP_OPT            3
#define SH_CSS_BINARY_ID_CAPTURE_PP           4
#define SH_CSS_BINARY_ID_PRE_ISP              5
#define SH_CSS_BINARY_ID_PRE_ISP_2            6
#define SH_CSS_BINARY_ID_GDC                  7
#define SH_CSS_BINARY_ID_POST_ISP             8
#define SH_CSS_BINARY_ID_POST_ISP_2           9
#define SH_CSS_BINARY_ID_PRE_ANR             10
#define SH_CSS_BINARY_ID_ANR                 11
#define SH_CSS_BINARY_ID_ANR_2               12
#define SH_CSS_BINARY_ID_POST_ANR            13
#define SH_CSS_BINARY_ID_PREVIEW_CONT_DS     14
#define SH_CSS_BINARY_ID_PREVIEW_DS          15
#define SH_CSS_BINARY_ID_PREVIEW_DZ          16
#define SH_CSS_BINARY_ID_PREVIEW_DZ_2        17
#define SH_CSS_BINARY_ID_PREVIEW_DEC         18
#define SH_CSS_BINARY_ID_PREVIEW_DEC_2       19

#define SH_CSS_BINARY_ID_ACCELERATION        20

/* skycam product pipelines */
#define SH_CSS_BINARY_ID_PRIMARY                            21
#define SH_CSS_BINARY_ID_VIDEO                              22

/* skycam test pipelines */
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_NORM              23
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_LIN               24
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_ACC_SHD           25
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_ACC_AWB           26
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_3A                27
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_ACC_AF            28
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_OBGRID            29
#define SH_CSS_BINARY_ID_VIDEO_TEST_INPUTCORRECTION         30
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_BAYER_DENOISE       31
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_DEMOSAIC            32
#define SH_CSS_BINARY_ID_VIDEO_TEST_SHD_BNR_DM_RGBPP        33
#define SH_CSS_BINARY_ID_VIDEO_TEST_ADD_RGBPP               34
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_YUVP1               35
#define SH_CSS_BINARY_ID_VIDEO_TEST_SPATIAL_FF              36
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_DVS               37
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_YUVP2               38
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_XNR               39
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_TNR               40
#define SH_CSS_BINARY_ID_VIDEO_PARTIALPIPE_INPUTCOR_FULL    41
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_ACC_AE            42
#define SH_CSS_BINARY_ID_VIDEO_WITH_3A                      43
#define SH_CSS_BINARY_ID_VIDEO_RAW                          44
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_ACC_AWB_FR        45
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_DM_RGBPP          46
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_DM_RGBPP_STRIPED  47
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_ANR                 48
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_BDS_ACC           49
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_IF_STRIPED        50
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_OUTPUT_SYSTEM     51

#define SH_CSS_BINARY_ID_IF_TO_DPC                          52
#define SH_CSS_BINARY_ID_IF_TO_BDS                          53
#define SH_CSS_BINARY_ID_IF_TO_NORM                         54
#define SH_CSS_BINARY_ID_IF_TO_OB                           55
#define SH_CSS_BINARY_ID_IF_TO_LIN                          56
#define SH_CSS_BINARY_ID_IF_TO_SHD                          57
#define SH_CSS_BINARY_ID_IF_TO_BNR                          58
#define SH_CSS_BINARY_ID_IF_TO_DM_WO_ANR_STATS              59
#define SH_CSS_BINARY_ID_IF_TO_DM_3A_WO_ANR                 60
#define SH_CSS_BINARY_ID_IF_TO_RGB                          61
#define SH_CSS_BINARY_ID_IF_TO_YUVP1                        62
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_WO_ANR                 63
#define SH_CSS_BINARY_ID_IF_TO_DM_WO_STATS                  64
#define SH_CSS_BINARY_ID_IF_TO_DM_3A                        65
#define SH_CSS_BINARY_ID_IF_TO_YUVP2                        66
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_ANR_VIA_ISP            67
#define SH_CSS_BINARY_ID_VIDEO_IF_TO_DVS                    68
#define SH_CSS_BINARY_ID_VIDEO_IF_TO_TNR                    69
#define SH_CSS_BINARY_ID_IF_NORM_LIN_SHD_BNR_STRIPED        70
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_DVS_STAT            71
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_LACE_STAT           72
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_YUVP1_S             73
#define SH_CSS_BINARY_ID_IF_TO_BDS_STRIPED                  74
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_ANR_STRIPED         75
#define SH_CSS_BINARY_ID_IF_NORM_LIN_SHD_AWB_BNR_STRIPED    76
#define SH_CSS_BINARY_ID_VIDEO_TEST_ACC_YUVP2_STRIPED       77
#define SH_CSS_BINARY_ID_IF_NORM_LIN_SHD_AF_BNR_STRIPED     78
#define SH_CSS_BINARY_ID_IF_NORM_LIN_SHD_AWBFR_BNR_STRIPED  79
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_NO_DPC_OB_STATS        80
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_NO_DPC_OB_AE	    81
#define SH_CSS_BINARY_ID_IF_NORM_LIN_SHD_AE_BNR_STRIPED     82
#define SH_CSS_BINARY_ID_IF_TO_BDS_RGBP_DVS_STATS           83
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_NO_DPC_OB              84
#define SH_CSS_BINARY_ID_IF_TO_BDS_RGBP_DVS_STATS_STRIPED   85
#define SH_CSS_BINARY_ID_SC_VIDEO_HIGH_RESOLUTION           86
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_TNR_STRIPED       87
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_DVS_STRIPED       88
#define SH_CSS_BINARY_ID_IF_TO_YUVP2_NO_ISP_EXITS           89
#define SH_CSS_BINARY_ID_VIDEO_KERNELTEST_OBGRID_STRIPED    90

#define SH_CSS_BINARY_NUM_IDS                               91

#if defined(__ISP) || defined(__SP)
struct isp_uds_config {
	int      hive_dx;
	int      hive_dy;
	unsigned hive_woix;
	unsigned hive_bpp; /* gdc_bits_per_pixel */
	unsigned hive_bci;
};

struct s_isp_gdcac_config {
	unsigned nbx;
	unsigned nby;
};

/* output.hive.c request information */
typedef enum {
  output_y_channel,
  output_c_channel,
  OUTPUT_NUM_CHANNELS
} output_channel_type;

typedef struct s_output_dma_info {
  unsigned            cond;		/* Condition for transfer */
  output_channel_type channel_type;
  dma_channel         channel;
  unsigned            width_a;
  unsigned            width_b;
  unsigned            stride;
  unsigned            v_delta;	        /* Offset for v address to do cropping */
  char               *x_base;           /* X base address */
} output_dma_info_type;
#endif

/* Input stream formats, these correspond to the MIPI formats and the way
 * the CSS receiver sends these to the input formatter.
 * The bit depth of each pixel element is stored in the global variable
 * isp_bits_per_pixel.
 * NOTE: for rgb565, we set isp_bits_per_pixel to 565, for all other rgb
 * formats it's the actual depth (4, for 444, 8 for 888 etc).
 */
enum sh_stream_format {
	sh_stream_format_yuv420_legacy,
	sh_stream_format_yuv420,
	sh_stream_format_yuv422,
	sh_stream_format_rgb,
	sh_stream_format_raw,
	sh_stream_format_binary,	/* bytestream such as jpeg */
};

struct s_isp_frames {
	/* global variables that are written to by either the SP or the host,
	   every ISP binary needs these. */
	/* output frame */
	char *xmem_base_addr_y;
	char *xmem_base_addr_uv;
	char *xmem_base_addr_u;
	char *xmem_base_addr_v;
	/* 2nd output frame */
	char *xmem_base_addr_second_out_y;
	char *xmem_base_addr_second_out_u;
	char *xmem_base_addr_second_out_v;
	/* input yuv frame */
	char *xmem_base_addr_y_in;
	char *xmem_base_addr_u_in;
	char *xmem_base_addr_v_in;
	/* input raw frame */
	char *xmem_base_addr_raw;
	/* output raw frame */
	char *xmem_base_addr_raw_out;
	/* capture_pp intermediate frame */
	char *xmem_base_addr_y_cpp;
	char *xmem_base_addr_u_cpp;
	char *xmem_base_addr_v_cpp;
	/* viewfinder output (vf_veceven) */
	char *xmem_base_addr_vfout_y;
	char *xmem_base_addr_vfout_u;
	char *xmem_base_addr_vfout_v;
	/* overlay frame (for vf_pp) */
	char *xmem_base_addr_overlay_y;
	char *xmem_base_addr_overlay_u;
	char *xmem_base_addr_overlay_v;
	/* pre-gdc output frame (gdc input) */
	char *xmem_base_addr_qplane_r;
	char *xmem_base_addr_qplane_ratb;
	char *xmem_base_addr_qplane_gr;
	char *xmem_base_addr_qplane_gb;
	char *xmem_base_addr_qplane_b;
	char *xmem_base_addr_qplane_batr;
	/* YUV as input, used by postisp binary */
	char *xmem_base_addr_yuv_16_y;
	char *xmem_base_addr_yuv_16_u;
	char *xmem_base_addr_yuv_16_v;
};

#endif /* _ISP_TYPES_H_ */
