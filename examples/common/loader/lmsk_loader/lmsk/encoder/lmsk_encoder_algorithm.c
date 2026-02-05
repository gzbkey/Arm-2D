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
#include "lmsk_encoder.h"

#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <inttypes.h>

#ifdef   __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/
#undef this
#define this (*ptThis)


/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/

void __arm_lmsk_encode_line(arm_lmsk_encoder_t *ptThis,
                            __arm_lmsk_line_out_t *ptLine)
{
    assert(NULL != ptThis);
    assert(NULL != ptLine);

    int16_t iWidth = this.Mask.iWidth;

    ptLine->pchBuffer = (uint8_t *)malloc(iWidth * 2); /* for the worst case*/
    assert(NULL != ptLine->pchBuffer);

    /* let's add encoding algorithm later */
    do {
        memcpy(ptLine->pchBuffer, ptLine->pchSourceLine, iWidth);
        ptLine->tSize = iWidth;
    } while(0);
}

#ifdef   __cplusplus
}
#endif


