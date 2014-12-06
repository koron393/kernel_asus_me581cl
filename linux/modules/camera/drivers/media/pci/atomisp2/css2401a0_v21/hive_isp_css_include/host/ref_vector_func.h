/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2013 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */


#ifndef _REF_VECTOR_FUNC_H_INCLUDED_
#define _REF_VECTOR_FUNC_H_INCLUDED_

#ifndef STORAGE_CLASS_REF_VECTOR_FUNC_H
#define STORAGE_CLASS_REF_VECTOR_FUNC_H extern
#endif

#include "ref_vector_func_types.h"

/** @brief Doubling multiply accumulate
 *
 * @param[in] acc accumulator
 * @param[in] a multiply input
 * @param[in] b multiply input
  *
 * @return		acc + (a*b)
 *
 * This function will do a doubling multiply ont
 * inputs a and b, and will add the result to acc.
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector2w OP_1w_maccd(
	tvector2w acc,
	tvector1w a,
	tvector1w b );

/** @brief Re-aligning multiply
 *
 * @param[in] a multiply input
 * @param[in] b multiply input
 * @param[in] shift shift amount
 *
 * @return		(a*b)>>shift
 *
 * This function will multiply a with b, followed by a right
 * shift with rounding. the result is saturated and casted
 * to single precision.
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w OP_1w_mul_realigning(
	tvector1w a,
	tvector1w b,
	tscalar1w shift );
/** @brief Coring
 *
 * @param[in] coring_vec   Amount of coring based on brightness level
 * @param[in] filt_input   Vector of input pixels on which Coring is applied
 * @param[in] m_CnrCoring0 Coring Level0
 *
 * @return                 vector of filtered pixels after coring is applied
 *
 * This function will perform adaptive coring based on brightness level to
 * remove noise
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w coring(
	tvector1w coring_vec,
	tvector1w filt_input,
	tscalar1w m_CnrCoring0 );

/** @brief Normalised FIR with coefficients [3,4,1]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [3,4,1],
 *-5dB at Fs/2, -90 degree phase shift (quarter pixel)
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_5dB_m90_nrm (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [1,4,3]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1,4,3],
 *-5dB at Fs/2, +90 degree phase shift (quarter pixel)
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_5dB_p90_nrm (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [1,2,1]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1,2,1], -6dB at Fs/2
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [13,16,3]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [13,16,3],
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm_ph0 (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [9,16,7]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [9,16,7],
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm_ph1 (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [5,16,11]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [5,16,11],
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm_ph2 (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with coefficients [1,16,15]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1,16,15],
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm_ph3 (
	const s_1w_1x3_matrix		m);

/** @brief Normalised FIR with programable phase shift
 *
 * @param[in] m	1x3 matrix with pixels
 * @param[in] coeff	phase shift
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [8-coeff,16,8+coeff],
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_6dB_nrm_calc_coeff (
	const s_1w_1x3_matrix		m, tscalar1w_3bit coeff);

/** @brief 3 tab FIR with coefficients [1,1,1]
 *
 * @param[in] m	1x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * FIR with coefficients [1,1,1], -9dB at Fs/2 normalized with factor 1/2
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x3m_9dB_nrm (
	const s_1w_1x3_matrix		m);


/** @brief Normalised 2D FIR with coefficients  [1;2;1] * [1,2,1]
 *
 * @param[in] m	3x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients  [1;2;1] * [1,2,1]
 * Unity gain filter through repeated scaling and rounding
 *	- 6 rotate operations per output
 *	- 8 vector operations per output
 * _______
 *   14 total operations
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir3x3m_6dB_nrm (
	const s_1w_3x3_matrix		m);

/** @brief Normalised 2D FIR with coefficients  [1;1;1] * [1,1,1]
 *
 * @param[in] m	3x3 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1;1;1] * [1,1,1]
 *
 * (near) Unity gain filter through repeated scaling and rounding
 *	- 6 rotate operations per output
 *	- 8 vector operations per output
 * _______
 *   14 operations
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir3x3m_9dB_nrm (
	const s_1w_3x3_matrix		m);

/** @brief Normalised dual output 2D FIR with coefficients  [1;2;1] * [1,2,1]
 *
 * @param[in] m	4x3 matrix with pixels
 *
 * @return		two filtered outputs (2x1 matrix)
 *
 * This function will calculate the
 * Normalised FIR with coefficients  [1;2;1] * [1,2,1]
 * and produce two outputs (vertical)
 * Unity gain filter through repeated scaling and rounding
 * compute two outputs per call to re-use common intermediates
 *	- 4 rotate operations per output
 *	- 6 vector operations per output (alternative possible, but in this
 *	    form it's not obvious to re-use variables)
 * _______
 *   10 total operations
 */
 STORAGE_CLASS_REF_VECTOR_FUNC_H s_1w_2x1_matrix fir3x3m_6dB_out2x1_nrm (
	const s_1w_4x3_matrix		m);

/** @brief Normalised dual output 2D FIR with coefficients [1;1;1] * [1,1,1]
 *
 * @param[in] m	4x3 matrix with pixels
 *
 * @return		two filtered outputs (2x1 matrix)
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1;1;1] * [1,1,1]
 * and produce two outputs (vertical)
 * (near) Unity gain filter through repeated scaling and rounding
 * compute two outputs per call to re-use common intermediates
 *	- 4 rotate operations per output
 *	- 7 vector operations per output (alternative possible, but in this
 *	    form it's not obvious to re-use variables)
 * _______
 *   11 total operations
 */
STORAGE_CLASS_REF_VECTOR_FUNC_H s_1w_2x1_matrix fir3x3m_9dB_out2x1_nrm (
	const s_1w_4x3_matrix		m);

/** @brief Normalised 2D FIR 5x5
 *
 * @param[in] m	5x5 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1;1;1] * [1;2;1] * [1,2,1] * [1,1,1]
 * and produce a filtered output
 * (near) Unity gain filter through repeated scaling and rounding
 *	- 20 rotate operations per output
 *	- 28 vector operations per output
 * _______
 *   48 total operations
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir5x5m_15dB_nrm (
	const s_1w_5x5_matrix	m);

/** @brief Normalised FIR 1x5
 *
 * @param[in] m	1x5 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1,2,1] * [1,1,1] = [1,4,6,4,1]
 * and produce a filtered output
 * (near) Unity gain filter through repeated scaling and rounding
 *	- 4 rotate operations per output
 *	- 5 vector operations per output
 * _______
 *   9 total operations
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x5m_12dB_nrm (
	const s_1w_1x5_matrix m);

/** @brief Normalised 2D FIR 5x5
 *
 * @param[in] m	5x5 matrix with pixels
 *
 * @return		filtered output
 *
 * This function will calculate the
 * Normalised FIR with coefficients [1;2;1] * [1;2;1] * [1,2,1] * [1,2,1]
 * and produce a filtered output
 * (near) Unity gain filter through repeated scaling and rounding
 *	- 20 rotate operations per output
 *	- 30 vector operations per output
 * _______
 *   50 total operations
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir5x5m_12dB_nrm (
	const s_1w_5x5_matrix m);

STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x5m_box (
	s_1w_1x5_matrix m);

STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w fir1x9m_box (
	s_1w_1x9_matrix m);

/** @brief Mean of 1x3 matrix
 *
 *  @param[in] m 1x3 matrix with pixels
 *
 *  @return mean of 1x3 matrix
 *
 * This function calculates the mean of 1x3 pixels,
 * with a factor of 1/4.
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w mean1x3m(
	s_1w_1x3_matrix m);

/** @brief Mean of 3x3 matrix
 *
 *  @param[in] m 3x3 matrix with pixels
 *
 *  @return mean of 3x3 matrix
 *
 * This function calculates the mean of 1x3 pixels,
 * with a factor of 1/16.
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w mean3x3m(
	s_1w_3x3_matrix m);

/** @brief Mean of 1x4 matrix
 *
 *  @param[in] m 1x4 matrix with pixels
 *
 *  @return mean of 1x4 matrix
 *
 * This function calculates the mean of 1x4 pixels
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w mean1x4m(
	s_1w_1x4_matrix m);

/** @brief Mean of 4x4 matrix
 *
 *  @param[in] m 4x4 matrix with pixels
 *
 *  @return mean of 4x4 matrix
 *
 * This function calculates the mean of 4x4 matrix with pixels
*/
STORAGE_CLASS_REF_VECTOR_FUNC_H tvector1w mean4x4m(
	s_1w_4x4_matrix m);

#endif /*_REF_VECTOR_FUNC_H_INCLUDED_*/

