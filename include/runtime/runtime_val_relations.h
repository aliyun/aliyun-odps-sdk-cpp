#ifndef APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_VAL_COMPARER_H
#define APSARA_ODPS_EXECUTION_ENGINE_RUNTIME_VAL_COMPARER_H
#include "runtime_decimal_val_funcs.h"
#include "include/runtime_types.h"

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif

namespace apsara {
namespace odps {
template <typename T0, typename T1 = T0, typename Enable = void>
class RuntimeValComparer {
 public:
  RuntimeValComparer(RuntimeTypePtr type0 = RuntimeTypePtr(),
                RuntimeTypePtr type1 = RuntimeTypePtr()) {}
  typedef T0 Type0;
  typedef T1 Type1;
  ALWAYS_INLINE int8_t EQ(const Type0& l, const Type1& r) const {
    return l == r;
  }
  ALWAYS_INLINE int8_t NE(const Type0& l, const Type1& r) const {
    return l != r;
  }
  ALWAYS_INLINE int8_t GT(const Type0& l, const Type1& r) const {
    return l > r;
  }
  ALWAYS_INLINE int8_t GE(const Type0& l, const Type1& r) const {
    return l >= r;
  }
  ALWAYS_INLINE int8_t LT(const Type0& l, const Type1& r) const {
    return l < r;
  }
  ALWAYS_INLINE int8_t LE(const Type0& l, const Type1& r) const {
    return l <= r;
  }
};

// T0 and T1 are both floating point
template <typename T0, typename T1>
class RuntimeValComparer<T0, T1,
        typename std::enable_if<std::is_floating_point<T0>::value &&
                std::is_floating_point<T1>::value>::type> {
public:
    RuntimeValComparer(RuntimeTypePtr type0 = RuntimeTypePtr(),
            RuntimeTypePtr type1 = RuntimeTypePtr()) {}
    typedef T0 Type0;
    typedef T1 Type1;
    ALWAYS_INLINE int8_t EQ(const Type0& l, const Type1& r) const {
        if (!std::isnan(l)) {
            return !std::isnan(r) && l == r;
        } else {
            return std::isnan(r);
        }
    }
    ALWAYS_INLINE int8_t NE(const Type0& l, const Type1& r) const { return !EQ(l, r); }
    ALWAYS_INLINE int8_t GT(const Type0& l, const Type1& r) const {
		// NaN as max value
        if (!std::isnan(r)) {
            return std::isnan(l) || l > r;
        } else {
            return false;
        }
    }
    ALWAYS_INLINE int8_t GE(const Type0& l, const Type1& r) const {
        return GT(l, r) || EQ(l, r);
    }
    ALWAYS_INLINE int8_t LT(const Type0& l, const Type1& r) const { return GT(r, l); }
    ALWAYS_INLINE int8_t LE(const Type0& l, const Type1& r) const { return GE(r, l); }
};

#define DEF_BINARY_RELATION_FOR_VAR_DECIMAL(T0, T1)                     \
template <>                                                             \
class RuntimeValComparer<T0, T1> {                                      \
 public:                                                                \
  RuntimeValComparer(RuntimeTypePtr type0, RuntimeTypePtr type1)        \
    : mScale0(type0->GetScale()), mScale1(type1->GetScale()) {          \
  }                                                                     \
  RuntimeValComparer(RuntimeTypePtr type)                               \
    : mScale0(type->GetScale()), mScale1(type->GetScale()) {            \
  }                                                                     \
  typedef T0 Type0;                                                     \
  typedef T1 Type1;                                                     \
  typedef decltype(std::declval<T0>().mValue) StorageT0;                \
  typedef decltype(std::declval<T1>().mValue) StorageT1;                \
  ALWAYS_INLINE int8_t EQ(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::EQ<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
  ALWAYS_INLINE int8_t NE(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::NE<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
  ALWAYS_INLINE int8_t GT(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::GT<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
  ALWAYS_INLINE int8_t GE(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::GE<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
  ALWAYS_INLINE int8_t LT(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::LT<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
  ALWAYS_INLINE int8_t LE(const T0& l, const T1& r) const  {            \
    return RuntimeDecimalValFuncs::LE<StorageT0, StorageT1>(            \
        l.mValue, mScale0, r.mValue, mScale1);                          \
  }                                                                     \
 private:                                                               \
  int32_t mScale0;                                                      \
  int32_t mScale1;                                                      \
};

DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal16, RuntimeDecimalVal16)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal16, RuntimeDecimalVal32)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal16, RuntimeDecimalVal64)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal16, RuntimeDecimalVal128)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal32, RuntimeDecimalVal16)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal32, RuntimeDecimalVal32)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal32, RuntimeDecimalVal64)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal32, RuntimeDecimalVal128)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal64, RuntimeDecimalVal16)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal64, RuntimeDecimalVal32)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal64, RuntimeDecimalVal64)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal64, RuntimeDecimalVal128)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal128, RuntimeDecimalVal16)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal128, RuntimeDecimalVal32)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal128, RuntimeDecimalVal64)
DEF_BINARY_RELATION_FOR_VAR_DECIMAL(RuntimeDecimalVal128, RuntimeDecimalVal128)
#undef DEF_BINARY_RELATION_FOR_VAR_DECIMAL

#define DEF_BINARY_RELATION(NAME)                                         \
template <typename T0, typename T1>                                       \
class RuntimeVal ## NAME {                                                \
 public:                                                                  \
  RuntimeVal ## NAME(RuntimeTypePtr type0, RuntimeTypePtr type1)          \
    : mComparer(type0, type1) {                                           \
  }                                                                       \
  RuntimeVal ## NAME(RuntimeTypePtr type)                                 \
    : mComparer(type) {                                                   \
  }                                                                       \
  typedef T0 Type0;                                                       \
  typedef T1 Type1;                                                       \
  ALWAYS_INLINE int8_t operator ()(const T0& l, const T1& r) const  {     \
    return mComparer.NAME(l, r);                                          \
  }                                                                       \
 private:                                                                 \
  RuntimeValComparer<T0, T1> mComparer;                                   \
};

DEF_BINARY_RELATION(GT)
DEF_BINARY_RELATION(GE)
DEF_BINARY_RELATION(LT)
DEF_BINARY_RELATION(LE)
DEF_BINARY_RELATION(EQ)
DEF_BINARY_RELATION(NE)
#undef DEF_BINARY_RELATION

} // namespace odps
} // namespace apsara
#endif
