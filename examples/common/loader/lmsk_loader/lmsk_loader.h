/*
 * Copyright (c) 2009-2025 Arm Limited. All rights reserved.
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

#ifndef __LMSK_LOADER_H__
#define __LMSK_LOADER_H__

/*============================ INCLUDES ======================================*/
#if defined(_RTE_)
#   include "RTE_Components.h"
#endif

#if defined(RTE_Acceleration_Arm_2D_Helper_PFB) && defined(RTE_Acceleration_Arm_2D_Extra_Loader)

#include "arm_2d_helper.h"
#include "../generic_loader/arm_2d_generic_loader.h"
#include "lmsk/decoder/arm_lmsk_decoder.h"

#ifdef   __cplusplus
extern "C" {
#endif

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wmissing-declarations"
#   pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"
#   pragma clang diagnostic ignored "-Wpadded"
#endif

/*============================ MACROS ========================================*/

/* OOC header, please DO NOT modify  */
#ifdef __LMSK_LOADER_IMPLEMENT__
#   undef   __LMSK_LOADER_IMPLEMENT__
#   define  __ARM_2D_IMPL__
#elif defined(__LMSK_LOADER_INHERIT__)
#   undef   __LMSK_LOADER_INHERIT__
#   define __ARM_2D_INHERIT__
#endif
#include "arm_2d_utils.h"
/*============================ MACROS ========================================*/

#define ARM_LMSK_GROUP_DEF(__COUNT, ...)                                        \
    struct {                                                                    \
        arm_lmsk_loader_t tLoader;                                              \
        union {                                                                 \
            arm_loader_io_file_t tFILE;                                         \
            arm_loader_io_binary_t tBINARY;                                     \
            arm_loader_io_rom_t tROM;                                           \
            arm_loader_io_cache_t tCACHE;                                       \
            __VA_ARGS__                                                         \
        } LoaderIO;                                                             \
    } LMSK[__COUNT]

#define ARM_LMSK_GROUP_ON_LOAD()                                                \
    do {arm_foreach(this.LMSK) {                                                \
        arm_lmsk_loader_on_load(&_->tLoader);                                   \
    }} while(0)

#define ARM_LMSK_GROUP_DEPOSE()                                                 \
    do {arm_foreach(this.LMSK) {                                                \
        arm_lmsk_loader_depose(&_->tLoader);                                    \
    }} while(0)

#define ARM_LMSK_GROUP_ON_FRAME_START()                                         \
    do {arm_foreach(this.LMSK) {                                                \
        arm_lmsk_loader_on_frame_start(&_->tLoader);                            \
    }} while(0)

#define ARM_LMSK_GROUP_ON_FRAME_COMPLETE()                                      \
    do {arm_foreach(this.LMSK) {                                                \
        arm_lmsk_loader_on_frame_complete(&_->tLoader);                         \
    }} while(0)

#if __ARM_LMSK_USE_LOADER_IO__
#   define __ARM_LMSK_ITEM_INIT(__IDX, __IO_NAME, __LMSK_SRC, ...)              \
    do {                                                                        \
        arm_lmsk_loader_cfg_t tCFG = {                                          \
            .ptScene = (arm_2d_scene_t *)ptThis,                                \
            .ImageIO = {                                                        \
                .ptIO = &ARM_LOADER_IO_##__IO_NAME,                             \
                .pTarget = (uintptr_t)&this.LMSK[__IDX].LoaderIO.t##__IO_NAME,  \
            },                                                                  \
            __VA_ARGS__                                                         \
        };                                                                      \
                                                                                \
        arm_lmsk_loader_init(&this.LMSK[__IDX].tLoader, &tCFG);                 \
    } while(0)
#else
#   define __ARM_LMSK_ITEM_INIT(__IDX, __IO_NAME, __LMSK_SRC, ...)              \
    do {                                                                        \
        arm_lmsk_loader_cfg_t tCFG = {                                          \
            .ptScene = (arm_2d_scene_t *)ptThis,                                \
            .pchLMSKSource = (const uint8_t *)(__LMSK_SRC),                     \
            __VA_ARGS__                                                         \
        };                                                                      \
                                                                                \
        arm_lmsk_loader_init(&this.LMSK[__IDX].tLoader, &tCFG);                 \
    } while(0)
#endif

#define ARM_LMSK_ITEM_INIT(__IDX, __IO_NAME, __LMSK_SRC, ...)                   \
            __ARM_LMSK_ITEM_INIT(  (__IDX),                                     \
                                    __IO_NAME,                                  \
                                    (__LMSK_SRC), ##__VA_ARGS__)


#define ARM_LMSK_ITEM_INIT_WITH_ROM(__IDX, __LMSK_SRC, __SIZE)                  \
        do {                                                                    \
            extern const uint8_t __LMSK_SRC[__SIZE];                            \
                                                                                \
            arm_loader_io_rom_init( &this.LMSK[__IDX].LoaderIO.tROM,            \
                                    (uintptr_t)(__LMSK_SRC),                    \
                                    sizeof(__LMSK_SRC));                        \
                                                                                \
            ARM_LMSK_ITEM_INIT((__IDX), ROM, (__LMSK_SRC));                     \
        } while(0)

#define ARM_LMSK_ITEM_INIT_WITH_BINARY(__IDX, __LMSK_SRC, __SIZE)               \
        do {                                                                    \
            extern const uint8_t __LMSK_SRC[__SIZE];                            \
                                                                                \
            arm_loader_io_binary_init(  &this.LMSK[__IDX].LoaderIO.tBINARY,     \
                                        (uint8_t *)(__LMSK_SRC),                \
                                        sizeof(__LMSK_SRC));                    \
                                                                                \
            ARM_LMSK_ITEM_INIT((__IDX), BINARY, (__LMSK_SRC));                  \
        } while(0)

#define ARM_LMSK_ITEM_INIT_WITH_FILE(__IDX, __FILE_PATH)                        \
        do {                                                                    \
            arm_loader_io_file_init(&this.LMSK[0].LoaderIO.tFILE,               \
                                    (__FILE_PATH));                             \
                                                                                \
            ARM_LMSK_ITEM_INIT((__IDX), FILE, NULL);                            \
        } while(0)

#define ARM_LMSK_ITEM(__IDX)    this.LMSK[__IDX]
#define ARM_LMSK_TILE(__IDX)    this.LMSK[__IDX].tLoader.tTile

/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/


typedef struct arm_lmsk_loader_cfg_t {

    bool bUseHeapForVRES;

#if __ARM_LMSK_USE_LOADER_IO__
    struct {
        const arm_loader_io_t *ptIO;
        uintptr_t pTarget;
    } ImageIO;
#else
    const uint8_t *pchLMSKSource;
#endif

    arm_2d_scene_t *ptScene;
} arm_lmsk_loader_cfg_t;

/*!
 * \brief the Lossless Compressed Mask Loader
 */
typedef struct arm_lmsk_loader_t arm_lmsk_loader_t;

struct arm_lmsk_loader_t {

    union {
        arm_2d_tile_t tTile;
        inherit(arm_generic_loader_t);
    };

ARM_PRIVATE(
#if !__ARM_LMSK_USE_LOADER_IO__
    const uint8_t *pchLMSKSource;
#endif

    arm_lmsk_decoder_t tDecoder;
    __arm_lmsk_floor_context_t tContext;
    
    bool bIsNewFrame;
)
    
};

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ PROTOTYPES ====================================*/

extern
ARM_NONNULL(1, 2)
arm_2d_err_t arm_lmsk_loader_init(  arm_lmsk_loader_t *ptThis,
                                    arm_lmsk_loader_cfg_t *ptCFG);
extern
ARM_NONNULL(1)
void arm_lmsk_loader_depose( arm_lmsk_loader_t *ptThis);

extern
ARM_NONNULL(1)
void arm_lmsk_loader_on_load( arm_lmsk_loader_t *ptThis);

extern
ARM_NONNULL(1)
void arm_lmsk_loader_on_frame_start( arm_lmsk_loader_t *ptThis);

extern
ARM_NONNULL(1)
void arm_lmsk_loader_on_frame_complete( arm_lmsk_loader_t *ptThis);



#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

#ifdef   __cplusplus
}
#endif

#else

#define lmsk_loader_init(...)                 ARM_2D_ERR_NOT_AVAILABLE
#define lmsk_loader_depose(...)
#define lmsk_loader_on_load(...)
#define lmsk_loader_on_frame_start(...)
#define lmsk_loader_on_frame_complete(...)

#endif 

#endif
