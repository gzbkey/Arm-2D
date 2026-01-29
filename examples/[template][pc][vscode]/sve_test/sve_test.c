

#if __ARM_FEATURE_SVE2
#include <arm_sve.h>


#if __ARM_FEATURE_SVE_BITS == 512 && __ARM_FEATURE_SVE_VECTOR_OPERATORS

typedef svuint64_t vuint64x8_t __attribute__((arm_sve_vector_bits(__ARM_FEATURE_SVE_BITS)));
typedef svuint32_t vuint32x16_t __attribute__((arm_sve_vector_bits(__ARM_FEATURE_SVE_BITS)));

#endif

svuint16_t uaddlb_array(svuint8_t Zs1, svuint8_t Zs2)
{
    svuint16_t result = svmullb(Zs1, Zs2);
    return result;
}


vuint64x8_t uadd(vuint32x16_t Zs1, vuint32x16_t Zs2)
{
    vuint64x8_t result = Zs1 + Zs2;
    return result;
}


//static __attribute__((always_inline))
void __arm_2d_sve2_rgb565_fill_colour_with_src_mask(const uint8_t *pchSourceMask, uint16_t *phwTarget, uint64_t n)
{
    uint64_t i = 0;
    
    while (i < n) {
        // 创建 predicate：确定当前迭代要处理多少个 uint16_t 元素（剩余不足补满）
        svbool_t tailPred = svwhilelt_b16_u64(i, n);
        
        // 从 uint8_t* 加载，零扩展为 uint16_t
        // 每个 active lane 从 pchSourceMask[i + lane_index] 加载 1 字节，扩展为 16-bit
        svuint16_t vSource = svld1ub_u16(tailPred, &pchSourceMask[i]);
        svuint16_t vTarget = svld1_u16(tailPred, &phwTarget[i]);

        vSource += vTarget;
        
        // 存储到 uint16_t* 目标
        svst1_u16(tailPred, &phwTarget[i], vSource);
        
        // 步进：当前向量能容纳的 uint16_t 元素数量（通常是 VL/16）
        i += svcnth();
    }
}

#endif