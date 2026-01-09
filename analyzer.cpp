#include "analyzer.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <vector>

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    // Attempt to skip header if present
    // We handle this by checking if the first character of the first token is a digit in the loop
    // But logically, simply reading the first line as a potential header is safer if strict "skip header" is required.
    // However, the most robust way for "dirty" data is to validate every row's format.
    
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Manual tokenization is faster than stringstream
        std::vector<std::string> tokens;
        tokens.reserve(6);
        size_t start = 0;
        size_t end = line.find(',');

        while (end != std::string::npos) {
            tokens.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(',', start);
        }
        tokens.push_back(line.substr(start));

        // Validation 1: Column count
        // Expecting at least: TripID, PickupZone, DropoffZone, PickupTime, ... (4+ columns)
        // Based on file: ID, PickupZone, DropoffZone, Timestamp, Distance, Fare (6 columns)
        if (tokens.size() < 4) {
            continue; // Malformed row
        }

        // Handle Header Row: If first token is not numeric, assume header and skip
        if (firstLine) {
            firstLine = false;
            if (!isdigit(tokens[0][0])) {
                continue; 
            }
        }

        // Extract Key Fields
        // Index 1: PickupZoneID
        // Index 3: PickupDatetime (e.g., "2024-01-01 00:00")
        std::string zone = tokens[1];
        std::string dateStr = tokens[3];

        if (zone.empty() || dateStr.empty()) continue;

        // Extract Hour
        // Format expects "YYYY-MM-DD HH:MM" or similar. We look for the space.
        size_t spacePos = dateStr.find(' ');
        if (spacePos == std::string::npos || spacePos + 2 >= dateStr.size()) {
            continue; // Malformed date format
        }

        // Parse Hour safely
        std::string hourStr = dateStr.substr(spacePos + 1, 2);
        int hour = -1;
        try {
            hour = std::stoi(hourStr);
        } catch (...) {
            continue; // Parsing failed
        }

        if (hour < 0 || hour > 23) {
            continue; // Invalid hour logic
        }

        // Aggregate
        _zoneCounts[zone]++;
        
        // Ensure vector exists
        if (_zoneHourlyCounts.find(zone) == _zoneHourlyCounts.end()) {
            _zoneHourlyCounts[zone] = std::vector<long long>(24, 0);
        }
        _zoneHourlyCounts[zone][hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> results;
    results.reserve(_zoneCounts.size());

    // Flatten map to vector
    for (const auto& kv : _zoneCounts) {
        results.push_back({kv.first, kv.second});
    }

    // Sort: Count DESC, Zone ASC
    std::sort(results.begin(), results.end(), [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; // Higher count first
        }
        return a.zone < b.zone;       // Lexicographical zone string
    });

    // Trim to k
    if (k >= 0 && (size_t)k < results.size()) {
        results.resize(k);
    }

    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> results;
    // Reserve estimation: zones * 24 is max, but usually sparse
    results.reserve(_zoneHourlyCounts.size() * 12); 

    // Flatten nested map/vector to linear vector
    for (const auto& pair : _zoneHourlyCounts) {
        const std::string& zone = pair.first;
        const std::vector<long long>& hours = pair.second;
        
        for (int h = 0; h < 24; ++h) {
            if (hours[h] > 0) {
                results.push_back({zone, h, hours[h]});
            }
        }
    }

    // Sort: Count DESC, Zone ASC, Hour ASC
    std::sort(results.begin(), results.end(), [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; // Higher count first
        }
        if (a.zone != b.zone) {
            return a.zone < b.zone;   // Zone A-Z
        }
        return a.hour < b.hour;       // Hour 0-23
    });

    // Trim to k
    if (k >= 0 && (size_t)k < results.size()) {
        results.resize(k);
    }

    return results;
}
