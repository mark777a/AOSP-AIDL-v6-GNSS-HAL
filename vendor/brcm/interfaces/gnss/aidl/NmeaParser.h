#pragma once

#include <string>
#include <vector>
#include <aidl/android/hardware/gnss/GnssLocation.h>
#include <aidl/android/hardware/gnss/IGnssCallback.h>

using aidl::android::hardware::gnss::GnssLocation;
using aidl::android::hardware::gnss::IGnssCallback;
using GnssSvInfo = aidl::android::hardware::gnss::IGnssCallback::GnssSvInfo;

class NmeaParser {
public:
    NmeaParser() : mCurrentHdop(1.0f), mHasDate(false) {}

    // Appends raw serial bytes and extracts completed NMEA sentences
    void append(const char* data, size_t length);
    std::vector<std::string> getReadySentences();

    // The hardened checksum validator
    bool verifyChecksum(const std::string& sentence);
    
    // Returns true if a valid Location fix is ready
    bool parseSentence(const std::string& sentence, GnssLocation& location);
    
    // Parses NMEA strings into the Android GnssLocation struct
    // Returns true if a complete batch of SvStatus data is ready
    bool parseSvStatus(const std::string& sentence, std::vector<GnssSvInfo>& svInfoList);

    float getCurrentHdop() const { return mCurrentHdop; }

private:
    std::string mBuffer;
    float mCurrentHdop;
    std::string mLatestDate; 
    bool mHasDate;
    
    // SV buffering
    std::vector<GnssSvInfo> mSvBuffer;

    std::vector<std::string> split(const std::string& s, char delimiter);
    double parseCoordinate(const std::string& coord, const std::string& dir);
    int64_t parseUtcToEpochMs(const std::string& timeStr, const std::string& dateStr);
};