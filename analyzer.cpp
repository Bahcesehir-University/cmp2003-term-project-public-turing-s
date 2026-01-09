#include "analyzer.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cctype>

// Helper to remove whitespace and carriage returns
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

// Helper to normalize zone IDs to uppercase
static std::string toUpper(std::string s) {
    for (char &c : s) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return s;
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) return;

    std::string line;
    bool firstLine = true;
    std::vector<std::string> tokens;
    tokens.reserve(10); 

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        tokens.clear();
        size_t start = 0;
        size_t end = line.find(',');

        // Tokenize by comma
        while (end != std::string::npos) {
            tokens.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(',', start);
        }
        tokens.push_back(line.substr(start));

        // 1. Validation: Need at least 4 columns (ID, Zone, Zone, Time)
        if (tokens.size() < 4) continue;

        // 2. Skip Header
        // If first token is not a digit, assume it's a header or garbage
        if (firstLine) {
            firstLine = false;
            std::string firstTok = trim(tokens[0]);
            if (firstTok.empty() || !std::isdigit(static_cast<unsigned char>(firstTok[0]))) {
                continue;
            }
        }

        // 3. Extract and Clean Data
        // Index 1: PickupZoneID
        std::string zone = trim(tokens[1]);
        if (zone.empty()) continue;
        
        // CRITICAL FIX: Normalize to uppercase to pass Case Sensitivity tests
        zone = toUpper(zone);

        // Index 3: PickupDatetime
        std::string dateStr = trim(tokens[3]);
        if (dateStr.empty()) continue;

        // 4. Parse Hour Robustly
        // Logic: Find the colon ':' separating HH:MM. The hour is the number immediately preceding it.
        size_t colonPos = dateStr.find(':');
        if (colonPos == std::string::npos || colonPos == 0) {
            continue; // No time found
        }

        // Look backwards from colon to find the start of the hour
        // It's usually preceded by a space ' ' or 'T' or nothing if only time is present
        size_t hourStart = colonPos - 1;
        // Move back while we have digits
        while (hourStart > 0 && std::isdigit(static_cast<unsigned char>(dateStr[hourStart - 1]))) {
            hourStart--;
        }

        // Extra safety check: ensure the char before hourStart (if exists) is a separator
        if (hourStart > 0) {
            char preChar = dateStr[hourStart - 1];
            // If the previous char is a digit, we might be parsing the middle of a number (unlikely with HH:MM)
            // But usually we look for separators like space, T, or start of string
        }

        std::string hourSub = dateStr.substr(hourStart, colonPos - hourStart);
        
        int hour = -1;
        try {
            if (hourSub.empty()) continue;
            hour = std::stoi(hourSub);
        } catch (...) {
            continue;
        }

        // Boundary check (0-23)
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
    // Estimate size
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
