#include "zhRGB565_Decoder.h"

#if defined(RTE_Acceleration_Arm_2D_Extra_Loader)
#   define __ZHRGB565_LOADER_IMPLEMENT__
#   include "arm_2d_example_loaders.h"
#endif

#if defined(RTE_Acceleration_Arm_2D)
__STATIC_FORCEINLINE
void zhRGB565_save_pixel(COLOUR_INT *pPixel, uint16_t hwRGB565Pixel)
{
#if __GLCD_CFG_COLOUR_DEPTH__ == 16
    *pPixel = hwRGB565Pixel;
#else
    __arm_2d_color_fast_rgb_t tPixel;
    __arm_2d_rgb565_unpack(hwRGB565Pixel, &tPixel);

#    if __GLCD_CFG_COLOUR_DEPTH__ == 8
    *pPixel = __arm_2d_gray8_pack(&tPixel);
#    elif __GLCD_CFG_COLOUR_DEPTH__ == 32
    *pPixel = __arm_2d_ccca8888_pack(&tPixel);
#endif
#endif
}

/**
 * ref_color：reference color
 * encoded_num：repeat count
 * pixel：pointer to store the decoded pixel
 */
__STATIC_INLINE
void zhRGB565_RLE_decoder(uint16_t ref_color, uint16_t encoder_num, COLOUR_INT *pixel)
{
    for(uint32_t i=0;i<encoder_num; i++){
        zhRGB565_save_pixel(pixel++, ref_color);
    }
}

#else

/**
 * ref_color：reference color
 * encoded_num：repeat count
 * pixel：pointer to store the decoded pixel
 */
static inline void zhRGB565_RLE_decoder(uint16_t ref_color, uint16_t encoder_num, uint16_t *pixel)
{
    for(uint32_t i=0;i<encoder_num; i++){
        *(pixel) = ref_color;
        pixel++;
    }
}

#endif

/**
 * ref_color：reference color
 * encoderDATA： encoded data
 * pixel1,pixel2：decoded pixels
 */
static inline void zhRGB565_DIFF_decoder(uint16_t ref_color, uint16_t encoderDATA, uint16_t *pixel1, uint16_t *pixel2)
{
    uint8_t tmpdataH = encoderDATA>>8, tmpdataL = encoderDATA;
    uint16_t HI = GET_RGB332_TO_RGB565(tmpdataH);
    uint16_t LO = GET_RGB332_TO_RGB565(tmpdataL);
    
    *pixel1 = ref_color ^ HI;
    *pixel2 = *pixel1 ^ LO;
}

#if !defined(RTE_Acceleration_Arm_2D_Extra_Loader)
// x0,y0:(x0,y0) top-left coordinates of the selected area in the image
// width，height：dimensions of the region to extract from the image
// src：compressed data
// buf：decompression buffer
void zhRGB565_decompress_baseversion(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height,const uint16_t *src, uint16_t *buf)
{
    uint16_t pic_width = GET_RGB565_ENCODER_WIDTH(src);
    uint16_t pic_height = GET_RGB565_ENCODER_HEIGHT(src);
    uint16_t ecoder_rle_Flag = GET_RGB565_ENCODER_FLAG(src);
    // The high-byte flag is identical for both delta and RLE encoding; however, 
    // delta encoding produces far fewer data, so it never uses the “long-code” variant.
    uint16_t ecoder_diff_Flag = GET_RGB565_ENCODER_FLAG(src)|0x0080;
    uint32_t sjb_length = GET_RGB565_ENCODER_SJB_LENGHT(src);               // upgrade table length
    uint32_t encoder_addr = GET_RGB565_ENCODER_DATA_ADDR(src);              // Starting coordinates of encoded data
    
    uint16_t pic_line, pic_col;
    uint32_t line_pos_base = 0, real_width;
    uint32_t x00, line_addr;
    
    uint16_t ref_color, len, pixl_len;
    
    // Boundary check
    if (x0 >= pic_width || y0 >= pic_height) return;
    if (x0 + width > pic_width) width = pic_width - x0;
    if (y0 + height > pic_height) height = pic_height - y0;

    for(pic_line=y0; pic_line<(y0+height); pic_line++)
    {
        /************************* Process upgrade table *************************/
        if(sjb_length != 0)    // Upgrade table exists
        {
            // Linear search method affects performance when decoding ultra-large images
//            line_pos_base = 0;
//            for(uint16_t i=0; i<sjb_length; i++){
//                if(pic_line >= GET_RGB565_ENCODER_SJB_DATA(src, i))
//                {
//                    line_pos_base += 65536;
//                    if(pic_line < GET_RGB565_ENCODER_SJB_DATA(src, i+1))
//                        break;
//                }
//                else
//                {
//                    line_pos_base = 0;
//                    break;
//                }
//            }
            
            //  Binary search algorithm
            uint16_t low = 0, high = sjb_length;
            while (low < high)
            {
                uint16_t mid = (low + high)>>1;
                if (pic_line < GET_RGB565_ENCODER_SJB_DATA(src, mid))
                {
                    high = mid;
                }
                else
                {
                    low = mid + 1;
                }
            }
            line_pos_base = (uint32_t)low << 16;
        }
        /********************** upgrade table processing complete **********************/
        
        // Pre-fetch the start offset of the specified row in the encoded data
        line_addr = encoder_addr + GET_RGB565_ENCODER_LINE_DATA_ADDR(src, pic_line) + line_pos_base;
        
        x00 = 0;
        real_width = width;        // Actual image width
        for(pic_col = 0; pic_col<pic_width; pic_col++)
        {
            uint16_t EncodeData = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
            uint16_t tmpdata = EncodeData & 0xff80;
            
            if(tmpdata == ecoder_rle_Flag)
            {
                pic_col++;        // Point to ref_color
                ref_color = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                
                if (EncodeData == ecoder_rle_Flag)        // Long-code encoding
                {
                    pic_col++;        // Point to length data
                    pixl_len = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);        // Read RLE-encoded length
                }
                else     // short-code encoding
                {
                    pixl_len = EncodeData & 0x007f;        // Fetch encoded length
                }

                if(x00 < x0){        // x00 Cursor is to the left of the target start
                    if (x00 + pixl_len < x0)    // x00 Cursor still left of target after adding decode length, skip
                    {
                        x00 += pixl_len;
                        continue;               // Continue to fetch next data segment
                    }
                    else
                    {
                        pixl_len = x00 + pixl_len - x0;     // Valid length starting from x0 coordinate
                        x00 = x0 + pixl_len;
                    }
                }
                
                if (real_width > pixl_len)    // Required pixel length exceeds encoded length, decode entire run
                {
                    zhRGB565_RLE_decoder(ref_color, pixl_len, buf);
                    buf += pixl_len;
                    real_width -= pixl_len;
                }
                else
                {// Remaining pixels < encoded run length; decode only the leftover width
                    zhRGB565_RLE_decoder(ref_color, real_width, buf);
                    buf += real_width;
                    break;
                }
            }
            else if(tmpdata == ecoder_diff_Flag)    // Delta encoding flag
            {
                len = EncodeData & 0x007f;         // Fetch encoded length; delta encoding uses short-length format
                pixl_len = len*2 + 1;              // Total decompressed pixel length
                
                uint16_t real_pixl_len = pixl_len;
                uint16_t skip_cnt = 0;
                
                if(x00 < x0)        // x00 Cursor is to the left of the target start
                {    
                    if (x00 + pixl_len < x0)    // x00 Cursor is to the left of the target start
                    {
                        x00 += pixl_len;
                        pic_col += len + 1;
                        continue;    
                    }
                    else    // If fully decoded, length can reach the start coordinate
                    {
                        // Calculate new coordinate and decode length
                        real_pixl_len = x00 + pixl_len - x0;
                        x00 = x0 + real_pixl_len;
                        skip_cnt  = pixl_len - real_pixl_len;    // Number of leading pixels to skip
                    }
                }

                uint16_t pix1, pix2, encdata;
                pic_col++;        // Point to ref_color
                ref_color = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                
                if(skip_cnt == 0){        // No skip needed, decode directly
                    *buf++ = ref_color;        // Reference color stored as-is, retrieved as-is
                    real_width--;
                    // Decoding of current line finished, switch to next line
                    if(real_width == 0){ break; }
                }
                else
                    skip_cnt--;
                
                for(uint16_t i=0; i<len; i++)
                {
                    pic_col++;        // Point to encoded data
                    encdata = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                    zhRGB565_DIFF_decoder(ref_color, encdata, &pix1, &pix2);
                    
                    if(skip_cnt == 0){        // No skip needed, decode directly
                        *buf++ = pix1;        // First pixel
                        real_width--;
                        // Decoding of current line finished, switch to next line
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;
                    
                    if(skip_cnt == 0){        // No skip needed, decode directly
                        *buf++ = pix2;        // Second pixel
                        real_width--;
                        // Decoding of current line finished, switch to next line
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;
                    // The last decoded pixel serves as the reference color for the next encoded data
                    ref_color = pix2;
                }
                            
                // Buffer line break
                if(real_width == 0){ break; }
            }
            else
            {
                if (x00 < x0)
                {
                    x00++;
                    continue;
                }
                *buf++ = EncodeData;
                real_width--;
                // Buffer line break
                if(real_width == 0){ break; }
            }
        }
    }
}
#endif

#if defined(RTE_Acceleration_Arm_2D)

#if defined(RTE_Acceleration_Arm_2D_Extra_Loader) && __ARM_2D_ZHRGB565_USE_LOADER_IO__
#undef GET_RGB565_ENCODER_WIDTH
#undef GET_RGB565_ENCODER_HEIGHT
#undef GET_RGB565_ENCODER_FLAG
#undef GET_RGB565_ENCODER_SJB_LENGHT
#undef GET_RGB565_ENCODER_LINE_POS
#undef GET_RGB565_ENCODER_DATA_ADDR
#undef GET_RGB565_ENCODER_SJB_DATA
#undef GET_RGB565_ENCODER_LINE_DATA_ADDR
#undef GET_RGB565_ENCODER_LINE_DATA
#undef GET_RGB565_ENCODER_LINE_DATA2

__STATIC_FORCEINLINE
void zhRGB565_read_u16_array_from_loader(uintptr_t ptLoader,
                                         uint8_t *phwBuffer,
                                         uint32_t Count)
{
    arm_generic_loader_io_read( (arm_generic_loader_t *)ptLoader,
                                (uint8_t *)phwBuffer,
                                Count * sizeof(uint16_t));
}

__STATIC_FORCEINLINE
void zhRGB565_read_u16_array_from_loader_with_offset(uintptr_t ptLoader, uintptr_t nOffset, uint32_t Count, uint8_t *phwBuffer)
{
    arm_generic_loader_io_seek( (arm_generic_loader_t *)ptLoader,
                                nOffset * sizeof(uint16_t),
                                SEEK_SET);
     zhRGB565_read_u16_array_from_loader(ptLoader, (uint8_t *)phwBuffer, Count);
}

/* Get image data */
#define GET_RGB565_ENCODER_DATA(buf, __addr, offset, count)    \
    zhRGB565_read_u16_array_from_loader_with_offset((uintptr_t)(__addr), (uintptr_t)(offset), (count), (uint8_t*)(buf))

/******************************************************************************************************/

__STATIC_FORCEINLINE
uint16_t zhRGB565_read_u16_from_loader(uintptr_t ptLoader)
{
    uint16_t hwData;
    arm_generic_loader_io_read( (arm_generic_loader_t *)ptLoader, 
                                (uint8_t *)&hwData, 2);

    return hwData;
}

__STATIC_FORCEINLINE
uint16_t zhRGB565_read_u16_from_loader_with_offset(uintptr_t ptLoader, uintptr_t nOffset)
{
    arm_generic_loader_io_seek( (arm_generic_loader_t *)ptLoader, 
                                nOffset * sizeof(uint16_t), 
                                SEEK_SET);
    return zhRGB565_read_u16_from_loader(ptLoader);
}

__STATIC_FORCEINLINE
void zhRGB565_set_loader_offset(uintptr_t ptLoader, uintptr_t nOffset)
{
    arm_generic_loader_io_seek( (arm_generic_loader_t *)ptLoader, 
                                nOffset * sizeof(uint16_t), 
                                SEEK_SET);
}

/* Get image width */
//#define     GET_RGB565_ENCODER_WIDTH(BUF)                ((BUF)[0])
#define GET_RGB565_ENCODER_WIDTH(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 0)

/* Get image height */
//#define     GET_RGB565_ENCODER_HEIGHT(BUF)                ((BUF)[1])
#define GET_RGB565_ENCODER_HEIGHT(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 1)

/* Get image encoding flag */
//#define     GET_RGB565_ENCODER_FLAG(BUF)                ((BUF)[2])
#define GET_RGB565_ENCODER_FLAG(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 2)

/* Get image upgrade table length */
//#define     GET_RGB565_ENCODER_SJB_LENGHT(BUF)          ((BUF)[3])
#define GET_RGB565_ENCODER_SJB_LENGHT(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 3)

/* Get image row-offset table start address */
//#define     GET_RGB565_ENCODER_LINE_POS(BUF)            ((BUF)[4])
#define GET_RGB565_ENCODER_LINE_POS(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 4)

/* Get image encoded data start address */
//#define     GET_RGB565_ENCODER_DATA_ADDR(BUF)            ((BUF)[5])
#define GET_RGB565_ENCODER_DATA_ADDR(__addr)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 5)

/* Get image row index of the N-th entry in upgrade table, N = 0,1,2... */
//#define     GET_RGB565_ENCODER_SJB_DATA(BUF, N)            ((BUF)[6+N])
#define GET_RGB565_ENCODER_SJB_DATA(__addr, __index)    \
        zhRGB565_read_u16_from_loader_with_offset((uintptr_t)(__addr), 6 + (__index))

/* Get image address of N-th row's encoded data in row-offset table, N = 0,1,2... */
//#define     GET_RGB565_ENCODER_LINE_DATA_ADDR(BUF,N)    ((BUF)[GET_RGB565_ENCODER_LINE_POS(BUF) + (N)])
#define GET_RGB565_ENCODER_LINE_DATA_ADDR(__addr, __index)                      \
        zhRGB565_read_u16_from_loader_with_offset(                              \
            (uintptr_t)(__addr),                                                \
            GET_RGB565_ENCODER_LINE_POS(__addr) + (__index))


/* Get image M-th data of N-th row's encoded data, N = 0,1,2... */
//#define     GET_RGB565_ENCODER_LINE_DATA(BUF,N,M)        ((BUF)[GET_RGB565_ENCODER_DATA_ADDR(BUF) + GET_RGB565_ENCODER_LINE_DATA_ADDR(BUF,N) + (M)])
#define GET_RGB565_ENCODER_LINE_DATA(__addr, N, M)                              \
        zhRGB565_read_u16_from_loader_with_offset(                              \
            (uintptr_t)(__addr),                                                \
            GET_RGB565_ENCODER_DATA_ADDR(__addr) +                              \
            GET_RGB565_ENCODER_LINE_DATA_ADDR(__addr, (N)) + (M))

//#define     GET_RGB565_ENCODER_LINE_DATA2(BUF,N,M)        ((BUF)[(N) + (M)])
#define GET_RGB565_ENCODER_LINE_DATA2(__addr, N, M)                             \
        zhRGB565_read_u16_from_loader((uintptr_t)(__addr))

#define SET_RGB565_ENCODER_READ_HINT(__addr, __index)                           \
        zhRGB565_set_loader_offset((uintptr_t)(__addr), __index)
#else

#   define SET_RGB565_ENCODER_READ_HINT(...)

#endif

ARM_NONNULL(1)
arm_2d_size_t zhRGB565_get_image_size(arm_generic_loader_t *ptLoader)
{
    assert(NULL != ptLoader);
#if __ARM_2D_ZHRGB565_USE_LOADER_IO__
    arm_2d_size_t tSize = {
        .iWidth = GET_RGB565_ENCODER_WIDTH((const uint16_t *)ptLoader),
        .iHeight = GET_RGB565_ENCODER_HEIGHT((const uint16_t *)ptLoader),
    };
#else
    arm_generic_loader_io_seek(    ptLoader, 
                                0, 
                                SEEK_SET);

    arm_zhrgb565_loader_t *ptThis = (arm_zhrgb565_loader_t *)ptLoader;
    const uint16_t *phwSrc = this.phwLocalSource;

    arm_2d_size_t tSize = {
        .iWidth = GET_RGB565_ENCODER_WIDTH(phwSrc),
        .iHeight = GET_RGB565_ENCODER_HEIGHT(phwSrc),
    };

#endif
    return tSize;
}

#if (__ARM_2D_ZHRGB565_USE_LOADER_IO__ == 0)
// x0,y0:(x0,y0) top-left coordinates of the selected area in the image
// width，height：dimensions of the region to extract from the image
// src：compressed data
// buf：decompression buffer
void zhRGB565_decompress_for_arm2d(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const uint16_t *src, COLOUR_INT *buf, int16_t iTargetStride)
{
    uint16_t pic_width = GET_RGB565_ENCODER_WIDTH(src);
    uint16_t pic_height = GET_RGB565_ENCODER_HEIGHT(src);
    uint16_t ecoder_rle_Flag = GET_RGB565_ENCODER_FLAG(src);
    // The high-byte flag is identical for both delta and RLE encoding; however, 
    // delta encoding produces far fewer data, so it never uses the “long-code” variant.
    uint16_t ecoder_diff_Flag = ecoder_rle_Flag |0x0080;
    uint32_t sjb_length = GET_RGB565_ENCODER_SJB_LENGHT(src);           // upgrade table length
    uint32_t encoder_addr = GET_RGB565_ENCODER_DATA_ADDR(src);          // Starting coordinates of encoded data
    
    uint16_t pic_line, pic_col;
    uint32_t line_pos_base = 0, real_width;
    uint32_t x00, line_addr;
    
    COLOUR_INT *buf_base;
    buf_base = (COLOUR_INT *)buf;
    
    uint16_t ref_color, len, pixl_len;
    
    // Boundary check
    if (x0 >= pic_width || y0 >= pic_height) return;
    if (x0 + width > pic_width) width = pic_width - x0;
    if (y0 + height > pic_height) height = pic_height - y0;

    for(pic_line=y0; pic_line<(y0+height); pic_line++)
    {
        /************************* Process upgrade table *************************/
        if(sjb_length != 0)    // Upgrade table exists
        {
            // Binary search algorithm
            uint16_t low = 0, high = sjb_length;
            while (low < high)
            {
                uint16_t mid = (low + high)>>1;
                if (pic_line < GET_RGB565_ENCODER_SJB_DATA(src, mid))
                {
                    high = mid;
                }
                else
                {
                    low = mid + 1;
                }
            }
            line_pos_base = (uint32_t)low << 16;
        }
        /********************** upgrade table processing complete **********************/

        // Pre-fetch the start offset of the specified row in the encoded data
        line_addr = encoder_addr + GET_RGB565_ENCODER_LINE_DATA_ADDR(src, pic_line) + line_pos_base;

        x00 = 0;
        real_width = width;        // Actual image width
        for(pic_col = 0; pic_col<pic_width; pic_col++)
        {
            SET_RGB565_ENCODER_READ_HINT(src, line_addr + pic_col);
            uint16_t EncodeData = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
            uint16_t tmpdata = EncodeData & 0xff80;
            
            /********************************* RLE decoder  **********************************/
            if(tmpdata == ecoder_rle_Flag)
            {
                pic_col++;        // Point to ref_color
                ref_color = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                
                if (EncodeData == ecoder_rle_Flag)        // Long-code encoding
                {
                    pic_col++;        // Point to length data
                    pixl_len = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);        // Read RLE-encoded length
                }
                else    // short-code encoding
                {
                    pixl_len = EncodeData & 0x007f;        // Fetch encoded length
                }

                if(x00 < x0){        // x00 Cursor is to the left of the target start
                    if (x00 + pixl_len < x0)    // x00 Cursor still left of target after adding decode length, skip
                    {
                        x00 += pixl_len;
                        continue;                // Continue to fetch next data segment
                    }
                    else
                    {
                        pixl_len = x00 + pixl_len - x0;    // Valid length starting from x0 coordinate
                        x00 = x0 + pixl_len;
                    }
                }
                
                if (real_width > pixl_len)    // Required pixel length exceeds encoded length, decode entire run
                {
                    zhRGB565_RLE_decoder(ref_color, pixl_len, buf);
                    buf += pixl_len;
                    real_width -= pixl_len;
                }
                else
                {// Remaining pixels < encoded run length; decode only the leftover width
                    zhRGB565_RLE_decoder(ref_color, real_width, buf);
                    buf += real_width;
                    // Buffer line break
                    buf_base += iTargetStride;
                    buf = buf_base;
                    break;
                }
            }
            /********************************* Delta decoding  **********************************/
            else if(tmpdata == ecoder_diff_Flag)    // Delta encoding flag
            {
                len = EncodeData & 0x007f;            // Fetch encoded length; delta encoding uses short-length format
                pixl_len = len*2 + 1;                 // Total decompressed pixel length
                
                uint16_t real_pixl_len = pixl_len;
                uint16_t skip_cnt = 0;
                
                if(x00 < x0)        // x00 Cursor is to the left of the target start
                {    
                    if (x00 + pixl_len < x0)    // x00 Cursor still left of target after adding decode length, skip
                    {
                        x00 += pixl_len;
                        pic_col += len + 1;
                        continue;    
                    }
                    else    // If fully decoded, length can reach the start coordinate
                    {
                        // Calculate new coordinate and decode length
                        real_pixl_len = x00 + pixl_len - x0;
                        x00 = x0 + real_pixl_len;
                        skip_cnt = pixl_len - real_pixl_len;    // Number of leading pixels to skip
                    }
                }

                uint16_t pix1, pix2, encdata;
                pic_col++;        // Point to encoded data
                ref_color = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                
                if(skip_cnt == 0){            // No skip needed, decode directly
                    // Reference color stored as-is, retrieved as-is
                    zhRGB565_save_pixel(buf++, ref_color);
                    real_width--;
                    // Decoding of current line finished, switch to next line
                    if(real_width == 0){
                        buf_base += iTargetStride;
                        buf = buf_base;
                        break;
                    }
                }
                else
                    skip_cnt--;
                
                for(uint16_t i=0; i<len; i++)
                {
                    pic_col++;        // Point to encoded data
                    encdata = GET_RGB565_ENCODER_LINE_DATA2(src, line_addr, pic_col);
                    zhRGB565_DIFF_decoder(ref_color, encdata, &pix1, &pix2);    // Decode one data word to yield 2 pixels
                    
                    if(skip_cnt == 0){        // No skip needed, decode directly
                        // First pixel
                        zhRGB565_save_pixel(buf++, pix1);
                        real_width--;
                        // Decoding of current line finished, switch to next line
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;
                    
                    if(skip_cnt == 0){        // No skip needed, decode directly
                        // Second pixel
                        zhRGB565_save_pixel(buf++, pix2);
                        real_width--;
                        // Decoding of current line finished, switch to next line
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;
                    // The last decoded pixel serves as the reference color for the next encoded data
                    ref_color = pix2;
                }
                            
                // Buffer line break
                if(real_width == 0){ buf_base += iTargetStride; buf = buf_base; break; }
            }
            else
            {
                if (x00 < x0)
                {
                    x00++;
                    continue;
                }
                zhRGB565_save_pixel(buf++, EncodeData);
                real_width--;
                
                // Buffer line break
                if(real_width == 0){ buf_base += iTargetStride; buf = buf_base; break; }
            }
        }
    }
}
#endif

#if __ARM_2D_ZHRGB565_USE_LOADER_IO__

#ifndef __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__
#   define __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__     32
#endif

#if __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__ < 6
#   undef __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__
#   define __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__ 6
#endif

#define FLUSH_RGB565_CACHE(cache, idx, src, addr)    \
    do { \
        if ((idx) == __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__) {    \
            (addr) += __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__; \
            GET_RGB565_ENCODER_DATA((cache), (src), (addr), __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__); \
            (idx) = 0; \
        } \
    } while(0)

//  Static cache variables used to accelerate upgrade table calculation
static uint32_t last_ext_addr = 0xFFFFFFFF;
static uint16_t last_pic_line = 0xFFFF;
static int16_t  last_sjb_index = -1;
static uint32_t last_line_pos_base = 0;

/*   Fast upgrade table calculation algorithm with caching */
uint32_t calc_line_pos_base_flash(uint16_t pic_line, uint32_t ExtAddr, uint16_t sjb_length)
{
    uint16_t low, high, mid;
    uint16_t mid_val;
    uint16_t next_sjb_val;
    uint32_t line_pos_base;
    uint8_t cache_valid;
    
    // Detect whether the image resource has been changed.
    if (ExtAddr != last_ext_addr) {
        last_ext_addr = ExtAddr;
        last_pic_line = 0xFFFF;
        last_sjb_index = -1;      // Invalid
        last_line_pos_base = 0;
        cache_valid = 0;
    } else {
        cache_valid = (last_pic_line != 0xFFFF) && (last_sjb_index >= -1);
    }
    
    // Fast path 1: Same row as last time, return the previous upgrade value directly
    if (cache_valid && pic_line == last_pic_line) {
        return last_line_pos_base;
    }
    
    // Fast path 2: Next row
    if (cache_valid && pic_line == last_pic_line + 1) {
        if (last_sjb_index == -1) {
            GET_RGB565_ENCODER_DATA(&next_sjb_val, ExtAddr, 6, 1);  // read sjb[0]
            if (pic_line >= next_sjb_val) {
                last_sjb_index = 0;
            }
        }
        else if (last_sjb_index + 1 < sjb_length) {
            GET_RGB565_ENCODER_DATA(&next_sjb_val, ExtAddr, 6 + last_sjb_index + 1, 1);
            if (pic_line >= next_sjb_val) {
                last_sjb_index++;
            }
        }
        
        last_pic_line = pic_line;
        last_line_pos_base = ((uint32_t)(last_sjb_index + 1)) << 16;
        return last_line_pos_base;
    }
    
    // Fast path 2: Previous row
    if (cache_valid && pic_line == last_pic_line - 1 && last_pic_line != 0) {
        if (last_sjb_index == -1) {
            // Already at the beginning, keep -1.
        }
        else if (last_sjb_index == 0) {
            GET_RGB565_ENCODER_DATA(&mid_val, ExtAddr, 6, 1);  // read sjb[0]
            if (pic_line < mid_val) {
                last_sjb_index = -1;
            }
        }
        else {
            GET_RGB565_ENCODER_DATA(&mid_val, ExtAddr, 6 + last_sjb_index, 1);
            if (pic_line < mid_val) {
                last_sjb_index--;
            }
        }
        
        last_pic_line = pic_line;
        last_line_pos_base = ((uint32_t)(last_sjb_index + 1)) << 16;
        return last_line_pos_base;
    }
    
    // Slow path: Binary search, only entered during first decode or when switching decoding resources
    low = 0;
    high = sjb_length;
    
    while (low < high) {
        mid = (low + high) >> 1;
        GET_RGB565_ENCODER_DATA(&mid_val, ExtAddr, 6 + mid, 1);
        
        if (pic_line < mid_val) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    
    last_sjb_index = (int16_t)low - 1;
    line_pos_base = ((uint32_t)(last_sjb_index + 1)) << 16;
    
    last_pic_line = pic_line;
    last_line_pos_base = line_pos_base;
    
    return line_pos_base;
}
    
// External Flash data read version
void zhRGB565_decompress_for_arm2d(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const uint16_t *src, COLOUR_INT *buf, int16_t iTargetStride)
{
    uint16_t cache_index;
    uint16_t cache_u16[__ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__];
    GET_RGB565_ENCODER_DATA(cache_u16, src, 0, 6);            // read image info

    uint16_t pic_width = cache_u16[0];
    uint16_t pic_height = cache_u16[1];
    uint16_t ecoder_rle_Flag = cache_u16[2];
    uint16_t ecoder_diff_Flag = cache_u16[2]|0x0080;
    uint16_t sjb_length = cache_u16[3];                    // upgrade table length
    uint32_t encoder_addr = cache_u16[5];                // Starting coordinates of encoded data
    uint16_t row_offset_addr_start = cache_u16[4];

    uint16_t pic_line, pic_col;
    uint32_t line_pos_base = 0, real_width;
    uint32_t x00, line_addr;
    COLOUR_INT *buf_base;
    buf_base = buf;

    uint16_t ref_color, len, pixl_len;

    // Boundary check
    if (x0 >= pic_width || y0 >= pic_height) return;
    if (x0 + width > pic_width) width = pic_width - x0;
    if (y0 + height > pic_height) height = pic_height - y0;

    for(pic_line=y0; pic_line<(y0+height); pic_line++)        //  Line-by-line decoding
    {
        /************************* Process upgrade table *************************/
        if(sjb_length != 0)        // Upgrade table exists
        {
            if (sjb_length <= __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)        //   Upgrade table length is smaller than cache length.
            {
                //  Binary search algorithm
                uint16_t low = 0, high = sjb_length;
                GET_RGB565_ENCODER_DATA(cache_u16, src, 6, sjb_length);        //  Read the entire Upgrade table data.
                while (low < high)
                {
                    uint16_t mid = (low + high)>>1;
                    if (pic_line < cache_u16[mid])
                    {
                        high = mid;
                    }
                    else 
                    {
                        low = mid + 1;
                    }
                }
                line_pos_base = (uint32_t)low << 16;
            }
            else    // Upgrade table length is greater than cache length.
            {
                // Linear search method: when the lookup table has many elements, decoding data towards the end will impact performance.
//                uint32_t sjb_addr = 6;
//                uint16_t sjb_length_tmp = sjb_length;
//                cache_index = 0;
//                GET_RGB565_ENCODER_DATA(cache_u16, ExtAddr, 6, __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__);
//                for(uint32_t i=0; i<sjb_length; i++)
//                {
//                    sjb_length_tmp--;
//                    uint16_t sjb_val_tmp = cache_u16[cache_index++];
//                    if (cache_index == __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)
//                    {
//                        sjb_addr += __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__;

//                        if (sjb_length_tmp >= __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)
//                        {
//                            GET_RGB565_ENCODER_DATA(cache_u16, ExtAddr, sjb_addr, __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__);
//                        }
//                        else
//                        {
//                            GET_RGB565_ENCODER_DATA(cache_u16, ExtAddr, sjb_addr, sjb_length_tmp);
//                        }
//                        cache_index = 0;
//                    }

//                    if(pic_line >= sjb_val_tmp)
//                    {
//                        line_pos_base = (i+1)<<16;
//                        sjb_val_tmp = cache_u16[cache_index];
//                        if(pic_line < sjb_val_tmp)
//                            break;
//                    }
//                    else
//                    {
//                        line_pos_base = 0;
//                        break;
//                    }
//                }

                // Binary search + cache memory, fast upgrade value calculation
                line_pos_base = calc_line_pos_base_flash(pic_line, (uintptr_t)(src), sjb_length);
            }
        }

        // Calculate the flash address of the current row's offset table.
        uint32_t tmpAddr = (uint32_t)row_offset_addr_start + (uint32_t)pic_line;
        GET_RGB565_ENCODER_DATA(cache_u16, src, tmpAddr, 2);        // Read the offset table of the specified row and its next row.

        // Calculate the amount of compressed data for the current row.
        uint16_t compressed_num = cache_u16[1] - cache_u16[0];
        
        // Calculate the starting address of the current row's compressed data.
        line_addr = encoder_addr + (uint32_t)cache_u16[0] + line_pos_base;

        if (compressed_num <= __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)
        {
            GET_RGB565_ENCODER_DATA(cache_u16, src, line_addr, compressed_num);
        }
        else
        {
            GET_RGB565_ENCODER_DATA(cache_u16, src, line_addr, __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__);
        }

        cache_index = 0;
        x00 = 0;
        real_width = width;        // Actual image width
        for(pic_col = 0; pic_col<pic_width; pic_col++)
        {
            uint16_t EncodeData = cache_u16[cache_index++];
            uint16_t tmpdata = EncodeData & 0xff80;
            FLUSH_RGB565_CACHE(cache_u16, cache_index, src, line_addr);

            /********************************* RLE  **********************************/
            if(tmpdata == ecoder_rle_Flag)
            {
                // Point to ref_color
                ref_color = cache_u16[cache_index++];
                FLUSH_RGB565_CACHE(cache_u16, cache_index, src, line_addr);

                if (EncodeData == ecoder_rle_Flag)        // Long-code encoding
                {
                    pixl_len = cache_u16[cache_index++];        // Point to length data
                    FLUSH_RGB565_CACHE(cache_u16, cache_index, src, line_addr);

                }
                else    // short-code encoding
                {
                    pixl_len = EncodeData & 0x007f;        // Fetch encoded length
                }

                if(x00 < x0)        // x00 Cursor is to the left of the target start
                {
                    if (x00 + pixl_len < x0)    // x00 Cursor still left of target after adding decode length, skip
                    {
                        x00 += pixl_len;
                        continue;                // Continue to fetch next data segment
                    }
                    else
                    {
                        pixl_len = x00 + pixl_len - x0;        // Valid length starting from x0 coordinate
                        x00 = x0 + pixl_len;
                    }
                }

                if (real_width > pixl_len)    // Required pixel length exceeds encoded length, decode entire run
                {
                    zhRGB565_RLE_decoder(ref_color, pixl_len, buf);
                    buf += pixl_len;
                    real_width -= pixl_len;
                }
                else
                {// Remaining data length to decode is less than the encoded length; directly decode the remaining length of data.
                    zhRGB565_RLE_decoder(ref_color, real_width, buf);
                    buf += real_width;
                    // Buffer line feed
                    buf_base += iTargetStride;
                    buf = buf_base;
                    break;
                }
            }
            /********************************* Delta decode  **********************************/
            else if(tmpdata == ecoder_diff_Flag)    // Delta encoding flag
            {
                len = EncodeData & 0x007f;            // Fetch encoded length; delta encoding uses short-length format
                pixl_len = len*2 + 1;                // Total decompressed pixel length

                uint16_t real_pixl_len = pixl_len;
                uint16_t skip_cnt = 0;

                if(x00 < x0)        // x00 Cursor is to the left of the target start
                {
                    if (x00 + pixl_len < x0)
                    {
                        x00 += pixl_len;
                        for (uint16_t ii = (len + 1); ii > 0; ii--)
                        {
                            cache_index++;
                            if (cache_index == __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)
                            {
                                cache_index = 0;
                                line_addr += __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__;
                                if (ii > __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__)
                                {
                                    continue;
                                }
                                GET_RGB565_ENCODER_DATA(cache_u16, src, line_addr, __ARM_2D_ZHRGB565_PIXEL_CACHE_SIZE__);
                            }
                        }
                        continue;
                    }
                    else    // If fully decoded, length can reach the start coordinate
                    {
                        // Calculate new coordinate and decode length
                        real_pixl_len = x00 + pixl_len - x0;
                        x00 = x0 + real_pixl_len;
                        skip_cnt = pixl_len - real_pixl_len;    // Number of leading pixels to skip
                    }
                }

                uint16_t pix1, pix2, encdata;
                // Point to ref_color
                ref_color = cache_u16[cache_index++];
                FLUSH_RGB565_CACHE(cache_u16, cache_index, src, line_addr);

                if(skip_cnt == 0){                              // No skip needed, decode directly

                    zhRGB565_save_pixel(buf++, ref_color);      // Reference color stored as-is, retrieved as-is
                    //*buf++ = ref_color;        
                    real_width--;
                    // Buffer line feed
                    if(real_width == 0){ buf_base += iTargetStride;    buf = buf_base;    break; }
                }
                else
                    skip_cnt--;

                for(uint16_t i=0; i<len; i++)
                {
                    encdata = cache_u16[cache_index++];
                    FLUSH_RGB565_CACHE(cache_u16, cache_index, src, line_addr);

                    zhRGB565_DIFF_decoder(ref_color, encdata, &pix1, &pix2);    // One data unit can decode into 2 pixels.
                    if(skip_cnt == 0){                          // No skip needed, decode directly
                        zhRGB565_save_pixel(buf++, pix1);       // First pixel
                        //*buf++ = pix1;        
                        real_width--;
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;

                    if(skip_cnt == 0){                          // No skip needed, decode directly
                        zhRGB565_save_pixel(buf++, pix2);       // Second pixel
                        //*buf++ = pix2;        
                        real_width--;
                        if(real_width == 0){ break; }
                    }
                    else
                        skip_cnt--;

                    ref_color = pix2;
                }

                // Buffer line feed
                if(real_width == 0){ buf_base += iTargetStride; buf = buf_base; break; }
            }
            else
            {
                if (x00 < x0)
                {
                    x00++;
                    continue;
                }
                zhRGB565_save_pixel(buf++, EncodeData);
                //*buf++ = EncodeData;
                real_width--;

                // Buffer line feed
                if(real_width == 0){ buf_base += iTargetStride; buf = buf_base; break; }
            }
        }
    }
}

#endif
#endif

