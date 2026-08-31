#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <string>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <map>
#include <memory>
#include <random>
#include <boost/program_options.hpp>

namespace tool {

class Iota {
public:
    int64_t counter;
    Iota(): counter(0) {}
    int64_t operator()() {
        return counter++;
    }

};

namespace impl {

template <typename T>
struct RangerValue{
    T value;
    RangerValue(T val): value(val) {}
    ~RangerValue() {}
    operator T() const { return value; }
};

template <typename T>
struct RangerIterator {
    T curr;
    T step;
    RangerIterator(T curr, T step): curr(curr), step(step) {};
    ~RangerIterator() {}
    RangerIterator<T> operator++() {
        curr+=step;
        return RangerIterator<T>(curr - step, step);
    }
    bool operator==(const RangerIterator<T>& other) {
        // prevent the step overflow case
        return curr >= other.curr;
    }
    bool operator!=(const RangerIterator<T>& other) {
        // prevent the step overflow case
        return !((*this) == other);
    }
    RangerValue<T> operator*() {
        return RangerValue<T>(curr);
    }
};

template <typename T>
struct Ranger {
    T mem_begin;
    T mem_end;
    T step;
    Ranger(T begin, T end, T step): mem_begin(begin), mem_end(end), step(step) {};
    RangerIterator<T> begin() const {
        return RangerIterator<T>(mem_begin, step);
    }
    RangerIterator<T> end() const {
        return RangerIterator<T>(mem_end, step);
    }
};

}

template <typename T, typename U>
typename impl::Ranger<T> Range(U begin, T end, U step = static_cast<U>(1)) {
    return impl::Ranger<T>(static_cast<T>(begin), end, static_cast<T>(step));
}


const std::string Reinform("\r");


namespace impl {

// trim from start (in place)
static inline void ltrim(std::string &s, int c) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [c](int ch) {
        return ch != c;
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s, int c) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [c](int ch) {
        return ch != c;
    }).base(), s.end());
}

}

static inline std::vector<std::string> StringSplit(const std::string& source, const std::string& delim = " ") {
    std::string tmp = source;
    std::vector<std::string> result;
    while(true) {
        auto position = tmp.find_first_of(delim);
        if (position == std::string::npos) {
            result.push_back(tmp);
            return result;
        }
        result.push_back(tmp.substr(0, position));
        tmp = tmp.substr(position + delim.length());
    }
}

static inline std::string StringTrim(std::string source, char c) {
    impl::ltrim(source, (int)c);
    impl::rtrim(source, (int)c);
    return source;
}

static inline std::string StringHexDump(const std::string& source, const std::string& padding = " ") {
    const static std::array<char, 16> HEX_CHARS = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string result;
    result.reserve(source.length() * (2 + padding.length()));
    for(size_t i = 0; i < source.length(); i++) {
        uint8_t v = source.at(i);
        result += HEX_CHARS[v / 16];
        result += HEX_CHARS[v % 16];
        if (i != source.length() - 1)
        {
            result += padding;
        }
    }
    return result;
}

template <typename T>
class ContiguousRandomizer
{
public:
    virtual T Next(T begin, T end) = 0;
};

namespace internal {

template <typename T, typename U, typename = void>
struct Distributor {
    U Generate(T& randomizer, U begin, U end) {
        return std::uniform_int_distribution<U>(begin, end)(randomizer);
    }
};

template <typename T, typename U>
struct Distributor<T, U, typename std::enable_if<std::is_floating_point<U>::value>::type> {
    U Generate(T& randomizer, U begin, U end) {
        return std::uniform_real_distribution<U>(begin, end)(randomizer);
    }
};

}

template <typename T>
class MT19937ContiguousRandomizer: public ContiguousRandomizer<T>
{
public:
    MT19937ContiguousRandomizer(): mt(std::random_device()()) {}
    virtual T Next(T begin, T end) override { return internal::Distributor<std::mt19937, T>().Generate(mt, begin, end); }
private:
    std::mt19937 mt;
};

template<typename T, typename U>
T Random(U begin, T end) {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<T> dist(static_cast<T>(begin), end);
    return dist(mt);
}

template<typename T, typename U>
T ContiguousRandom(ContiguousRandomizer<T>& crand, U begin, T end) {
    return crand.Next(static_cast<T>(begin), end);
}

template <typename T, typename U>
T RandomFloat(U begin, T end) {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_real_distribution<T> dist(static_cast<T>(begin), end);
    return dist(mt);
}

template<typename T>
T RandomIterator(const T& begin, const T& end) {
    auto size = std::distance(begin, end);
    if (size == 0) {
        return end;
    }
    return std::next(begin, Random(0, size - 1));
}

template<typename T>
T ContiguousRandomIterator(
    ContiguousRandomizer<typename std::iterator_traits<T>::difference_type>& crand,
    const T& begin, const T& end) {
    auto size = std::distance(begin, end);
    if (size == 0) {
        return end;
    }
    return std::next(begin, crand.Next(0, size - 1));
}

const std::string DefaultCharacterSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

// for strings
static inline std::string RandomString(size_t length, const std::string& characterSet = DefaultCharacterSet){
    std::string buffer;
    for(auto _ignore: Range(0, length)) {
        buffer += *RandomIterator(characterSet.begin(), characterSet.end());
    }
    return buffer;
}

using ContiguousRandomizerStringDiffType = std::iterator_traits<std::string::const_iterator>::difference_type;
static inline std::string ContiguousRandomString(
    ContiguousRandomizer<ContiguousRandomizerStringDiffType>& crand,
    size_t length, const std::string& characterSet = DefaultCharacterSet){
    std::string buffer;
    for(auto _ignore: Range(0, length)) {
        buffer += *ContiguousRandomIterator(crand, characterSet.begin(), characterSet.end());
    }
    return buffer;
}

static inline uint64_t CurrentTimeMillis() {
    struct timespec currtm;
    clock_gettime(CLOCK_REALTIME, &currtm);
    uint64_t k = currtm.tv_sec * 1000;
    return k + currtm.tv_nsec / 1000000;
}

template <typename... Ts>
std::string MakeString(Ts&&... vals) {
    std::ostringstream ss;
    int dummy[] = {0, ((ss << vals << " "), 0)...};
    static_cast<void>(dummy);
    std::string tmp = ss.str();
    return tmp.substr(0, tmp.size() - 1);
}

template <typename... Ts>
void Println(Ts&&... vals) {
    std::cout << MakeString(vals...) << std::endl;
}

template <typename... Ts>
void Informln(Ts&&... vals) {
    std::cerr << MakeString(vals...) << Reinform;
}

template <typename... Ts>
void Errorln(Ts&&... vals) {
    std::cerr << MakeString(vals...) << std::endl;
}

template <typename... Ts>
void ParaErrorln(Ts&&... vals) {
    static std::mutex protection;
    std::lock_guard<std::mutex> _guard(protection);
    std::cerr << MakeString(vals...) << std::endl;
}

template <typename T, typename... Ts>
bool EqualOneOf(const T& t1, Ts&&... vals)
{
    bool equaled = false;
    int dummy[] = {0, ((equaled |= (t1 == vals)), 0)...};
    static_cast<void>(dummy);
    return equaled;
}

static inline std::string DoubleFormatter(double d)
{
    char tmp[1024] = {0};
    snprintf(tmp, 1023, "%.2lf", d);
    return std::string(tmp, strnlen(tmp, 1024));
}

static inline std::string DoubleSizeUnitFormatter(double d, std::string UNIT_SUFFIX="/s")
{
    std::string unit = "B";
    if (d > 1024)
    {
        d = d / 1024.0;
        unit = "KB";
    }
    if (d > 1024)
    {
        d = d / 1024.0;
        unit = "MB";
    }
    if (d > 1024)
    {
        d = d / 1024.0;
        unit = "GB";
    }
    return DoubleFormatter(d) + unit + UNIT_SUFFIX;
}

struct Averager
{
    int64_t mTotal = 0;
    int64_t mCount = 0;
    std::mutex mLock;

    int64_t Report(int64_t v)
    {
        std::lock_guard<std::mutex> _G(mLock);
        mTotal += v;
        mCount ++;
        return mCount == 0 ? 0 : mTotal / mCount;
    }
};

template<typename T, typename... CreatorArgs>
class ClassRegistry
{
public:
    static ClassRegistry<T, CreatorArgs...>& GetInstance()
    {
        static ClassRegistry<T, CreatorArgs...> _registry;
        return _registry;
    }

    using ObjectCreator = std::function<std::shared_ptr<T>(CreatorArgs...)>;

    static std::string GetTypeName()
    {
        static std::string TYPE_NAME = typeid(T).name();
        return TYPE_NAME;
    }

    template <typename ClassToRegister>
    void Register(const std::string& name)
    {
        mRegistry[name] = [](CreatorArgs... args){ return std::make_shared<ClassToRegister>(args...); };
    }

    std::shared_ptr<T> Create(const std::string& name, CreatorArgs... args) const
    {
        auto it = mRegistry.find(name);
        if (it == mRegistry.end())
        {
            throw std::runtime_error("specified tool not found");
        }
        return it->second(args...);
    }

    std::vector<std::string> NameAll() const
    {
        std::vector<std::string> keys;
        for (auto it: mRegistry)
        {
            keys.push_back(it.first);
        }
        return keys;
    }

private:
    std::map<std::string, ObjectCreator> mRegistry;
};

template <typename ThisClassRegistry, typename ClassToRegister>
class ClassRegistryRegisterHelper
{
public:
    ClassRegistryRegisterHelper(const std::string& name)
    {
        ThisClassRegistry::GetInstance().template Register<ClassToRegister>(name);
    }
};

#define CLASS_REGISTRY_REGISTER(REGISTRY_CLASS, TARGET_CLASS) \
    const static tool::ClassRegistryRegisterHelper<REGISTRY_CLASS, TARGET_CLASS> _ClassRegistry_##TARGET_CLASS(#TARGET_CLASS)

struct Tool
{
    using Registry = ClassRegistry<Tool>;
    virtual ~Tool() {}
    virtual std::string GetName() const = 0;
    virtual int Run(int argc, char* argv[]) = 0;
    virtual std::string GetHelpMessage() const = 0;
};

// 工具注册宏
#define TOOL_REGISTER(TARGET_CLASS) \
    const static tool::ClassRegistryRegisterHelper<tool::Tool::Registry, TARGET_CLASS> _ToolRegistry_##TARGET_CLASS(#TARGET_CLASS)

}
