# Resource Loader

## Overview


## Comparison Among Image Decoders

| Format Name | Lossless Compression | Support Alpha | Decoding Speed | Compressed Image Size | Working Memory | Note                                                         |
| ----------- | -------------------- | ------------- | -------------- | --------------------- | -------------- | ------------------------------------------------------------ |
| TJpgDec     | NO                   | NO            | Slow           | Smallest              | 3.3K           |                                                              |
| ZJpgD       | NO                   | NO            | Slow           | Smallest              | 3K             | Optimised for ROI                                            |
| QOI         | YES                  | YES           | Fast           | Similar to PNG        | 1.5K           | Best for storing Icons and Animation                         |
| zhRGB565    | Only for RGB565      | NO            | Very Fast      | Balanced              | NO             | Best for RGB565 Animation                                    |
| LMSK        | YES                  | YES           | Very Fast      | Smallest              | NO             | The **most optimal solution** for mid/large masks. It supports **A1/A2/A3...A8** compression quality. It is always decoded as an 8-bit alpha mask. |

