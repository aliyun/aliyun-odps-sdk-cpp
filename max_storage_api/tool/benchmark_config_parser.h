#pragma once

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace benchmark_util {

// ============================================================================
// Benchmark Config Data Structures
//
// Two-level structure:
//   [global]       - connection info (endpoint, ak, as, project, etc.)
//   [suite:NAME]   - a complete test suite with all params (table, mode, thread, rows, etc.)
//
// Suite params override global params with the same key.
// ============================================================================

struct BenchmarkSuiteEntry
{
    std::string name;                              // suite name from [suite:NAME]
    std::map<std::string, std::string> params;     // merged params: global + suite
};

struct BenchmarkFileConfig
{
    std::map<std::string, std::string> globalParams;       // [global] params
    std::vector<BenchmarkSuiteEntry> suites;               // all suites
};

// ============================================================================
// Helper: trim whitespace
// ============================================================================
inline std::string TrimString(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ============================================================================
// Helper: split string by delimiter
// ============================================================================
inline std::vector<std::string> SplitString(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream stream(s);
    std::string token;
    while (std::getline(stream, token, delimiter))
    {
        std::string trimmed = TrimString(token);
        if (!trimmed.empty())
        {
            tokens.push_back(trimmed);
        }
    }
    return tokens;
}

// ============================================================================
// Parse INI config file
// ============================================================================
inline BenchmarkFileConfig ParseBenchmarkConfig(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open config file: " + filepath);
    }

    BenchmarkFileConfig config;

    enum class Section { NONE, GLOBAL, SUITE };
    Section currentSection = Section::NONE;
    int currentSuiteIdx = -1;
    // Temporary storage for suite-specific params (before merging with global)
    std::vector<std::map<std::string, std::string>> suiteOwnParams;

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line))
    {
        lineNum++;
        line = TrimString(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        // Section header
        if (line[0] == '[' && line.back() == ']')
        {
            std::string sectionName = line.substr(1, line.size() - 2);
            sectionName = TrimString(sectionName);

            if (sectionName == "global")
            {
                currentSection = Section::GLOBAL;
                currentSuiteIdx = -1;
            }
            else if (sectionName.substr(0, 6) == "suite:")
            {
                std::string suiteName = TrimString(sectionName.substr(6));
                if (suiteName.empty())
                {
                    throw std::runtime_error("Empty suite name at line " + std::to_string(lineNum));
                }

                BenchmarkSuiteEntry suite;
                suite.name = suiteName;
                config.suites.push_back(suite);
                suiteOwnParams.push_back(std::map<std::string, std::string>());
                currentSuiteIdx = static_cast<int>(config.suites.size() - 1);
                currentSection = Section::SUITE;
            }
            else
            {
                throw std::runtime_error("Unknown section type at line " + std::to_string(lineNum)
                                         + ": " + line + " (expected [global] or [suite:NAME])");
            }
            continue;
        }

        // Key = value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
        {
            throw std::runtime_error("Invalid line (no '=') at line " + std::to_string(lineNum) + ": " + line);
        }

        std::string key = TrimString(line.substr(0, eqPos));
        std::string value = TrimString(line.substr(eqPos + 1));

        if (key.empty())
        {
            throw std::runtime_error("Empty key at line " + std::to_string(lineNum));
        }

        switch (currentSection)
        {
            case Section::GLOBAL:
                config.globalParams[key] = value;
                break;
            case Section::SUITE:
                if (currentSuiteIdx >= 0)
                {
                    suiteOwnParams[currentSuiteIdx][key] = value;
                }
                break;
            case Section::NONE:
                throw std::runtime_error("Key-value pair outside of any section at line "
                                         + std::to_string(lineNum));
        }
    }

    // Merge: for each suite, params = global + suite (suite overrides global)
    for (size_t i = 0; i < config.suites.size(); i++)
    {
        // Start with global params
        config.suites[i].params = config.globalParams;
        // Overlay suite-specific params
        for (const auto& kv : suiteOwnParams[i])
        {
            config.suites[i].params[kv.first] = kv.second;
        }
    }

    // Validate
    if (config.suites.empty())
    {
        throw std::runtime_error("Config file contains no test suites");
    }

    return config;
}

// ============================================================================
// Parameter access helpers
// ============================================================================

inline std::string GetParam(const std::map<std::string, std::string>& params,
                            const std::string& key,
                            const std::string& defaultValue = "")
{
    auto it = params.find(key);
    if (it != params.end())
    {
        return it->second;
    }
    return defaultValue;
}

inline int GetParamInt(const std::map<std::string, std::string>& params,
                       const std::string& key, int defaultValue)
{
    auto it = params.find(key);
    if (it != params.end() && !it->second.empty())
    {
        try { return std::stoi(it->second); }
        catch (...) { return defaultValue; }
    }
    return defaultValue;
}

inline int64_t GetParamInt64(const std::map<std::string, std::string>& params,
                             const std::string& key, int64_t defaultValue)
{
    auto it = params.find(key);
    if (it != params.end() && !it->second.empty())
    {
        try { return std::stoll(it->second); }
        catch (...) { return defaultValue; }
    }
    return defaultValue;
}

inline double GetParamDouble(const std::map<std::string, std::string>& params,
                             const std::string& key, double defaultValue)
{
    auto it = params.find(key);
    if (it != params.end() && !it->second.empty())
    {
        try { return std::stod(it->second); }
        catch (...) { return defaultValue; }
    }
    return defaultValue;
}

inline bool GetParamBool(const std::map<std::string, std::string>& params,
                         const std::string& key, bool defaultValue)
{
    auto it = params.find(key);
    if (it != params.end() && !it->second.empty())
    {
        const std::string& v = it->second;
        if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
        if (v == "false" || v == "0" || v == "no" || v == "off") return false;
    }
    return defaultValue;
}

} // namespace benchmark_util
