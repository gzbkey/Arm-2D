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

typedef struct __arm_lmsk_algorithm_t __arm_lmsk_algorithm_t;

typedef struct __arm_lmsk_encode_result_t {
    uint16_t bHit       : 1;
    uint16_t u15Size    : 15;
    int16_t hwRawSize;
    uint8_t chNewPrevious;
    uint8_t *pchEncode;
    __arm_lmsk_algorithm_t *ptAlgorithm;
} __arm_lmsk_encode_result_t;

struct __arm_lmsk_algorithm_t {

    __arm_lmsk_encode_result_t (* const fnTry)( uint8_t *pchSource, 
                                                size_t tSizeLeft, 
                                                uint8_t chPrevious,
                                                uint8_t chAlphaMSBBits);

    uint32_t wPixelCover;
    const bool bCheckPalette;
    const char * const pchName;
};

enum {
    TAG_U2_INDEX        = 0x0,
    TAG_U2_REPETA       = 0x1,
    TAG_S2_DELTA_SMALL  = -2,   /*! 0b10 */
    TAG_S2_DELTA_LARGE  = -1,   /*! 0b11 */

    TAG_U8_ALPHA        = 0xFD,
    TAG_U8_GRADIENT     = 0xF9,
};

enum {
    LMSK_TAG_DELTA_SMALL,
    LMSK_TAG_DELTA_LARGE,
    LMSK_TAG_REPEAT_PREVIOUS,
    LMSK_TAG_ALPHA,
    LMSK_TAG_INDEX,
};


/*============================ PROTOTYPES ====================================*/

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_alpha_tag(uint8_t *pchSource, 
                                                    size_t tSizeLeft, 
                                                    uint8_t chPrevious,
                                                    uint8_t chAlphaMSBBits);

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_delta_large_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits);

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_delta_small_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits);

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_repeat_prev_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits);

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/

static
__arm_lmsk_algorithm_t c_tAlgorithms[] = {
    [LMSK_TAG_DELTA_SMALL] = {
        .fnTry = &__arm_lmsk_try_delta_small_tag,
        .pchName = "LMSK_TAG_DELTA_SMALL",
    },

    [LMSK_TAG_DELTA_LARGE] = {
        .fnTry = &__arm_lmsk_try_delta_large_tag,
        .pchName = "LMSK_TAG_DELTA_LARGE",
    },
    [LMSK_TAG_REPEAT_PREVIOUS] = {
        .fnTry = &__arm_lmsk_try_repeat_prev_tag,
        .pchName = "LMSK_TAG_REPEAT_PREVIOUS",
    },
    [LMSK_TAG_ALPHA] = {
        .fnTry = &__arm_lmsk_try_alpha_tag,
        .pchName = "LMSK_TAG_ALPHA",
        .bCheckPalette = true,
    },
    [LMSK_TAG_INDEX] = {
        .fnTry = NULL,
        .pchName = "LMSK_TAG_INDEX",
    },
};

static 
uint32_t s_wTotalPixels = 0;


/*============================ IMPLEMENTATION ================================*/

void __arm_lmsk_encoder_prepare(arm_lmsk_encoder_t *ptThis)
{
    for (uint_fast8_t n = 0; n < dimof(c_tAlgorithms); n++) {
        c_tAlgorithms[n].wPixelCover = 0;
    }
    s_wTotalPixels = 0;
}

void __arm_lmsk_encoder_report(arm_lmsk_encoder_t *ptThis)
{

    uint32_t wTotalPixels = s_wTotalPixels;

    printf("\r\n-[Report]------------------------------ \r\n");

    for (uint_fast8_t n = 0; n < dimof(c_tAlgorithms); n++) {
    
        double dfRatio = (double)c_tAlgorithms[n].wPixelCover / (double) wTotalPixels;
        dfRatio *= 100.0f;

        printf( "Algorithm: %s \r\n", c_tAlgorithms[n].pchName);
        printf( "Pixel Hits: %"PRIu32" [%f2.4%%]\r\n\r\n", 
                c_tAlgorithms[n].wPixelCover,
                dfRatio);
    }

    printf("--------------------------------------- \r\n\r\n");
}

static inline 
__arm_lmsk_encode_result_t __arm_lmsk_encode_use_index(__arm_lmsk_encode_result_t tOld, uint8_t chIndex)
{
    __arm_lmsk_encode_result_t tResult = {
        .pchEncode = (uint8_t *)malloc(1),
        .bHit = true,
        .hwRawSize = 1,
        .u15Size = 1,
        .chNewPrevious = tOld.chNewPrevious,
        .ptAlgorithm = &c_tAlgorithms[LMSK_TAG_INDEX],
    };

    assert(NULL != tResult.pchEncode);

    tResult.pchEncode[0] = (arm_lmsk_tag_index_t) {
        .u2Tag = TAG_U2_INDEX,
        .u6Index = chIndex,
    }.chByte;

    if (NULL != tOld.pchEncode) {
        /* free the old result */
        free(tOld.pchEncode);
    }

    return tResult;
}

void __arm_lmsk_encode_line(arm_lmsk_encoder_t *ptThis,
                            __arm_lmsk_line_out_t *ptLine,
                            uint8_t chAlphaMSBBits)
{
    assert(NULL != ptThis);
    assert(NULL != ptLine);

    int16_t iWidth = this.Mask.iWidth;

    ptLine->pchBuffer = (uint8_t *)malloc(iWidth * 2); /* for the worst case*/
    assert(NULL != ptLine->pchBuffer);

    ptLine->pchBuffer[0] = ptLine->pchSourceLine[0];    /* the first pixel */

    uint8_t chPrevious = ptLine->pchBuffer[0]; 
    int16_t iSizeLeft = iWidth - 1;
    uint8_t *pchSource = ptLine->pchSourceLine;
    uint8_t *pchTarget = ptLine->pchBuffer;
    size_t tEncodedSize = 1;
    do {
        __arm_lmsk_encode_result_t tBestResult = {0};

        float fCompressionRate = 0.0f;

        /* try all algorithm */
        for (uint_fast8_t n = 0; n < dimof(c_tAlgorithms); n++) {

            if (NULL == c_tAlgorithms[n].fnTry) {
                continue;
            }

            __arm_lmsk_encode_result_t tResult 
                = c_tAlgorithms[n].fnTry(pchSource, iSizeLeft, chPrevious, chAlphaMSBBits);

            if (tResult.bHit) {
                float fRate = (float)tResult.hwRawSize / (float)tResult.u15Size;
                if (fRate > fCompressionRate) {
                    if (NULL != tBestResult.pchEncode) {
                        /* free the old result */
                        free(tBestResult.pchEncode);
                    }

                    fCompressionRate = fRate;
                    
                    tBestResult = tResult;
                    continue;
                }
            }

            if (NULL != tResult.pchEncode) {
                free(tResult.pchEncode);
            }
        }

        assert(tBestResult.bHit);

        if ((tBestResult.ptAlgorithm->bCheckPalette)
        &&  (1 == tBestResult.hwRawSize)) {
            int_fast8_t chIndex = 0;
            bool bFindIndex = false;
            for (;chIndex < 64; chIndex++) {
                if (this.tOutput.chPalette[chIndex] == tBestResult.chNewPrevious) {

                    tBestResult = __arm_lmsk_encode_use_index(tBestResult, chIndex);
                    bFindIndex = true;
                    break;
                }

                if (0 == this.tOutput.chPalette[chIndex] && chIndex > 0) {
                    break;
                }
            }

            if (!bFindIndex && chIndex < 64) {
                this.tOutput.chPalette[chIndex] = tBestResult.chNewPrevious;
                tBestResult = __arm_lmsk_encode_use_index(tBestResult, chIndex);
            }
        }

        /* get the best encoding decision */

        do {
            size_t tTagSize = tBestResult.u15Size;
            size_t tRawSize = tBestResult.hwRawSize;
            memcpy(pchTarget, tBestResult.pchEncode, tTagSize);
            pchTarget += tTagSize;
            pchSource += tRawSize;
            tEncodedSize += tTagSize;

            assert(iSizeLeft >= tRawSize);
            iSizeLeft -= tRawSize;

            chPrevious = tBestResult.chNewPrevious;

            if (NULL != tBestResult.ptAlgorithm) {
                tBestResult.ptAlgorithm->wPixelCover += tRawSize;
            }

            if (NULL != tBestResult.pchEncode) {
                /* free the old result */
                free(tBestResult.pchEncode);
            }

        } while(0);

    } while(iSizeLeft);

    ptLine->tSize = tEncodedSize;

    s_wTotalPixels += iWidth;
}


static 
__arm_lmsk_encode_result_t __arm_lmsk_try_alpha_tag(uint8_t *pchSource, 
                                                    size_t tSizeLeft, 
                                                    uint8_t chPrevious,
                                                    uint8_t chAlphaMSBBits)
{
    __arm_lmsk_encode_result_t tResult = {
        .pchEncode = (uint8_t *)malloc(2),
        .bHit = true,
        .hwRawSize = 1,
        .u15Size = 2,
        .ptAlgorithm = &c_tAlgorithms[LMSK_TAG_ALPHA],
    };

    assert(NULL != tResult.pchEncode);

    tResult.pchEncode[0] = TAG_U8_ALPHA;
    tResult.pchEncode[1] = *pchSource;

    tResult.chNewPrevious = *pchSource;

    return tResult;

}

static inline
int8_t __arm_lmsk_get_delta(uint16_t hwPrevious, 
                            uint16_t hwCurrent,
                            uint8_t chAlphaMSBBits)
{

    uint8_t chBitsToShift = 8 - chAlphaMSBBits;
    hwPrevious >>= chBitsToShift;
    hwCurrent >>= chBitsToShift;

    int16_t iDelta0 = (int16_t)hwCurrent - (int16_t)hwPrevious;
    uint16_t chCompenstation = 0x100 >> chBitsToShift;

    if (iDelta0 < 0) {
        int16_t iDelta1 = (int16_t)(hwCurrent + chCompenstation) - (int16_t)hwPrevious;

        if (ABS(iDelta0) < ABS(iDelta1)) {
            return iDelta0;
        } 

        return iDelta1;

    } else if (iDelta0 > 0) {
        int16_t iDelta1 = (int16_t)(hwPrevious + chCompenstation) - (int16_t)hwCurrent;

        if (ABS(iDelta0) < ABS(iDelta1)) {
            return iDelta0;
        } 

        return -iDelta1;

    }

    return iDelta0;
}

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_delta_large_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits)
{
    __arm_lmsk_encode_result_t tResult = {
        .pchEncode = (uint8_t *)malloc(1),
        .bHit = false,
        .hwRawSize = 1,
        .u15Size = 1,
        .ptAlgorithm = &c_tAlgorithms[LMSK_TAG_DELTA_LARGE],
    };

    assert(NULL != tResult.pchEncode);

    uint8_t chBitsToShift = 8 - chAlphaMSBBits;
    int16_t iDelta = __arm_lmsk_get_delta(  chPrevious, 
                                            pchSource[0], 
                                            chAlphaMSBBits);

    if (iDelta >= -32 && iDelta <= 31) {
        tResult.bHit = true;

        tResult.pchEncode[0] = ((arm_lmsk_tag_delta_large_t){
            .s2Tag = TAG_S2_DELTA_LARGE,
            .s6Delta = iDelta,
        }).chByte;

        tResult.chNewPrevious = (*pchSource >> chBitsToShift) << chBitsToShift;
    }

    return tResult;

}

static 
__arm_lmsk_encode_result_t __arm_lmsk_try_delta_small_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits)
{
    __arm_lmsk_encode_result_t tResult = {
        .pchEncode = (uint8_t *)malloc(1),
        .bHit = false,
        .hwRawSize = 2,
        .u15Size = 1,
        .ptAlgorithm = &c_tAlgorithms[LMSK_TAG_DELTA_SMALL],
    };

    assert(NULL != tResult.pchEncode);


    uint8_t chBitsToShift = 8 - chAlphaMSBBits;
    do {
        if (tSizeLeft < 2) {
            break;
        }

        arm_lmsk_tag_delta_small_t tTagDeltaSmall = {
            .s2Tag = TAG_S2_DELTA_SMALL,
        };

        int16_t iDelta = __arm_lmsk_get_delta(  chPrevious, 
                                                pchSource[0], 
                                                chAlphaMSBBits);

        if (iDelta >= -4 && iDelta <= 3) {
            tTagDeltaSmall.s3Delta0 = iDelta;
        } else {
            break;
        }

        iDelta = __arm_lmsk_get_delta(  pchSource[0], 
                                        pchSource[1], 
                                        chAlphaMSBBits);

        if (iDelta >= -4 && iDelta <= 3) {
            tTagDeltaSmall.s3Delta1 = iDelta;
        } else {
            break;
        }

        tResult.bHit = true;
        tResult.pchEncode[0] = tTagDeltaSmall.chByte;
        tResult.chNewPrevious = (pchSource[1] >> chBitsToShift) << chBitsToShift;

    } while(0);

    return tResult;
}


static 
__arm_lmsk_encode_result_t __arm_lmsk_try_repeat_prev_tag(  uint8_t *pchSource, 
                                                            size_t tSizeLeft, 
                                                            uint8_t chPrevious,
                                                            uint8_t chAlphaMSBBits)
{
    __arm_lmsk_encode_result_t tResult = {
        .u15Size = 1,
        .ptAlgorithm = &c_tAlgorithms[LMSK_TAG_REPEAT_PREVIOUS],
    };

    size_t tRepeatCount = 0;

    uint8_t chBitsToShift = 8 - chAlphaMSBBits;
    uint8_t chShiftedPrevious = chPrevious >> chBitsToShift;

    do {
        uint8_t chCurrent = *pchSource++ >> chBitsToShift;
        if (chCurrent != chShiftedPrevious) {
            break;
        }

        tRepeatCount++;
    } while(--tSizeLeft);

    if (tRepeatCount == 0) {
        return tResult;
    }

    if (tRepeatCount <= 61) {

        tResult.bHit = true;
        tResult.hwRawSize = tRepeatCount;
        tResult.pchEncode = (uint8_t *)malloc(1);
        assert(NULL != tResult.pchEncode);

        tResult.pchEncode[0] = ((arm_lmsk_tag_repeat_t){
            .u2Tag = TAG_U2_REPETA,
            .u6Repeat = tRepeatCount,
        }).chByte;

        tResult.chNewPrevious = chShiftedPrevious << chBitsToShift;
    } else {

        tResult.bHit = true;
        tResult.hwRawSize = tRepeatCount;
        tResult.pchEncode = (uint8_t *)malloc(4);
        tResult.u15Size = 4;
        assert(NULL != tResult.pchEncode);

        tResult.pchEncode[0] = TAG_U8_GRADIENT;

        tResult.pchEncode[1] = chPrevious;                  /* to alpha */
        *(uint16_t *)(&tResult.pchEncode[2]) = tRepeatCount;

        tResult.chNewPrevious = chShiftedPrevious << chBitsToShift;
    }

    return tResult;

}

#ifdef   __cplusplus
}
#endif


