/****************************************************************************
*  Copyright 2023 Gorgon Meducer (Email:embedded_zhuoran@hotmail.com)       *
*                                                                           *
*  Licensed under the Apache License, Version 2.0 (the "License");          *
*  you may not use this file except in compliance with the License.         *
*  You may obtain a copy of the License at                                  *
*                                                                           *
*     http://www.apache.org/licenses/LICENSE-2.0                            *
*                                                                           *
*  Unless required by applicable law or agreed to in writing, software      *
*  distributed under the License is distributed on an "AS IS" BASIS,        *
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
*  See the License for the specific language governing permissions and      *
*  limitations under the License.                                           *
*                                                                           *
****************************************************************************/

/*============================ INCLUDES ======================================*/
#ifndef __LMSK_ENCODER_H__
#define __LMSK_ENCODER_H__   1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../__lmsk_common.h"

#ifdef   __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/

typedef struct __arm_lmsk_line_out_t {
    struct __arm_lmsk_line_out_t *ptNext;
    uint8_t *pchBuffer;
    size_t tSize;
    uint32_t wPosition;
} __arm_lmsk_line_out_t;

typedef struct __arm_lmsk_line_in_t {
    struct __arm_lmsk_line_in_t *ptNext;
    uint8_t *pchBuffer;
} __arm_lmsk_line_in_t;

typedef struct __arm_lmsk_output_t {
    arm_lmsk_header_t tHeader;

    __arm_lmsk_line_out_t *ptLines;
    uint32_t wDataSize;

    struct {
        uint32_t *pwReferences;
    } tLineIndexTable;

} __arm_lmsk_output_t;

typedef struct arm_lmsk_encoder_t {
    __arm_lmsk_output_t tOutput;

    struct {
        uint8_t *pchBuffer;
        int16_t iWidth;
        int16_t iHeight;
    } Mask;

} arm_lmsk_encoder_t;


/*============================ GLOBAL VARIABLES ==============================*/
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/

extern
arm_lmsk_encoder_t * arm_lmsk_encoder_init( arm_lmsk_encoder_t *ptThis, 
                                            uint8_t *pchMaskBuffer, 
                                            int16_t iWidth, 
                                            int16_t iHeight);

extern
__arm_lmsk_output_t *arm_lmsk_encode(   arm_lmsk_encoder_t *ptThis, 
                                        uint8_t chAlphaBits);

extern 
arm_lmsk_encoder_t * arm_lmsk_encoder_depose(arm_lmsk_encoder_t *ptThis);

#ifdef   __cplusplus
}
#endif

#endif

