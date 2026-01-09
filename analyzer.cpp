#include "analyzer.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cctype>

// Helper to remove whitespace and carriage returns (\r)
static std::string trim(const std::string& str) {
    if (str.empty()) return "";
    size_t first = 0;
    while (first < str.size() && std::isspace(static_cast<unsigned char>(str[first]))) {
        first++;
    }
    if (first == str.size()) return "";
    
    size_t last = str.size() - 1;
    while (last > first && std::isspace(static_cast<unsigned char>(str[last]))) {
        last--;
    }
    return str.substr(first, last - first + 1);
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) return;

    std::string line;
    bool firstLine = true;
    
    // Pre-allocate to speed up parsing
    std::vector<std::string> tokens;
    tokens.reserve(10); 

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        tokens.clear();
        size_t start = 0;
        size_t end = line.find(',');

        // Fast tokenization
        while (end != std::string::npos) {
            tokens.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(',', start);
        }
        tokens.push_back(line.substr(start));

        // 1. Validation: Column count (need at least 4)
        if (tokens.size() < 4) continue;

        // 2. Skip Header
        if (firstLine) {
            firstLine = false;
            // If the first char of the first token is NOT a digit, it's likely a header
            std::string firstTok = trim(tokens[0]);
            if (firstTok.empty() || !std::isdigit(static_cast<unsigned char>(firstTok[0]))) {
                continue;
            }
        }

        // 3. Extract Fields
        std::string zone = trim(tokens[1]);     // PickupZone
        std::string dateStr = trim(tokens[3]);  // Timestamp

        if (zone.empty() || dateStr.empty()) continue;

        // 4. Parse Hour
        // Expecting format "YYYY-MM-DD HH:MM"
        size_t spacePos = dateStr.find(' ');
        if (spacePos == std::string::npos || spacePos + 2 >= dateStr.size()) {
            continue;
        }

        int hour = -1;
        try {
            std::string hourSub = dateStr.substr(spacePos + 1, 2);
            // Quick check if numeric
            if (!std::isdigit(static_cast<unsigned char>(hourSub[0]))) continue;
            hour = std::stoi(hourSub);
        } catch (...) {
            continue;
        }

        if (hour < 0 || hour > 23) continue;

        // 5. Aggregate
        _zoneCounts[zone]++;
        
        if (_zoneHourlyCounts.find(zone) == _zoneHourlyCounts.end()) {
            _zoneHourlyCounts[zone] = std::vector<long long>(24, 0);
        }
        _zoneHourlyCounts[zone][hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> results;
    results.reserve(_zoneCounts.size());

    // Explicit construction to fix compiler error
    for (const auto& kv : _zoneCounts) {
        ZoneCount z;
        z.zone = kv.first;
        z.count = kv.second;
        results.push_back(z);
    }

    // Sort: Count DESC, Zone ASC
    std::sort(results.begin(), results.end(), [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; 
        }
        return a.zone < b.zone;
    });

    if (k >= 0 && (size_t)k < results.size()) {
        results.resize(k);
    }
    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> results;
    results.reserve(_zoneHourlyCounts.size() * 5); 

    for (const auto& pair : _zoneHourlyCounts) {
        const std::string& zone = pair.first;
        const std::vector<long long>& hours = pair.second;
        
        for (int h = 0; h < 24; ++h) {
            if (hours[h] > 0) {
                SlotCount s;
                s.zone = zone;
                s.hour = h;
                s.count = hours[h];
                results.push_back(s);
            }
        }
    }

    // Sort: Count DESC, Zone ASC, Hour ASC
    std::sort(results.begin(), results.end(), [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; 
        }
        if (a.zone != b.zone) {
            return a.zone < b.zone;   
        }
        return a.hour < b.hour;       
    });

    if (k >= 0 && (size_t)k < results.size()) {
        results.resize(k);
    }
    return results;
}
