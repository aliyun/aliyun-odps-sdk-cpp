#ifndef APSARA_ODPS_SDK_MAX_STORAGE_API_COMMON_MACRO_DEFINE_H
#define APSARA_ODPS_SDK_MAX_STORAGE_API_COMMON_MACRO_DEFINE_H

template<typename T>
struct parameter_pass_trait {using Type = const T&; };
template<> struct parameter_pass_trait<bool> { using Type = bool; };
template<> struct parameter_pass_trait<int32_t> { using Type = int32_t; };
template<> struct parameter_pass_trait<int64_t> { using Type = int64_t; };
template<> struct parameter_pass_trait<uint32_t> { using Type = uint32_t; };
template<> struct parameter_pass_trait<uint64_t> { using Type = uint64_t; };
template<> struct parameter_pass_trait<float> { using Type = float; };
template<> struct parameter_pass_trait<double> { using Type = double; };

#define DEFINE_GETTER_SETTER(builderClass, name, type) \
public: \
    virtual builderClass& Set##name(typename parameter_pass_trait<type>::Type value) \
    { \
        m##name = value; \
        return *this; \
    } \
    type Get##name() const \
    { \
        return m##name; \
    }

#define DEFINE_BUILDER_PARAM_WITH_DEFAULT_VALUE(builderClass, name, type, defaultValue) \
private: \
    type m##name = defaultValue; \
    DEFINE_GETTER_SETTER(builderClass, name, type)

#define DEFINE_BUILDER_PARAM(builderClass, name, type) \
    private: \
        type m##name{}; \
    DEFINE_GETTER_SETTER(builderClass, name, type)

#define IMPLEMENT_BUILDER(className, baseClass, targetType, params) \
class className : public baseClass \
{ \
public: \
    className(const Configuration& conf) : mConf(conf) {} \
    params; \
public: \
    virtual targetType Build(); \
    Configuration GetConfiguration() const { return mConf; } \
private: \
    Configuration mConf; \
};

#endif
