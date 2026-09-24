/*
 * ConfigParser — Simple INI-style configuration file parser
 *
 * Format:
 *   # comment
 *   [section]
 *   key = value
 *
 * Usage:
 *   EtherDB::ConfigParser cfg;
 *   cfg.load("/etc/etherdb/etherdb.cfg");
 *   std::string logDir = cfg.get("log", "logDir", "/var/log/etherdb/etherdb_log");
 *   int port = cfg.getInt("server", "shellPort", 7040);
 */

#ifndef __EtherDB_ConfigParser_H_
#define __EtherDB_ConfigParser_H_

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace EtherDB {

class ConfigParser {
public:
    ConfigParser() = default;

    // Load configuration from file. Returns true on success.
    bool load(const std::string& filePath) {
        _filePath = filePath;
        std::ifstream ifs(filePath);
        if (!ifs) return false;

        _sections.clear();
        std::string currentSection = "default";
        std::string line;

        while (std::getline(ifs, line)) {
            // Trim whitespace
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            // Section header: [section]
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.size() - 2);
                continue;
            }

            // Key = Value
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = trim(line.substr(0, eqPos));
            std::string val = trim(line.substr(eqPos + 1));

            // Remove surrounding quotes
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                     (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }

            std::string sectionKey = currentSection + "." + key;
            _sections[currentSection][key] = val;
        }
        return true;
    }

    // Get string value with default
    std::string get(const std::string& section, const std::string& key,
                    const std::string& defaultValue = "") const {
        auto secIt = _sections.find(section);
        if (secIt == _sections.end()) return defaultValue;
        auto kvIt = secIt->second.find(key);
        if (kvIt == secIt->second.end()) return defaultValue;
        return kvIt->second;
    }

    // Get int value with default
    int getInt(const std::string& section, const std::string& key,
               int defaultValue = 0) const {
        std::string v = get(section, key);
        if (v.empty()) return defaultValue;
        return std::atoi(v.c_str());
    }

    // Get bool value with default
    bool getBool(const std::string& section, const std::string& key,
                 bool defaultValue = false) const {
        std::string v = get(section, key);
        if (v.empty()) return defaultValue;
        std::string l = v;
        std::transform(l.begin(), l.end(), l.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return (l == "true" || l == "1" || l == "yes" || l == "on");
    }

    // Get float value with default
    float getFloat(const std::string& section, const std::string& key,
                   float defaultValue = 0.0f) const {
        std::string v = get(section, key);
        if (v.empty()) return defaultValue;
        return (float)std::atof(v.c_str());
    }

    // Check if a section exists
    bool hasSection(const std::string& section) const {
        return _sections.find(section) != _sections.end();
    }

    // Check if a key exists in a section
    bool hasKey(const std::string& section, const std::string& key) const {
        auto it = _sections.find(section);
        if (it == _sections.end()) return false;
        return it->second.find(key) != it->second.end();
    }

    const std::string& filePath() const { return _filePath; }

private:
    static std::string trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
        size_t end = s.size();
        while (end > start && std::isspace((unsigned char)s[end - 1])) --end;
        return s.substr(start, end - start);
    }

    std::string _filePath;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> _sections;
};

} // namespace EtherDB

#endif // __EtherDB_ConfigParser_H_
