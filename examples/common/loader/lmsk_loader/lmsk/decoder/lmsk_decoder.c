/*
 * Copyright (c) 2009-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*============================ INCLUDES ======================================*/


#include "lmsk_decoder.h"

#ifdef   __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/
#undef this
#define this (*ptThis)

/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/


static inline 
bool arm_lmsk_decoder_seek(arm_lmsk_decoder_t *ptThis, int32_t nOffset)
{
    return this.tCFG.IO.fnSeek(this.tCFG.IO.pTarget, nOffset);
}

static inline 
size_t arm_lmsk_decoder_read(   arm_lmsk_decoder_t *ptThis,  
                                uint8_t *pchBuffer,
                                size_t tLength)
{
    return this.tCFG.IO.fnRead(this.tCFG.IO.pTarget, pchBuffer, tLength);
}

int arm_lmsk_decoder_init(  arm_lmsk_decoder_t *ptThis, 
                            arm_lmsk_decoder_cfg_t *ptCFG)
{
    assert(NULL != ptThis);
    assert(NULL != ptCFG);

    memset(ptThis, 0, sizeof(arm_lmsk_decoder_t));

    this.tCFG = *ptCFG;

    if (NULL == this.tCFG.IO.fnSeek || NULL == this.tCFG.IO.fnRead) {
        return -1;
    }

    /* read header and palette */
    do {
        arm_lmsk_decoder_seek(ptThis, 0);

        arm_lmsk_header_t tHeader;

        if (sizeof(tHeader) 
        !=  arm_lmsk_decoder_read(ptThis, &tHeader, sizeof(tHeader))) {
            return -1;
        }

        /* check signature */
        if (0 != memcmp(tHeader.chName, "LMSK", 4)) {
            return -1;
        }

        /* check version */
        if (tHeader.Version.chValue > ARM_LMSK_VERSION) {
            return -1;
        }

        /* to do */
        if (tHeader.tSetting.bRaw) {
            return -1;
        }

        /* unsupported in this version */
        if (this.tSetting.u2TagSetBits > 0) {
            return -1;
        }

        this.tSetting = tHeader.tSetting;

        if (sizeof(this.chPalette) 
        !=  arm_lmsk_decoder_read(ptThis, this.chPalette, sizeof(this.chPalette))) {
            return -1;
        }
        
    } while(0);


    return 0;
}


#ifdef   __cplusplus
}
#endif



