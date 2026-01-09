#include "analyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        return; // Gracefully handle missing file
    }
    
    std::string line;
    // Skip header
    if (!std::getline(file, line)) {
        return; // Empty file
    }
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string tripID, pickupZone, dropoffZone, pickupDateTime, distance, fare;
        
        // Parse CSV: TripID, PickupZoneID, DropoffZoneID, PickupDateTime, DistanceKm, FareAmount
        if (!std::getline(ss, tripID, ',')) continue;
        if (!std::getline(ss, pickupZone, ',')) continue;
        if (!std::getline(ss, dropoffZone, ',')) continue;
        if (!std::getline(ss, pickupDateTime, ',')) continue;
        
        // Trim whitespace from pickupZone
        pickupZone.erase(0, pickupZone.find_first_not_of(" \t\r\n"));
        pickupZone.erase(pickupZone.find_last_not_of(" \t\r\n") + 1);
        
        // Trim whitespace from pickupDateTime
        pickupDateTime.erase(0, pickupDateTime.find_first_not_of(" \t\r\n"));
        pickupDateTime.erase(pickupDateTime.find_last_not_of(" \t\r\n") + 1);
        
        // Skip if zone is empty
        if (pickupZone.empty() || pickupDateTime.empty()) continue;
        
        // Extract hour from PickupDateTime (format: YYYY-MM-DD HH:MM)
        // Find the space, then parse the hour
        size_t spacePos = pickupDateTime.find(' ');
        if (spacePos == std::string::npos) continue;
        
        std::string timePart = pickupDateTime.substr(spacePos + 1);
        if (timePart.length() < 2) continue;
        
        // Extract hour (first two characters after space)
        std::string hourStr = timePart.substr(0, 2);
        
        int hour = -1;
        try {
            hour = std::stoi(hourStr);
        } catch (...) {
            continue; // Invalid hour format
        }
        
        // Validate hour range
        if (hour < 0 || hour > 23) continue;
        
        // Aggregate counts
        zoneTrips[pickupZone]++;
        slotTrips[pickupZone][hour]++;
    }
    
    file.close();
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    // Convert map to vector
    std::vector<ZoneCount> results;
    results.reserve(zoneTrips.size());
    
    for (const auto& pair : zoneTrips) {
        results.push_back({pair.first, pair.second});
    }
    
    // Sort: count desc, zone asc
    std::sort(results.begin(), results.end(), [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; // Descending by count
        }
        return a.zone < b.zone; // Ascending by zone ID
    });
    
    // Return first k elements
    if (results.size() > static_cast<size_t>(k)) {
        results.resize(k);
    }
    
    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    // Convert nested map to vector
    std::vector<SlotCount> results;
    
    for (const auto& zonePair : slotTrips) {
        const std::string& zone = zonePair.first;
        for (const auto& hourPair : zonePair.second) {
            int hour = hourPair.first;
            long long count = hourPair.second;
            results.push_back({zone, hour, count});
        }
    }
    
    // Sort: count desc, zone asc, hour asc
    std::sort(results.begin(), results.end(), [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; // Descending by count
        }
        if (a.zone != b.zone) {
            return a.zone < b.zone; // Ascending by zone
        }
        return a.hour < b.hour; // Ascending by hour
    });
    
    // Return first k elements
    if (results.size() > static_cast<size_t>(k)) {
        results.resize(k);
    }
    
    return results;
}
