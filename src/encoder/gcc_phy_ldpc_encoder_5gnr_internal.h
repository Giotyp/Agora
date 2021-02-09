// This is FlexRAN's header file modified to work with GCC.

/**********************************************************************
 *
 * INTEL CONFIDENTIAL
 * Copyright 2009-2019 Intel Corporation All Rights Reserved.
 * 
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and proprietary
 * and confidential information of Intel Corporation and its suppliers and
 * licensors, and is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 * 
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 * 
 * Unless otherwise agreed by Intel in writing, you may not remove or alter this
 * notice or any other notice embedded in Materials by Intel or Intel's suppliers
 * or licensors in any way.
 * 
 *  version: SDK-jenkins-FlexRAN-SDK-REL-448-g3be238
 *
 **********************************************************************/

/*!
    \file   phy_ldpc_encoder_5gnr_internal.h
    \brief  Source code of External API for 5GNR LDPC Encoder functions

    __Overview:__

    The bblib_ldpc_encoder_5gnr kernel is a 5G NR LDPC Encoder function.
    It is implemeneted as defined in TS38212 5.3.2

    __Requirements and Test Coverage:__

    BaseGraph 1(.2). Lifting Factor >176.

    __Algorithm Guidance:__

    The algorithm is implemented as defined in TS38212 5.3.2.
*/

#ifndef _PHY_LDPC_ENCODER_5GNR_INTERNAL_H_
#define _PHY_LDPC_ENCODER_5GNR_INTERNAL_H_

#include <immintrin.h> // AVX

#include "common_typedef_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROC_BYTES 64
#define WAYS_144to256 2
#define WAYS_72to128 4
#define WAYS_36to64 8
#define WAYS_18to32 16
#define WAYS_2to16 16

void AdapterFrom288to384(int8_t** pBuff0, int8_t* pBuff1, uint16_t zcSize,
    uint32_t cbLen, int8_t direct); //1ways
void Adapter2waysFrom144to256(int8_t** pbuff0, int8_t* pbuff1,
    uint16_t zcSize, uint32_t cbLen, int8_t direct); //2ways
void Adapter4waysFrom72to128(int8_t** pbuff0, int8_t* pbuff1, uint16_t zcSize,
    uint32_t cbLen, int8_t direct); //4ways
void Adapter8waysFrom36to64(int8_t** pbuff0, int8_t* pbuff1, uint16_t zcSize,
    uint32_t cbLen, int8_t direct); //8ways
void Adapter16waysFrom18to32(int8_t** pbuff0, int8_t* pbuff1, uint16_t zcSize,
    uint32_t cbLen, int8_t direct); //16ways
void Adapter16waysFrom2to16(int8_t** pbuff0, int8_t* pbuff1, uint16_t zcSize,
    uint32_t cbLen, int8_t direct); //16ways

typedef void (*LDPC_ADAPTER_P)(int8_t**, int8_t*, uint16_t, uint32_t, int8_t);
LDPC_ADAPTER_P LdpcSelectAdapterFunc(uint16_t zcSize);

#define PROC_BYTES 64
#define I_LS_NUM 8
#define ZC_MAX 384

#define BG1_COL_TOTAL 68
#define BG1_ROW_TOTAL 46
#define BG1_COL_INF_NUM 22
#define BG1_NONZERO_NUM 307

#define BG2_COL_TOTAL 52
#define BG2_ROW_TOTAL 42
#define BG2_COL_INF_NUM 10
#define BG2_NONZERO_NUM 188

void LdpcEncInitial();

void LdpcEncoderBg1(int8_t* pDataIn, int8_t* pDataOut,
    const int16_t* pMatrixNumPerCol, const int16_t* pAddr,
    const int16_t* pShiftMatrix, int16_t zcSize);

/* Table generated for BG1 from H Matrix in Table 5.3.2-3 in 38.212 */
/* Number of non-null elements per columns in BG1 */
static constexpr int16_t kBg1MatrixNumPerCol[BG1_COL_TOTAL] = { 30, 28, 7, 11, 9,
    4, 8, 12, 8, 7, 12, 10, 12, 11, 10, 7, 10, 10, 13, 7, 8, 11, 9, 3, 4, 4, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* Table generated for BG1 from H Matrix in Table 5.3.2-3 in 38.212 */
/* Addresses of each non-null elements in matrix (removed the 4x4 parity non identify matrix) */
static constexpr int16_t kBg1Address[BG1_NONZERO_NUM] = { 0, 64, 128, 192, 256,
    320, 384, 448, 512, 576, 704, 768, 832, 896, 960, 1088, 1216, 1280, 1408,
    1536, 1664, 1792, 1920, 2048, 2176, 2304, 2432, 2560, 2688, 2816, 0, 128,
    192, 256, 320, 448, 512, 576, 640, 704, 768, 960, 1024, 1152, 1216, 1344,
    1472, 1600, 1728, 1856, 1984, 2112, 2240, 2368, 2496, 2624, 2752, 2880, 0,
    64, 128, 640, 1472, 1664, 2112, 0, 64, 192, 320, 512, 832, 1024, 1280, 1536,
    2496, 2624, 64, 128, 192, 448, 640, 1536, 1664, 1792, 2688, 0, 64, 128,
    1344, 0, 128, 192, 384, 1600, 1728, 2240, 2880, 64, 128, 192, 448, 640, 832,
    1216, 1600, 1984, 2176, 2496, 2816, 64, 128, 192, 448, 640, 1216, 1728,
    2560, 0, 64, 128, 1280, 2432, 2624, 2816, 0, 128, 192, 384, 576, 768, 960,
    1216, 1472, 1920, 2432, 2880, 0, 64, 192, 384, 576, 768, 1024, 1280, 1536,
    2112, 0, 64, 192, 320, 512, 704, 896, 1152, 1408, 2048, 2240, 2432, 0, 128,
    192, 384, 576, 768, 960, 1152, 1408, 1920, 2368, 64, 128, 192, 448, 640,
    1088, 1600, 1856, 2048, 2304, 0, 64, 128, 896, 1664, 2176, 2304, 0, 64, 192,
    320, 512, 704, 896, 1088, 1344, 2752, 64, 128, 192, 384, 576, 896, 1088,
    1408, 2176, 2560, 0, 128, 192, 384, 576, 768, 960, 1152, 1472, 1856, 2304,
    2624, 2752, 0, 64, 128, 512, 1152, 1792, 2496, 0, 128, 192, 384, 576, 832,
    1024, 1344, 0, 64, 192, 320, 512, 704, 896, 1088, 1344, 1792, 2112, 320,
    512, 704, 1024, 1280, 1536, 1984, 2240, 2816, 704, 832, 2368, 512, 1920,
    2048, 2688, 960, 1856, 1984, 2752, 256, 320, 384, 448, 512, 576, 640, 704,
    768, 832, 896, 960, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472, 1536,
    1600, 1664, 1728, 1792, 1856, 1920, 1984, 2048, 2112, 2176, 2240, 2304,
    2368, 2432, 2496, 2560, 2624, 2688, 2752, 2816, 2880 };

/* Table generated for BG1 from H Matrix in Table 5.3.2-3 in 38.212 */
__attribute__((aligned(
    64))) static constexpr int16_t kBg1HShiftMatrix[BG1_NONZERO_NUM * I_LS_NUM]
    = { 250, 2, 106, 121, 157, 205, 183, 220, 112, 103, 77, 160, 177, 206, 40,
          7, 60, 151, 64, 112, 195, 128, 216, 221, 127, 37, 167, 157, 149, 139,
          69, 111, 89, 102, 236, 44, 4, 182, 98, 41, 42, 96, 64, 42, 73, 249,
          156, 23, 25, 86, 95, 2, 161, 198, 173, 167, 151, 149, 226, 239, 185,
          149, 147, 243, 187, 159, 117, 84, 194, 7, 248, 49, 186, 86, 139, 173,
          124, 63, 20, 159, 167, 236, 215, 165, 157, 100, 71, 117, 121, 10, 93,
          150, 22, 136, 104, 197, 151, 222, 229, 131, 31, 160, 151, 72, 116,
          177, 167, 149, 157, 104, 177, 243, 167, 49, 127, 194, 137, 59, 173,
          95, 217, 151, 139, 163, 229, 39, 136, 28, 109, 21, 65, 224, 170, 73,
          157, 167, 110, 220, 86, 67, 21, 32, 49, 47, 116, 41, 191, 102, 246,
          231, 211, 83, 55, 233, 142, 112, 207, 163, 9, 142, 219, 244, 142, 234,
          63, 8, 188, 120, 220, 109, 225, 211, 104, 58, 164, 182, 236, 199, 105,
          195, 132, 225, 206, 61, 164, 51, 23, 142, 240, 28, 102, 182, 127, 59,
          109, 163, 155, 245, 76, 11, 14, 16, 1, 158, 159, 149, 190, 205, 244,
          157, 61, 7, 75, 155, 152, 84, 120, 151, 173, 35, 255, 251, 164, 147,
          181, 0, 239, 117, 144, 211, 216, 185, 51, 131, 31, 28, 12, 123, 109,
          78, 229, 144, 171, 63, 211, 115, 241, 252, 154, 160, 222, 172, 103,
          173, 22, 62, 122, 90, 9, 121, 137, 179, 6, 61, 139, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 307, 76, 205, 276, 332, 195,
          278, 9, 307, 366, 48, 77, 313, 142, 241, 260, 145, 187, 30, 298, 71,
          222, 159, 102, 230, 210, 185, 175, 113, 80, 19, 250, 87, 181, 14, 62,
          179, 232, 101, 102, 186, 2, 13, 130, 213, 205, 24, 72, 194, 252, 100,
          323, 320, 269, 258, 52, 113, 135, 50, 76, 328, 339, 89, 81, 8, 369,
          73, 0, 115, 165, 177, 338, 206, 158, 93, 314, 288, 332, 275, 316, 274,
          235, 76, 19, 14, 181, 144, 256, 102, 216, 161, 199, 257, 17, 194, 335,
          149, 331, 267, 153, 333, 111, 266, 344, 383, 215, 148, 346, 78, 331,
          160, 56, 290, 383, 242, 101, 37, 317, 178, 63, 264, 177, 139, 163,
          288, 129, 132, 1, 321, 174, 210, 197, 61, 229, 289, 15, 109, 295, 305,
          351, 133, 232, 57, 341, 339, 361, 17, 342, 231, 166, 18, 8, 248, 163,
          11, 201, 2, 214, 357, 200, 341, 92, 57, 50, 318, 280, 233, 260, 82,
          217, 88, 212, 114, 354, 303, 312, 5, 175, 313, 215, 99, 53, 137, 136,
          202, 297, 106, 354, 304, 241, 39, 47, 89, 81, 328, 132, 114, 131, 300,
          253, 303, 347, 358, 22, 312, 312, 242, 240, 271, 18, 63, 74, 55, 132,
          27, 147, 21, 288, 114, 180, 331, 205, 224, 4, 244, 297, 330, 13, 39,
          225, 82, 115, 289, 213, 346, 112, 357, 51, 368, 188, 12, 375, 97, 274,
          105, 157, 67, 334, 57, 59, 234, 258, 266, 274, 115, 370, 115, 170, 90,
          287, 218, 269, 78, 256, 168, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 73, 303, 68, 220, 233, 83, 289, 12, 295, 189, 16, 229,
          39, 78, 229, 257, 64, 301, 177, 289, 270, 11, 91, 210, 187, 259, 151,
          32, 226, 234, 15, 7, 208, 205, 292, 88, 133, 244, 14, 147, 235, 290,
          69, 260, 181, 79, 249, 172, 210, 27, 222, 170, 207, 298, 102, 154,
          228, 101, 103, 294, 80, 80, 50, 110, 20, 49, 27, 30, 50, 130, 302,
          140, 162, 280, 77, 47, 261, 280, 197, 207, 211, 110, 318, 293, 65,
          240, 161, 38, 175, 39, 227, 61, 21, 295, 29, 158, 228, 133, 202, 175,
          50, 75, 303, 101, 96, 308, 296, 192, 227, 4, 200, 79, 25, 161, 270,
          304, 80, 15, 80, 71, 40, 179, 124, 259, 162, 106, 281, 293, 36, 169,
          60, 41, 133, 23, 64, 126, 215, 129, 303, 13, 286, 48, 45, 130, 187,
          140, 164, 300, 253, 318, 231, 290, 299, 294, 20, 22, 55, 181, 133,
          295, 164, 232, 151, 105, 130, 291, 55, 105, 15, 76, 283, 53, 76, 311,
          147, 46, 308, 271, 179, 298, 266, 301, 54, 67, 5, 178, 110, 72, 44,
          201, 296, 289, 61, 128, 132, 69, 83, 184, 28, 302, 267, 179, 51, 316,
          44, 197, 113, 246, 77, 138, 135, 52, 184, 141, 105, 117, 160, 207,
          176, 16, 260, 230, 110, 295, 50, 208, 189, 276, 319, 235, 209, 160,
          115, 283, 32, 301, 68, 267, 269, 177, 258, 228, 103, 234, 33, 279,
          245, 43, 300, 10, 281, 66, 285, 260, 280, 37, 115, 154, 135, 217, 126,
          51, 29, 162, 102, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 223, 141, 207, 201, 170, 164, 158, 17, 33, 9, 52, 142, 81, 14,
          90, 56, 8, 105, 53, 49, 107, 146, 34, 192, 82, 222, 123, 67, 114, 84,
          16, 203, 18, 10, 59, 76, 95, 37, 82, 11, 175, 120, 154, 199, 6, 192,
          88, 1, 208, 150, 175, 114, 192, 81, 12, 23, 206, 184, 94, 45, 31, 165,
          203, 176, 49, 91, 151, 165, 86, 4, 56, 164, 210, 157, 77, 215, 46,
          176, 5, 104, 174, 64, 212, 153, 91, 74, 119, 180, 131, 10, 186, 45,
          119, 166, 141, 173, 121, 157, 95, 142, 100, 19, 72, 103, 65, 49, 186,
          49, 4, 133, 153, 16, 150, 194, 198, 174, 45, 0, 87, 177, 121, 90, 60,
          9, 205, 70, 34, 113, 213, 136, 131, 8, 168, 130, 73, 29, 216, 206,
          155, 21, 105, 3, 43, 214, 193, 161, 21, 93, 213, 80, 217, 2, 175, 110,
          0, 209, 26, 10, 215, 77, 147, 63, 89, 28, 209, 200, 3, 210, 195, 79,
          214, 69, 158, 103, 110, 81, 11, 58, 157, 14, 9, 77, 211, 127, 68, 0,
          70, 118, 96, 182, 204, 35, 191, 200, 220, 22, 194, 198, 74, 51, 185,
          51, 63, 148, 150, 96, 141, 117, 99, 136, 109, 182, 209, 143, 122, 53,
          6, 167, 134, 198, 31, 223, 39, 186, 217, 114, 104, 90, 30, 116, 218,
          217, 189, 50, 81, 187, 158, 130, 58, 32, 43, 4, 106, 114, 137, 153,
          44, 84, 101, 183, 124, 177, 187, 12, 201, 78, 138, 201, 123, 30, 78,
          81, 68, 128, 161, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 211, 179, 258, 187, 246, 261, 80, 169, 54, 162, 55, 225, 231, 0,
          170, 153, 0, 265, 72, 236, 0, 275, 0, 0, 197, 216, 190, 216, 27, 18,
          198, 167, 145, 235, 181, 189, 0, 159, 178, 23, 162, 0, 270, 161, 0,
          64, 180, 205, 45, 0, 144, 0, 199, 72, 153, 0, 52, 168, 188, 162, 220,
          1, 0, 0, 0, 186, 223, 166, 72, 252, 0, 13, 81, 199, 0, 0, 256, 133,
          108, 154, 28, 0, 0, 0, 0, 219, 160, 243, 46, 4, 202, 82, 144, 0, 36,
          278, 0, 76, 218, 132, 184, 267, 216, 118, 0, 144, 0, 165, 0, 202, 63,
          197, 104, 234, 144, 72, 144, 29, 117, 0, 90, 0, 0, 0, 144, 3, 41, 169,
          93, 244, 183, 0, 0, 90, 0, 144, 116, 109, 162, 90, 134, 151, 99, 144,
          266, 76, 216, 15, 57, 283, 41, 274, 186, 151, 189, 211, 0, 0, 115, 74,
          36, 59, 45, 238, 108, 0, 72, 252, 144, 72, 229, 115, 164, 201, 137,
          183, 180, 36, 16, 233, 152, 0, 253, 277, 108, 0, 144, 158, 242, 254,
          98, 181, 16, 0, 266, 243, 147, 216, 165, 177, 132, 0, 0, 257, 0, 2,
          95, 269, 0, 151, 76, 243, 68, 241, 165, 0, 0, 183, 0, 216, 156, 200,
          46, 144, 155, 117, 73, 234, 113, 108, 209, 47, 54, 9, 261, 119, 108,
          79, 15, 273, 79, 162, 18, 62, 18, 144, 230, 39, 0, 228, 0, 166, 205,
          57, 26, 36, 0, 54, 173, 162, 35, 64, 42, 19, 270, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 294, 77, 226, 97, 42, 219, 294, 3,
          348, 156, 25, 123, 311, 22, 176, 110, 87, 89, 280, 38, 325, 102, 171,
          351, 60, 135, 164, 304, 288, 79, 118, 35, 94, 256, 130, 103, 75, 88,
          175, 322, 217, 348, 190, 47, 110, 162, 18, 279, 91, 273, 101, 56, 100,
          319, 236, 123, 210, 82, 167, 225, 213, 253, 6, 326, 304, 330, 96, 49,
          251, 22, 251, 293, 65, 170, 264, 77, 338, 302, 279, 224, 27, 249, 226,
          1, 83, 207, 268, 111, 264, 165, 265, 139, 73, 255, 326, 210, 67, 112,
          128, 166, 297, 231, 265, 147, 111, 297, 320, 37, 244, 302, 237, 91,
          215, 49, 258, 268, 237, 243, 50, 294, 155, 196, 25, 293, 250, 127,
          106, 330, 293, 142, 15, 204, 181, 16, 209, 235, 1, 167, 246, 99, 111,
          110, 332, 244, 288, 141, 339, 253, 345, 322, 312, 200, 322, 286, 157,
          265, 195, 246, 201, 110, 269, 172, 92, 176, 81, 246, 236, 95, 236,
          334, 286, 185, 39, 267, 228, 54, 104, 338, 15, 53, 242, 125, 277, 99,
          112, 35, 347, 257, 249, 295, 224, 351, 156, 247, 346, 3, 133, 131,
          215, 150, 152, 66, 116, 113, 54, 135, 304, 163, 143, 284, 23, 76, 176,
          181, 304, 243, 188, 272, 53, 167, 9, 210, 17, 73, 40, 272, 47, 7, 121,
          305, 337, 94, 331, 143, 188, 302, 121, 258, 59, 166, 78, 190, 109,
          167, 101, 283, 314, 338, 114, 30, 194, 279, 268, 272, 192, 81, 85,
          244, 212, 83, 17, 113, 107, 222, 167, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 22, 132, 4, 24, 185, 6, 145, 172, 6, 184,
          6, 52, 1, 173, 91, 12, 6, 44, 9, 21, 4, 2, 6, 4, 6, 91, 10, 163, 4, 0,
          37, 6, 204, 100, 88, 2, 10, 126, 194, 20, 6, 88, 1, 6, 6, 45, 4, 98,
          92, 4, 10, 4, 82, 4, 2, 1, 181, 0, 11, 21, 77, 18, 142, 30, 0, 124,
          33, 24, 131, 147, 198, 12, 125, 28, 75, 0, 180, 113, 112, 156, 191,
          192, 1, 10, 0, 10, 4, 86, 0, 149, 49, 27, 74, 140, 45, 45, 0, 48, 21,
          153, 16, 1, 166, 16, 49, 153, 109, 6, 0, 38, 6, 159, 12, 184, 22, 84,
          0, 2, 122, 15, 64, 142, 142, 0, 195, 151, 163, 145, 203, 81, 191, 132,
          88, 198, 153, 0, 16, 83, 50, 53, 153, 160, 5, 28, 6, 0, 60, 154, 65,
          141, 123, 202, 41, 58, 126, 168, 100, 0, 155, 87, 48, 201, 104, 182,
          167, 130, 112, 2, 0, 28, 5, 76, 70, 184, 28, 136, 63, 200, 0, 6, 85,
          118, 197, 197, 177, 0, 30, 92, 207, 96, 16, 130, 30, 96, 163, 0, 47,
          173, 24, 4, 1, 3, 131, 155, 12, 0, 179, 120, 38, 164, 207, 53, 68,
          100, 106, 43, 128, 99, 0, 168, 42, 99, 148, 40, 188, 0, 66, 2, 91,
          173, 16, 122, 42, 0, 31, 142, 161, 101, 104, 2, 155, 199, 93, 92, 72,
          35, 109, 182, 30, 6, 125, 185, 3, 124, 46, 135, 116, 20, 20, 162, 46,
          6, 194, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          135, 96, 189, 128, 64, 2, 199, 77, 181, 169, 45, 186, 220, 124, 39,
          183, 179, 77, 25, 32, 163, 32, 170, 103, 161, 11, 121, 4, 222, 191,
          227, 4, 23, 211, 171, 146, 105, 12, 116, 115, 215, 138, 78, 183, 108,
          197, 185, 27, 165, 232, 73, 199, 231, 59, 115, 53, 22, 177, 126, 236,
          225, 151, 127, 131, 132, 134, 136, 162, 47, 141, 185, 152, 187, 178,
          188, 189, 221, 151, 220, 209, 70, 2, 169, 43, 170, 84, 128, 236, 122,
          83, 117, 43, 22, 141, 232, 174, 114, 92, 179, 186, 32, 230, 154, 159,
          11, 149, 237, 168, 211, 172, 92, 96, 166, 115, 138, 9, 103, 53, 56,
          24, 203, 90, 215, 187, 225, 68, 1, 23, 206, 124, 220, 196, 117, 199,
          26, 93, 205, 11, 216, 100, 221, 180, 84, 167, 156, 172, 128, 189, 22,
          143, 223, 134, 144, 215, 47, 110, 145, 140, 75, 6, 24, 92, 17, 98,
          173, 180, 126, 26, 204, 95, 101, 167, 18, 84, 112, 181, 32, 151, 207,
          135, 85, 33, 182, 98, 142, 42, 217, 153, 200, 210, 177, 1, 95, 106,
          215, 127, 87, 96, 32, 207, 212, 72, 219, 178, 180, 30, 220, 125, 235,
          52, 92, 80, 142, 143, 199, 118, 100, 24, 49, 90, 163, 67, 145, 14,
          200, 52, 105, 230, 172, 13, 205, 178, 5, 65, 137, 216, 219, 180, 199,
          152, 76, 129, 216, 205, 65, 180, 153, 165, 205, 130, 58, 175, 100,
          148, 107, 150, 161, 38, 105, 211, 71, 49, 103, 108, 125, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Table generated for BG2 from H Matrix in Table 5.3.2-3 in 38.212 */
/* Number of non-null elements per columns in BG2 */
static constexpr int16_t kBg2MatrixNumPerCol[BG2_COL_TOTAL] = { 22, 23, 10, 5, 5,
    14, 7, 13, 6, 8, 6, 14, 7, 10, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* Table generated for BG2 from H Matrix in Table 5.3.2-3 in 38.212 */
/* Addresses of each non-null elements in matrix (removed the 4x4 parity non identify matrix) */
static constexpr int16_t kBg2Address[BG2_NONZERO_NUM] = { 0, 64, 128, 256, 320,
    384, 512, 640, 704, 832, 960, 1152, 1216, 1344, 1472, 1600, 1728, 1856,
    2048, 2176, 2304, 2496, 0, 128, 192, 256, 320, 448, 512, 576, 640, 768, 832,
    896, 1024, 1088, 1216, 1280, 1408, 1536, 1792, 1984, 2240, 2432, 2624, 0,
    192, 1408, 1536, 1664, 1792, 1920, 2112, 2304, 2560, 0, 64, 128, 768, 1472,
    64, 128, 192, 1280, 1856, 64, 192, 320, 384, 448, 1088, 1472, 1600, 1792,
    1920, 2048, 2240, 2432, 2624, 0, 64, 192, 640, 896, 1152, 1728, 64, 192,
    320, 384, 448, 640, 704, 1152, 1664, 1920, 2112, 2304, 2496, 64, 128, 192,
    576, 832, 1344, 0, 64, 192, 384, 704, 1024, 1536, 1920, 576, 960, 1216,
    2112, 2368, 2560, 256, 320, 384, 448, 576, 768, 896, 960, 1024, 1088, 1280,
    2240, 2432, 2624, 512, 1024, 1088, 1664, 2048, 2176, 2496, 448, 704, 832,
    896, 1344, 1664, 1984, 2176, 2368, 2560, 256, 320, 384, 448, 512, 576, 640,
    704, 768, 832, 896, 960, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472,
    1536, 1600, 1664, 1728, 1792, 1856, 1920, 1984, 2048, 2112, 2176, 2240,
    2304, 2368, 2432, 2496, 2560, 2624 };

/* Table generated for BG2 from H Matrix in Table 5.3.2-3 in 38.212 */
__attribute__((aligned(
    64))) static constexpr int16_t kBg2HShiftMatrix[BG2_NONZERO_NUM * I_LS_NUM]
    = { 9, 167, 81, 179, 231, 155, 142, 11, 11, 83, 51, 220, 87, 76, 23, 228, 8,
          18, 242, 147, 140, 239, 117, 114, 8, 214, 41, 129, 94, 203, 185, 63,
          2, 115, 203, 254, 20, 26, 222, 46, 98, 106, 57, 31, 129, 204, 58, 63,
          139, 29, 101, 71, 132, 38, 0, 26, 166, 44, 111, 235, 253, 52, 158,
          105, 28, 125, 104, 194, 228, 147, 124, 238, 156, 135, 240, 44, 40, 66,
          229, 189, 226, 209, 0, 145, 194, 151, 156, 54, 159, 45, 140, 117, 236,
          50, 143, 9, 164, 154, 172, 224, 240, 18, 205, 38, 42, 205, 252, 128,
          28, 210, 142, 8, 84, 61, 175, 185, 235, 219, 75, 71, 103, 158, 3, 247,
          14, 3, 213, 8, 114, 29, 63, 38, 118, 230, 242, 64, 160, 166, 85, 34,
          116, 56, 222, 232, 210, 122, 1, 36, 151, 120, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 174, 27, 25, 72, 10, 129, 118, 59, 32, 49, 68,
          186, 58, 157, 106, 45, 103, 110, 84, 173, 25, 93, 97, 114, 136, 74,
          44, 80, 70, 28, 104, 39, 125, 19, 87, 158, 42, 76, 20, 182, 70, 3, 77,
          84, 147, 166, 175, 52, 153, 67, 111, 120, 165, 151, 103, 66, 36, 117,
          93, 86, 48, 110, 113, 61, 17, 92, 72, 121, 92, 186, 23, 95, 21, 168,
          154, 8, 184, 151, 7, 71, 31, 123, 22, 118, 6, 50, 187, 118, 80, 100,
          16, 52, 92, 46, 137, 52, 179, 170, 132, 185, 114, 28, 132, 35, 175,
          172, 3, 186, 49, 174, 177, 64, 56, 185, 63, 156, 124, 37, 107, 29, 48,
          184, 102, 178, 11, 21, 81, 135, 9, 153, 18, 190, 60, 152, 64, 6, 55,
          17, 177, 57, 143, 154, 166, 163, 67, 85, 170, 12, 31, 163, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 137, 20, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 94, 38, 136, 131, 0, 65, 0, 17, 0,
          112, 0, 0, 0, 158, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 4, 69, 0, 126, 0, 0,
          63, 0, 0, 124, 99, 113, 75, 0, 9, 102, 148, 154, 0, 146, 142, 124, 45,
          24, 158, 65, 110, 35, 20, 157, 93, 2, 0, 88, 12, 156, 138, 18, 32, 0,
          57, 141, 99, 148, 20, 7, 86, 100, 51, 88, 82, 24, 0, 108, 53, 97, 102,
          17, 0, 55, 46, 45, 4, 79, 87, 134, 51, 73, 154, 13, 0, 36, 157, 64,
          148, 96, 85, 48, 57, 99, 111, 109, 104, 6, 19, 55, 87, 143, 18, 13,
          122, 19, 138, 78, 2, 26, 27, 33, 7, 20, 78, 144, 143, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 53, 152, 200, 185, 123, 147, 174,
          99, 37, 116, 68, 35, 80, 156, 211, 27, 108, 108, 29, 1, 106, 110, 131,
          185, 16, 138, 103, 43, 2, 150, 46, 113, 36, 75, 48, 138, 6, 49, 153,
          216, 147, 91, 37, 120, 23, 6, 1, 88, 90, 212, 106, 71, 175, 98, 181,
          156, 46, 217, 54, 115, 191, 36, 20, 61, 156, 124, 170, 55, 13, 132,
          134, 94, 193, 44, 21, 165, 97, 101, 95, 115, 124, 8, 95, 16, 118, 200,
          110, 219, 31, 105, 56, 138, 156, 6, 185, 12, 83, 181, 29, 91, 156, 30,
          143, 43, 8, 31, 133, 222, 110, 158, 63, 176, 184, 200, 86, 109, 40,
          35, 101, 193, 209, 150, 83, 109, 40, 110, 134, 206, 141, 55, 46, 81,
          152, 97, 2, 221, 110, 201, 154, 181, 99, 140, 116, 81, 6, 182, 69, 12,
          36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 19, 95, 42, 40,
          109, 70, 46, 28, 76, 139, 17, 79, 91, 68, 128, 13, 133, 32, 37, 121,
          119, 26, 106, 120, 24, 140, 97, 69, 97, 41, 33, 37, 143, 48, 120, 28,
          2, 54, 30, 106, 80, 60, 1, 48, 53, 121, 132, 42, 142, 77, 87, 135,
          129, 6, 35, 94, 92, 122, 115, 104, 110, 22, 103, 25, 66, 4, 84, 87,
          135, 43, 56, 63, 43, 56, 89, 137, 70, 47, 115, 84, 73, 101, 51, 106,
          10, 98, 49, 137, 107, 35, 96, 30, 142, 28, 104, 6, 26, 32, 69, 111,
          128, 40, 62, 75, 127, 50, 79, 133, 116, 9, 101, 70, 24, 96, 41, 2, 97,
          73, 51, 71, 139, 108, 49, 131, 130, 128, 28, 65, 78, 93, 1, 19, 88, 8,
          42, 100, 71, 25, 142, 65, 64, 47, 97, 81, 133, 139, 114, 56, 102, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 156, 17, 98, 86, 79, 47, 53,
          111, 91, 29, 137, 173, 13, 156, 110, 72, 42, 139, 116, 11, 73, 109,
          143, 168, 53, 67, 84, 48, 31, 104, 25, 122, 91, 11, 78, 134, 135, 128,
          18, 113, 64, 117, 126, 112, 132, 14, 174, 163, 108, 36, 24, 84, 105,
          154, 160, 3, 65, 107, 11, 132, 63, 82, 174, 52, 161, 1, 127, 35, 154,
          125, 23, 150, 136, 149, 173, 73, 152, 7, 6, 40, 55, 17, 174, 145, 31,
          104, 37, 89, 103, 10, 24, 23, 175, 22, 38, 93, 137, 129, 6, 171, 142,
          17, 142, 27, 166, 123, 133, 105, 155, 24, 158, 61, 29, 99, 103, 145,
          29, 167, 156, 83, 60, 29, 47, 64, 4, 8, 40, 17, 62, 173, 172, 19, 8,
          161, 165, 163, 53, 142, 41, 105, 55, 141, 127, 166, 40, 145, 148, 162,
          38, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 143, 18, 126, 43,
          136, 7, 101, 125, 39, 32, 174, 129, 110, 10, 52, 197, 168, 50, 110,
          197, 197, 181, 19, 163, 36, 27, 49, 163, 177, 186, 60, 18, 53, 91,
          125, 57, 124, 196, 202, 113, 14, 115, 157, 157, 191, 176, 48, 126,
          161, 164, 186, 70, 163, 167, 193, 165, 27, 47, 155, 170, 3, 183, 18,
          35, 27, 102, 111, 36, 34, 78, 201, 13, 194, 46, 17, 0, 167, 173, 197,
          196, 185, 203, 177, 20, 203, 193, 17, 3, 132, 198, 143, 51, 29, 140,
          172, 50, 173, 179, 157, 14, 132, 191, 27, 95, 122, 13, 180, 160, 168,
          35, 31, 88, 6, 205, 108, 52, 179, 181, 163, 117, 62, 12, 107, 81, 49,
          52, 102, 54, 142, 114, 181, 191, 167, 22, 176, 35, 49, 163, 191, 173,
          58, 8, 186, 109, 23, 161, 189, 193, 193, 179, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 145, 142, 74, 29, 121, 137, 176, 38, 178, 48, 38,
          128, 39, 238, 5, 66, 64, 25, 179, 184, 178, 167, 131, 31, 239, 140,
          41, 86, 169, 167, 217, 124, 57, 82, 170, 196, 84, 117, 195, 81, 7,
          201, 85, 42, 53, 71, 171, 44, 19, 146, 144, 37, 46, 112, 78, 21, 174,
          3, 122, 94, 183, 53, 95, 227, 57, 27, 110, 169, 72, 186, 173, 111, 95,
          16, 139, 14, 225, 41, 215, 23, 96, 159, 208, 232, 211, 181, 23, 199,
          88, 172, 87, 232, 214, 210, 66, 221, 2, 106, 45, 9, 155, 43, 238, 167,
          13, 112, 167, 75, 124, 168, 23, 130, 17, 48, 217, 88, 106, 154, 67,
          180, 207, 56, 172, 68, 72, 204, 157, 175, 195, 6, 175, 105, 230, 225,
          202, 218, 190, 116, 135, 189, 154, 51, 219, 162, 11, 86, 46, 141, 114,
          180, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#ifdef __cplusplus
}
#endif

#endif
