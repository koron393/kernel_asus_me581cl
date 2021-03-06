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

#ifndef __SH_CSS_STRUCT_H
#define __SH_CSS_STRUCT_H

/* This header files contains the definition of the
   sh_css struct and friends; locigally the file would
   probably be called sh_css.h after the pattern
   <type>.h but sh_css.h is the predecesssor of ia_css.h
   so this could cause confusion; hence the _struct
   in the filename
*/

#include <type_support.h>
#include <system_types.h>
#include "ia_css_pipeline.h"
#include "ia_css_pipe_public.h"
#include "ia_css_frame_public.h"
#include "ia_css_queue.h"
#include "ia_css_irq.h"

struct sh_css {
	struct ia_css_pipe            *active_pipes[IA_CSS_PIPELINE_NUM_MAX];
	/* All of the pipes created at any point of time. At this moment there can
	 * be no more than MAX_SP_THREADS of them because pipe_num is reused as SP
	 * thread_id to which a pipe's pipeline is associated. At a later point, if
	 * we support more pipe objects, we should add test code to test that
	 * possibility. Also, active_pipes[] should be able to hold only
	 * SH_CSS_MAX_SP_THREADS objects. Anything else is misleading. */
	struct ia_css_pipe            *all_pipes[IA_CSS_PIPELINE_NUM_MAX];
	void * (*malloc)(size_t bytes, bool zero_mem);
	void (*free)(void *ptr);
	void (*flush)(struct ia_css_acc_fw *fw);
	bool                           check_system_idle;
	bool                           stop_copy_preview;
	unsigned int                   num_cont_raw_frames;
#if defined(USE_INPUT_SYSTEM_VERSION_2) || defined(USE_INPUT_SYSTEM_VERSION_2401)
	unsigned int                   num_mipi_frames[N_CSI_PORTS];
	struct ia_css_frame           *mipi_frames[N_CSI_PORTS][NUM_MIPI_FRAMES_PER_STREAM];
#endif
	hrt_vaddress                   sp_bin_addr;
	hrt_data                       page_table_base_index;
	unsigned int                   size_mem_words;
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
	unsigned int                   mipi_sizes_for_check[N_CSI_PORTS][IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT];
#endif
	bool                           contiguous;
	enum ia_css_irq_type           irq_type;
	unsigned int                   pipe_counter;
};

extern struct sh_css my_css;

#endif /* __SH_CSS_STRUCT_H */

