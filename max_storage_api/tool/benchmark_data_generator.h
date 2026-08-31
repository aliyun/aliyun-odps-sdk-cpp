#pragma once

#include "arrow/api.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cmath>

namespace benchmark_util {

static int64_t random_string_length = 10;
static bool use_fixed_string = false;
static std::string fixed_string = "not_set_yet";

inline void SetRandomStringLength(int64_t length) {
    random_string_length = length;
}

inline int64_t GetRandomStringLength() {
    return random_string_length;
}

inline void SetUseFixedString(bool use_fixed) {
    use_fixed_string = use_fixed;
}

inline bool GetUseFixedString() {
    return use_fixed_string;
}

inline void SetFixedString(const std::string& fixed_str) {
    fixed_string = fixed_str;
}

inline const std::string& GetFixedString() {
    return fixed_string;
}

inline int64_t GenerateBigintValue(int64_t base_val, size_t column_index, size_t row_index) {
    return (base_val + row_index) * (column_index + 1) % 1234567;
}

inline bool GenerateBooleanValue(int64_t base_val, size_t column_index, size_t row_index) {
    return ((2 * (base_val + row_index + column_index)) % 3) == 0;
}

inline float GenerateFloatValue(int64_t base_val, size_t column_index, size_t row_index) {
    float value = ((base_val + row_index) * (column_index + 1)) / 1.2f;
    const float MAX_SAFE_FLOAT = 1e30f;
    if (value > MAX_SAFE_FLOAT) {
        value = std::fmod(value, MAX_SAFE_FLOAT);
    } else if (value < -MAX_SAFE_FLOAT) {
        value = -std::fmod(-value, MAX_SAFE_FLOAT);
    }
    return value;
}

inline std::string GenerateStringValue(int64_t base_val, size_t column_index, size_t row_index) {
    if (use_fixed_string) {
        return fixed_string;
    }
    std::string str;
    str.reserve(random_string_length);
    for (int idx = 0; idx < random_string_length; idx++) {
        char c = 'a' + ((base_val + row_index) * (column_index + idx)) % 26;
        str.push_back(c);
    }
    return str;
}

inline std::string GenerateJsonValue(int64_t base_val, size_t column_index, size_t row_index) {
    if (use_fixed_string) {
        return "{\"" + fixed_string + "\":1}";
    }
    std::string str;
    str.reserve(random_string_length + 8);
    str = "{\"";
    for (int idx = 0; idx < random_string_length; idx++) {
        char c = 'a' + ((base_val + row_index) * (column_index + idx)) % 26;
        str.push_back(c);
    }
    str += "\":1}";
    return str;
}

template <typename T>
void GenerateDataIntoBuilder(arrow::ArrayBuilder* _builder, typename T::value_type value, uint64_t count) {
    T* builder = reinterpret_cast<T*>(_builder);
    std::vector<typename T::value_type> tmpvec;
    tmpvec.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        tmpvec.push_back(value);
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoBigintBuilder(arrow::Int64Builder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    std::vector<int64_t> tmpvec;
    tmpvec.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        tmpvec.push_back(GenerateBigintValue(base_val, column_index, i));
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoBooleanBuilder(arrow::BooleanBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    std::vector<bool> tmpvec;
    tmpvec.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        tmpvec.push_back(GenerateBooleanValue(base_val, column_index, i));
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoFloatBuilder(arrow::FloatBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    std::vector<float> tmpvec;
    tmpvec.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        tmpvec.push_back(GenerateFloatValue(base_val, column_index, i));
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoDoubleBuilder(arrow::DoubleBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    std::vector<double> tmpvec;
    tmpvec.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        double value = static_cast<double>(GenerateFloatValue(base_val, column_index, i));
        tmpvec.push_back(value);
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoStringBuilder(arrow::StringBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    if (use_fixed_string) {
        std::vector<std::string> tmpvec;
        tmpvec.reserve(count);
        tmpvec.assign(count, fixed_string);
        builder->AppendValues(tmpvec);
        return;
    }
    
    std::vector<std::string> tmpvec;
    tmpvec.reserve(count);
    
    std::vector<char> char_buffer(random_string_length);
    
    for (uint64_t i = 0; i < count; i++) {
        for (int idx = 0; idx < random_string_length; idx++) {
            char_buffer[idx] = 'a' + ((base_val + i) * (column_index + idx)) % 26;
        }
        tmpvec.emplace_back(char_buffer.data(), random_string_length);
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoJsonBuilder(arrow::StringBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    if (use_fixed_string) {
        std::string json_str = "{\"" + fixed_string + "\":1}";
        std::vector<std::string> tmpvec;
        tmpvec.reserve(count);
        tmpvec.assign(count, json_str);
        builder->AppendValues(tmpvec);
        return;
    }
    
    std::vector<std::string> tmpvec;
    tmpvec.reserve(count);
    
    std::vector<char> char_buffer(random_string_length + 8);
    char_buffer[0] = '{';
    char_buffer[1] = '"';
    char_buffer[random_string_length + 2] = '"';
    char_buffer[random_string_length + 3] = ':';
    char_buffer[random_string_length + 4] = '1';
    char_buffer[random_string_length + 5] = '}';
    
    for (uint64_t i = 0; i < count; i++) {
        for (int idx = 0; idx < random_string_length; idx++) {
            char_buffer[idx + 2] = 'a' + ((base_val + i) * (column_index + idx)) % 26;
        }
        tmpvec.emplace_back(char_buffer.data(), random_string_length + 6);
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoBinaryBuilder(arrow::BinaryBuilder* builder, size_t column_index, uint64_t count, int64_t base_val) {
    if (use_fixed_string) {
        std::vector<std::string> tmpvec;
        tmpvec.reserve(count);
        tmpvec.assign(count, fixed_string);
        builder->AppendValues(tmpvec);
        return;
    }
    
    std::vector<std::string> tmpvec;
    tmpvec.reserve(count);
    
    std::vector<char> char_buffer(random_string_length);
    
    for (uint64_t i = 0; i < count; i++) {
        for (int idx = 0; idx < random_string_length; idx++) {
            char_buffer[idx] = 'a' + ((base_val + i) * (column_index + idx)) % 26;
        }
        tmpvec.emplace_back(char_buffer.data(), random_string_length);
    }
    builder->AppendValues(tmpvec);
}

void GenerateDataIntoDecimalBuilder(arrow::DecimalBuilder* builder, size_t column_index, uint64_t count, int64_t base_val, int precision = 38, int scale = 2) {
    int64_t max_int_part = 1;
    for (int i = 0; i < (precision - scale); i++) {
        max_int_part *= 10;
    }
    max_int_part -= 1;

    for (uint64_t i = 0; i < count; i++) {
        float raw_value = GenerateFloatValue(base_val, column_index, i);
        float clamped_value = std::fabs(raw_value);
        if (clamped_value > static_cast<float>(max_int_part) + 0.99f) {
            clamped_value = std::fmod(clamped_value, static_cast<float>(max_int_part) + 1.0f);
        }
        int64_t scaled_value = static_cast<int64_t>(clamped_value * std::pow(10, scale));
        arrow::Decimal128 value(scaled_value);
        builder->Append(value);
    }
}

std::shared_ptr<arrow::ArrayBuilder> GenerateArrayBuilderBasedOnTypeInfo(const std::string& type, arrow::MemoryPool* pool) {
    if (type == "tinyint") {
        return std::make_shared<arrow::Int8Builder>(pool);
    } else if (type == "smallint") {
        return std::make_shared<arrow::Int16Builder>(pool);
    } else if (type == "int") {
        return std::make_shared<arrow::Int32Builder>(pool);
    } else if (type == "bigint") {
        return std::make_shared<arrow::Int64Builder>(pool);
    } else if (type == "boolean") {
        return std::make_shared<arrow::BooleanBuilder>(pool);
    } else if (type == "float") {
        return std::make_shared<arrow::FloatBuilder>(pool);
    } else if (type == "double") {
        return std::make_shared<arrow::DoubleBuilder>(pool);
    } else if (type == "string" || type.find("varchar") == 0 || type.find("char") == 0 || type == "json") {
        return std::make_shared<arrow::StringBuilder>(pool);
    } else if (type == "date") {
        return std::make_shared<arrow::Date32Builder>(pool);
    } else if (type == "timestamp") {
        return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), pool);
    } else if (type == "datetime") {
        return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::MILLI), pool);
    } else if (type == "binary") {
        return std::make_shared<arrow::BinaryBuilder>(pool);
    } else if (type.find("decimal") == 0) {
        size_t start = type.find('(');
        size_t comma = type.find(',');
        size_t end = type.find(')');
        if (start != std::string::npos && comma != std::string::npos && end != std::string::npos) {
            int precision = std::stoi(type.substr(start + 1, comma - start - 1));
            int scale = std::stoi(type.substr(comma + 1, end - comma - 1));
            return std::make_shared<arrow::DecimalBuilder>(arrow::decimal(precision, scale), pool);
        }
        return std::make_shared<arrow::DecimalBuilder>(arrow::decimal(38, 2), pool);
    } else if (type.find("array<") == 0) {
        size_t start = type.find('<');
        size_t end = type.find('>');
        if (start != std::string::npos && end != std::string::npos) {
            std::string element_type = type.substr(start + 1, end - start - 1);
            std::shared_ptr<arrow::ArrayBuilder> elementBuilder = GenerateArrayBuilderBasedOnTypeInfo(element_type, pool);
            return std::make_shared<arrow::ListBuilder>(pool, elementBuilder);
        }
    } else if (type.find("map<") == 0) {
        size_t start = type.find('<');
        size_t comma = type.find(',');
        size_t end = type.find('>');
        if (start != std::string::npos && comma != std::string::npos && end != std::string::npos) {
            std::string key_type = type.substr(start + 1, comma - start - 1);
            std::string value_type = type.substr(comma + 1, end - comma - 1);
            std::shared_ptr<arrow::ArrayBuilder> keyBuilder = GenerateArrayBuilderBasedOnTypeInfo(key_type, pool);
            std::shared_ptr<arrow::ArrayBuilder> valueBuilder = GenerateArrayBuilderBasedOnTypeInfo(value_type, pool);
            return std::make_shared<arrow::MapBuilder>(pool, keyBuilder, valueBuilder);
        }
    } else if (type.find("struct<") == 0) {
        throw std::runtime_error("Struct type not fully supported in simplified version");
    }
    
    return std::make_shared<arrow::StringBuilder>(pool);
}

void GenerateDataIntoBuilderBasedOnTypeInfo(arrow::ArrayBuilder* builder, const std::string& type, 
                                            size_t count, size_t column_index, int64_t base_val) {
    int64_t rand_num = rand() % 1234567;
    
    if (type == "tinyint") {
        GenerateDataIntoBuilder<arrow::Int8Builder>(builder, static_cast<int8_t>(rand_num % 128), count);
    } else if (type == "smallint") {
        GenerateDataIntoBuilder<arrow::Int16Builder>(builder, static_cast<int16_t>(rand_num % 32768), count);
    } else if (type == "int") {
        GenerateDataIntoBuilder<arrow::Int32Builder>(builder, static_cast<int32_t>(rand_num), count);
    } else if (type == "bigint") {
        GenerateDataIntoBigintBuilder(reinterpret_cast<arrow::Int64Builder*>(builder), column_index, count, base_val);
    } else if (type == "boolean") {
        GenerateDataIntoBooleanBuilder(reinterpret_cast<arrow::BooleanBuilder*>(builder), column_index, count, base_val);
    } else if (type == "float") {
        GenerateDataIntoFloatBuilder(reinterpret_cast<arrow::FloatBuilder*>(builder), column_index, count, base_val);
    } else if (type == "double") {
        GenerateDataIntoDoubleBuilder(reinterpret_cast<arrow::DoubleBuilder*>(builder), column_index, count, base_val);
    } else if (type == "string" || type.find("varchar") == 0 || type.find("char") == 0) {
        GenerateDataIntoStringBuilder(reinterpret_cast<arrow::StringBuilder*>(builder), column_index, count, base_val);
    } else if (type == "json") {
        GenerateDataIntoJsonBuilder(reinterpret_cast<arrow::StringBuilder*>(builder), column_index, count, base_val);
    } else if (type == "date") {
        GenerateDataIntoBuilder<arrow::Date32Builder>(builder, static_cast<int32_t>(rand_num), count);
    } else if (type == "timestamp" || type == "datetime") {
        GenerateDataIntoBuilder<arrow::TimestampBuilder>(builder, static_cast<int64_t>(rand_num), count);
    } else if (type == "binary") {
        GenerateDataIntoBinaryBuilder(reinterpret_cast<arrow::BinaryBuilder*>(builder), column_index, count, base_val);
    } else if (type.find("decimal") == 0) {
        int precision = 38;
        int scale = 2;
        size_t start = type.find('(');
        size_t comma = type.find(',');
        size_t end = type.find(')');
        if (start != std::string::npos && comma != std::string::npos && end != std::string::npos) {
            precision = std::stoi(type.substr(start + 1, comma - start - 1));
            scale = std::stoi(type.substr(comma + 1, end - comma - 1));
        }
        GenerateDataIntoDecimalBuilder(reinterpret_cast<arrow::DecimalBuilder*>(builder), column_index, count, base_val, precision, scale);
    } else if (type.find("array<") == 0) {
        arrow::ListBuilder* listbuilder = reinterpret_cast<arrow::ListBuilder*>(builder);
        size_t start = type.find('<');
        size_t end = type.find('>');
        if (start != std::string::npos && end != std::string::npos) {
            std::string element_type = type.substr(start + 1, end - start - 1);
            for (uint64_t i = 0; i < count; i++) {
                listbuilder->Append(true);
                GenerateDataIntoBuilderBasedOnTypeInfo(listbuilder->value_builder(), element_type, 20, i, base_val);
            }
        }
    } else if (type.find("map<") == 0) {
        arrow::MapBuilder* mapbuilder = reinterpret_cast<arrow::MapBuilder*>(builder);
        size_t start = type.find('<');
        size_t comma = type.find(',');
        size_t end = type.find('>');
        if (start != std::string::npos && comma != std::string::npos && end != std::string::npos) {
            std::string key_type = type.substr(start + 1, comma - start - 1);
            std::string value_type = type.substr(comma + 1, end - comma - 1);
            for (uint64_t i = 0; i < count; i++) {
                mapbuilder->Append();
                GenerateDataIntoBuilderBasedOnTypeInfo(mapbuilder->key_builder(), key_type, 20, i, base_val);
                GenerateDataIntoBuilderBasedOnTypeInfo(mapbuilder->item_builder(), value_type, 20, i, base_val);
            }
        }
    } else {
        GenerateDataIntoStringBuilder(reinterpret_cast<arrow::StringBuilder*>(builder), column_index, count, base_val);
    }
}

} // namespace benchmark_util

