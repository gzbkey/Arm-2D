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
static
void __arm_lmsk_free_output_lines(__arm_lmsk_output_t *ptThis);

/*============================ IMPLEMENTATION ================================*/

arm_lmsk_encoder_t * arm_lmsk_encoder_init( arm_lmsk_encoder_t *ptThis, 
                                            uint8_t *pchMaskBuffer, 
                                            int16_t iWidth, 
                                            int16_t iHeight)
{
    do {
        if (NULL == ptThis || NULL == pchMaskBuffer || iWidth <= 0 || iHeight <= 0) {
            break;
        }
        memset(ptThis, 0, sizeof(arm_lmsk_encoder_t));

        this.Mask.pchBuffer = pchMaskBuffer;
        this.Mask.iWidth = iWidth;
        this.Mask.iHeight = iHeight;

        memcpy(this.tOutput.tHeader.chName, "LMSK", 4);

        this.tOutput.tHeader.Version.chValue = ARM_LMSK_VERSION;
        this.tOutput.tHeader.iWidth = iWidth;
        this.tOutput.tHeader.iHeight = iHeight;

        /* allocate memory for the line index reference table */
        this.tOutput.tLineIndexTable.pwReferences = (uint32_t *)malloc(iHeight * sizeof(uint32_t));
        assert(NULL != this.tOutput.tLineIndexTable.pwReferences);
    } while(0);

    return NULL;
}


__arm_lmsk_output_t *arm_lmsk_encode(arm_lmsk_encoder_t *ptThis, uint8_t chAlphaBits)
{
    if (NULL == ptThis || chAlphaBits == 0 || chAlphaBits > 8) {
        return NULL;
    }

    this.tOutput.tHeader.u3AlphaMSBCount = chAlphaBits - 1;



    return &this.tOutput;
}

int arm_lmsk_write_to_file(__arm_lmsk_output_t *ptThis, FILE *ptOut)
{
    if (NULL == ptThis || NULL == ptOut) {
        return -1;
    }

    int32_t nTotalSize = 0;

    /* write header */
    do {
        if (sizeof(this.tHeader) != fwrite(&this.tHeader, 1, sizeof(this.tHeader), ptOut)) {
            return -1;
        }
        nTotalSize += sizeof(this.tHeader);
    } while(0);

    /* write floor table and index table  */
    do {

        uint16_t *phwIndexTable = (uint16_t *)malloc(this.tHeader.iHeight * sizeof(uint16_t));
        assert(NULL != phwIndexTable);

        uint32_t wFloorSize = 1 << (16 - this.tHeader.u2TagSetBits);
        uint32_t wFloorLevel = 0;
        for (int_fast16_t iY = 0; iY < this.tHeader.iHeight; iY++) {
            if (this.tLineIndexTable.pwReferences[iY] - wFloorLevel >= wFloorSize) {

                /* write a floor */
                wFloorLevel += wFloorSize;

                if (sizeof(uint16_t) != fwrite(&iY, sizeof(uint16_t), 1, ptOut)) {
                    free(phwIndexTable);
                    return -1;
                }
                nTotalSize += sizeof(uint16_t);
            }

            phwIndexTable[iY] = (uint16_t)( (uint32_t)this.tLineIndexTable.pwReferences[iY] 
                                          - (uint32_t)wFloorLevel);
        }



        free(phwIndexTable);
    } while(0);


    /* write data section */
    do {
        __arm_lmsk_line_out_t *ptLine = this.ptLines;
        while(NULL != ptLine) {

            if (ptLine->tSize != fwrite(ptLine->pchBuffer, 1, ptLine->tSize, ptOut)) {
                return -1;
            }

            nTotalSize += ptLine->tSize;

            ptLine = ptLine->ptNext;
        }
    } while(0);

    return 0;
}


arm_lmsk_encoder_t * arm_lmsk_encoder_depose(arm_lmsk_encoder_t *ptThis)
{
    if (NULL == ptThis) {
        return NULL;
    }


    /* free output lines */
    __arm_lmsk_free_output_lines(&this.tOutput);

    /* free the line index reference table */
    if (NULL != this.tOutput.tLineIndexTable.pwReferences) {
        free(this.tOutput.tLineIndexTable.pwReferences);
    }

    return ptThis;
}

static
void __arm_lmsk_free_output_lines(__arm_lmsk_output_t *ptThis)
{
    if (NULL != ptThis) {
        return ;
    }

    __arm_lmsk_line_out_t *ptLine = this.ptLines;
    while(NULL != ptLine) {

        __arm_lmsk_line_out_t *ptNext = ptLine->ptNext;

        free(ptLine);

        ptLine = ptNext;
    }

    this.ptLines = NULL;
}



#ifdef   __cplusplus
}
#endif


