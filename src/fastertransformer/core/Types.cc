#include "Types.h"

#include <string>
#include <cstdint>
#include <immintrin.h>

#if GOOGLE_CUDA
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#endif

namespace fastertransformer {

#define FT_FOREACH_TYPE(F) \
    F(DataType::TYPE_BOOL, bool); \
    F(DataType::TYPE_UINT8, uint8_t); \
    F(DataType::TYPE_UINT16, uint16_t); \
    F(DataType::TYPE_UINT32, uint32_t); \
    F(DataType::TYPE_UINT64, uint64_t); \
    F(DataType::TYPE_INT8, int8_t); \
    F(DataType::TYPE_INT16, int16_t); \
    F(DataType::TYPE_INT32, int32_t); \
    F(DataType::TYPE_INT64, int64_t); \
    F(DataType::TYPE_FP32, float); \
    F(DataType::TYPE_FP64, double); \
    F(DataType::TYPE_BYTES, char); \
    F(DataType::TYPE_STR, std::string);

#if GOOGLE_CUDA
#define FT_FOREACH_DEVICE_TYPE(F) \
    F(DataType::TYPE_FP16, half); \
    F(DataType::TYPE_BF16, __nv_bfloat16);
#else
struct fake_half {
    uint16_t x;
};
struct fake_bfloat16 {
    uint16_t x;
};
#define FT_FOREACH_DEVICE_TYPE(F) \
    F(DataType::TYPE_FP16, fake_half); \
    F(DataType::TYPE_BF16, fake_bfloat16);
#endif

template<typename T>
struct TypeTrait {
    static const DataType type = TYPE_INVALID;
    static const size_t size = 0;
};

#define DECLARE_TYPE_TRAIT(DT, T) \
    template<> \
    struct TypeTrait<T> { \
        static const DataType type = DT; \
        static const size_t size = sizeof(T); \
    };

#define DECLARE_GET_TYPE(T) \
    template DataType getTensorType<T>(); \
    template DataType getTensorType<const T>(); \
    template DataType getTensorType<const T *>();

#define DEFINE_TYPE(DT, T) \
    DECLARE_TYPE_TRAIT(DT, T); \
    DECLARE_GET_TYPE(T)

template<typename T>
DataType getTensorType() {
    return TypeTrait<T>::type;
}

FT_FOREACH_TYPE(DEFINE_TYPE);
FT_FOREACH_DEVICE_TYPE(DEFINE_TYPE);
DEFINE_TYPE(DataType::TYPE_UINT64, unsigned long long int);
DECLARE_GET_TYPE(void);

size_t getTypeSize(DataType type) {
#define CASE(DT, T) { \
    case DT: { \
        return TypeTrait<T>::size; \
    } \
}

    switch (type) {
        FT_FOREACH_TYPE(CASE);
        FT_FOREACH_DEVICE_TYPE(CASE);
        CASE(DataType::TYPE_QINT8, int8_t);
        default:
            return 0;
    }

}


} // namespace fastertransformer

