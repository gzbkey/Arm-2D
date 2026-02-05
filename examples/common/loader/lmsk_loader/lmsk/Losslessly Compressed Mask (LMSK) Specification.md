# Losslessly Compressed Mask (LMSK) Specification (1.1.0)



## 1 Overview 

**LMSK** is a lossless compression format optimised for alpha mask images. It employs a hybrid entropy coding strategy combining 4-bit deltas, run-length encoding, linear gradient descriptions, and random line access capabilities.



#### Byte Order
- **Integers**: All multi-byte integers are stored in **Little Endian**.
- **Bitstream**: Decoding uses **LSB First** bit ordering (Least Significant Bit consumed first).

#### File Structure
```
[Header: 16 bytes]
[64 Palette]
[Floor Table: floor_count * 2 bytes]
[Line Index Table: height * 2 bytes]
[Data Stream]
```



## 2 Data Structures

### Header

```c
/* 16byte header */
typedef struct arm_lmsk_header_t {
    uint8_t chName[5];					 /*! "LMSK": Losslessly compressed MaSK */
    struct {
        uint8_t u4Major : 4;
        uint8_t u4Minor : 4;
    } Version;
  
    int16_t iWidth;
    int16_t iHeight;
  
    uint8_t u3AlphaMSBCount : 3;  /*! MSB alpha bits = u3AlphaBits + 1 */
    uint8_t bRaw          	: 1;  /*! whether the alpha is compressed or not */
    uint8_t u2TagSetBits    : 2;  /*! must be 0x00, reservef for the future */
    uint8_t                 : 2;  /*! must be 0x00, reserved for the future */
    uint8_t chFloorCount;
    uint32_t                : 32; /*! reserved */
} arm_lmsk_header_t;
```
- `u3AlphaMSBCount`: Significant alpha bits minus 1 (i.e., effective_bits = value + 1).  Unless otherwise specified,  all operations apply only to these significant high bits.
- `bRaw`: 
  - `0` - The data section contains compressed data. The Floor Table and Line Index Table are valid.
  - `1`- The data section **ONLY** contains the raw alpha pixels. The Floor Table and the Line Index Table are removed. Hence, `chFloorCount` should always be `0`, and the decoder should ignore `chFloorCount` and `u3AlphaMSBCount`.


### Floor Table and Line Index
The **Floor Table** defines base address partitions for line data. For a line number `L` where `L >= floor_table[i]` (and `L < floor_table[i+1]` if exists), the base address for that line is `(i + 1) * (1 << (16 - u2TagSetBits))` bytes or `(i + 1) * 65536` for the current version.

The **Line Index Table** stores **byte offsets** for each line relative to its **base address**. The address in the **data section** is calculated as:
```
base_address = (floor_index + 1) * (1 << (16 - u2TagSetBits))
line_offset  = line_index_table[row]
line_pos     = base_address + line_offset
```

> [!TIP]
>
> **Row-level Deduplication**
> Multiple line index entries **may point to the same offset** within the data stream. If several scanlines contain identical pixel data (e.g., solid horizontal lines or repeating patterns), the encoder may map these line indices to the same compressed data segment. The decoder processes each line independently without requiring special handling.



## 3 Data Stream Specification

The data stream (a.k.a **data section**) is organised by scanlines. Each line begins with **1 byte of raw alpha value** (the absolute value of the first pixel in the line), followed by a bitstream of Tags.

### 3.1 Tags

| Tag Bits (LSB) |    Size    | Name            | Description                                                  |
| :------------- | :--------: | :-------------- | :----------------------------------------------------------- |
| `00`           | **8 bits** | **Index**       | An index for a constant Palette.                             |
| `01`           | **8 bits** | **REPEAT**      | Run-length control. `count = bits[7:2]` (6-bit):<br>• `0`: **DO**<br>• `1–61`: **WHILE**, run length = `count + 1`<br>• `62` (0xF8): **GRADIENT_TAG**<br>• `63` (0xFC): **ALPHA_TAG** |
| `10`           | **8 bits** | **DELTA_SMALL** | Small delta.  Two delta encode two pixels, here`delta = sign_extend(bits[3:1], 3)`, range **[-4, +3]**. |
| `11`           | **8 bits** | **DELTA_LARGE** | Large delta. `delta = sign_extend(bits[7:2], 6)`, range **[-32, +31]**. |

##### Special Tags Details

* **ALPHA_TAG (0xFC)**  

  Followed by **1 byte** of raw alpha value, setting the current pixel directly. 

  > [!NOTE]
  >
  > The `u3AlphaMSBCount` does **NOT** affect **ALPHA_TAG**. It always uses 8-bit. 

  

* **GRADIENT_TAG (0xF8)**  

  Followed by **3 bytes**: `uint8_t to_alpha`, `uint16_t count`.  Linearly interpolates from current alpha to target alpha using **Q15.16 fixed-point** arithmetic over `count` pixels:

```c
q16_step = ((to_alpha - previous_alpha) << 16) / count;
for (i = 0; i < count - 1; i++)
    out[i] = current + ((q16_step * i) >> 16);
out[count-1] = to_alpha;  /* Force endpoint alignment */
previous_alpha = to_alpha;
```

> [!IMPORTANT]
>
> When using a gradient mode, regardless of `u3AlphaMSBCount`, the start and stop alpha values are treated as 8-bit alpha. If the previous pixel is different from the actual alpha, an **ALPHA_TAG** **MUST**  be inserted to set the start alpha. 

> [!IMPORTANT]
>
> The encoder **MUST** always emit an even number of consecutive **DELTA_SMALL** tags to maintain **8-bit** alignment. If not possible, please choose **DELTA_LARGE** instead. 



* **DO / WHILE**  
  - **Standalone WHILE** (a.k.a **REPEAT**): If no matching **DO** precedes it, repeat the **previous pixel** `count + 1` times.  
  
  - **Paired**: Instructions between **DO** and **WHILE** form a **macro**, executed `count + 1` times (initial + N repeats). Nesting is not supported.
  - **Standalone DO** (without **WHILE**) is ignored. 


> [!IMPORTANT]
>
> The **DO / WHILE** implements a loop structure. If you want to repeat the exact same string of pixels, you have to explicitly specify the pixel alpha at the start of an iteration; otherwise, for each iteration, the start alpha (a.k.a the alpha at the end of the previous iteration) might be different, leading to a totally different string of pixels. 
>
> Please implement **REPEAT_START/STOP** as a feature, not a **BUG**. 



### 3.2 Decoding Procedure
Per-line decoder state:
1. Calculate the start address of a line based on the **Floor Table** and the **Line Index Table**.
2. Read 1 byte as `current_alpha`.
3. Loop until `width` pixels are output:
   i)  Read a tag
   ii) Inspect the lowest 2 bits:
     - If `...01`: consume 4 bits, execute **DELTA_SMALL**.
     - If `...00`: consume 8 bits, parse **REPEAT** or **special tags**, i.e. **ALPHA_TAG** and **GRADIENT_TAG**
     - If `...10`: consume 8 bits, execute **DELTA_LARGE**.
4. Truncate immediately when line width is reached; discard remaining bits for that line.



## 4 Tag Sets Scheme Reserved for Future

There is a bitfield `u2TagSetBits` defined in the `arm_cm_header_t`. It determines how many **MSB** bits in each line index are used as **TAG_SET**.  Here:

* When `u2TagSetBits` are `0`, the **TAG_SET** of each line is seen as `0`.  In this version of the spec, the `u2TagSetBits` should always be `0`.
* **TAG_SETs** are read in little-endian. 
  * `0` is the TAG set defined in this spec.
  * Other values are reserved for the future.
* The floor advance is calculated as `1 << (16 - u2TagSetBits)`.
* Each line can have a different **TAG_SET** value in the future. In this version of the spec, all lines have the default **TAG_SET** value `0`.