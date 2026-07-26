#include "NmeaParser.h"
#include <android-base/logging.h>
#include <cmath>
#include <cstring>
#include <time.h>

static bool parseInt(const std::string& s, int& value) {
    if (s.empty()) return false;

    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);

    if (*end != '\0')
        return false;

    value = static_cast<int>(v);
    return true;
}

static bool parseFloat(const std::string& s, float& value) {
    if (s.empty()) return false;

    char* end = nullptr;
    value = std::strtof(s.c_str(), &end);

    return *end == '\0';
}

static bool parseDouble(const std::string& s, double& value) {
    if (s.empty()) return false;

    char* end = nullptr;
    value = std::strtod(s.c_str(), &end);

    return *end == '\0';
}

void NmeaParser::append(const char* data, size_t length) {
    mBuffer.append(data, length);
}

std::vector<std::string> NmeaParser::getReadySentences() {
    std::vector<std::string> sentences;
    size_t pos = 0;
    
    // Extract complete sentences ending in \r\n
    while ((pos = mBuffer.find('\n')) != std::string::npos) {
        std::string sentence = mBuffer.substr(0, pos);
        mBuffer.erase(0, pos + 1);

        if (!sentence.empty() && sentence.back() == '\r') {
            sentence.pop_back();
        }
        
        if (!sentence.empty() && sentence[0] == '$') {
            sentences.push_back(sentence);
        }

        // Strip leading garbage if a byte got dropped causing missed synchronization
        auto dollarPos = mBuffer.find('$');

        if (dollarPos != std::string::npos && dollarPos > 0) {
            mBuffer.erase(0, dollarPos);
        }
    }
    
    // Prevent runaway memory leaks if the serial port spews endless garbage without \r\n
    if (mBuffer.size() > 4096) {
        LOG(WARNING) << "NMEA buffer overflow. Clearing garbage data.";
        LOG(ERROR)
                << "Overflow size=" << mBuffer.size()
                << " tail="
                << mBuffer.substr(
                    mBuffer.size() > 128 ? mBuffer.size()-128 : 0);
        mBuffer.clear();
    }
    
    return sentences;
}

bool NmeaParser::verifyChecksum(const std::string& sentence) {
    // 1. Minimum length bounds check (e.g., "$GPGGA*00")
    if (sentence.length() < 9 || sentence[0] != '$') return false;

    // 2. Locate the checksum delimiter
    size_t asteriskPos = sentence.find('*');
    if (asteriskPos == std::string::npos || asteriskPos + 2 >= sentence.length()) {
        return false; 
    }

    // 3. XOR all characters between '$' and '*'
    uint8_t calculatedChecksum = 0;
    for (size_t i = 1; i < asteriskPos; ++i) {
        calculatedChecksum ^= sentence[i];
    }

    // 4. Exception-free Hex parsing to prevent HAL crashes on malformed serial strings
    const char* hexChars = sentence.c_str() + asteriskPos + 1;
    char hexBuffer[3] = {hexChars[0], hexChars[1], '\0'};
    char* endPtr;
    long providedChecksum = std::strtol(hexBuffer, &endPtr, 16);

    if (*endPtr != '\0') {
        return false; // Contained non-hex characters
    }

    return calculatedChecksum == static_cast<uint8_t>(providedChecksum);
}

std::vector<std::string> NmeaParser::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;
    while ((end = s.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

double NmeaParser::parseCoordinate(const std::string& coord, const std::string& dir) {
    if (coord.empty() || dir.empty()) return 0.0;
    
    // NMEA format: DDDMM.MMMMM
    size_t dotPos = coord.find('.');
    if (dotPos == std::string::npos || dotPos < 2) return 0.0;

    int degrees;
    double minutes;

    if (!parseInt(coord.substr(0, dotPos - 2), degrees))
        return 0.0;

    if (!parseDouble(coord.substr(dotPos - 2), minutes))
        return 0.0;
    double decimalDegrees = degrees + (minutes / 60.0);

    if (dir == "S" || dir == "W") {
        decimalDegrees = -decimalDegrees;
    }
    return decimalDegrees;
}

int64_t NmeaParser::parseUtcToEpochMs(const std::string& timeStr, const std::string& dateStr) {
    if (timeStr.length() < 6 || dateStr.length() != 6) return 0;

    int day, month, year, hour, min, sec;
    if (!parseInt(dateStr.substr(0, 2), day) ||
        !parseInt(dateStr.substr(2, 2), month) ||
        !parseInt(dateStr.substr(4, 2), year) ||
        !parseInt(timeStr.substr(0, 2), hour) ||
        !parseInt(timeStr.substr(2, 2), min) ||
        !parseInt(timeStr.substr(4, 2), sec)) {
        return 0;  // malformed field - avoid std::stoi throwing on corrupted serial data
    }

    struct tm t = {};
    t.tm_mday = day;
    t.tm_mon = month - 1;
    t.tm_year = year + (year > 80 ? 1900 : 2000) - 1900;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;

    // timegm converts UTC struct tm to time_t
    return static_cast<int64_t>(timegm(&t)) * 1000LL; 
}

bool NmeaParser::parseSentence(const std::string& sentence, GnssLocation& location) {
    std::string data = sentence;
    
    // For many NMEA receivers (especially u-blox), the last SNR field of the final satellite often looks
    // like 42*7C instead of 42 - this will cause a std::invalid_argument: stof: no conversion error - so remove *
    size_t star = data.find('*');
    if (star != std::string::npos) {
        data.erase(star);
    }

    std::vector<std::string> tokens = split(data, ',');
    if (tokens.empty()) return false;

    const std::string& type = tokens[0];

    if (type == "$GPRMC" || type == "$GNRMC") {
        if (tokens.size() < 10 || tokens[2] != "A") return false; // V = Invalid, A = Active
        
        mLatestDate = tokens[9]; // DDMMYY
        mHasDate = true;
     
        if (!tokens[1].empty()) {
            location.timestampMillis = parseUtcToEpochMs(tokens[1], mLatestDate);
        }

        location.gnssLocationFlags |= GnssLocation::HAS_LAT_LONG;
        location.latitudeDegrees = parseCoordinate(tokens[3], tokens[4]);
        location.longitudeDegrees = parseCoordinate(tokens[5], tokens[6]);

        // Speed (knots to m/s)
        if (!tokens[7].empty()) {
            double knots;
            if (parseDouble(tokens[7], knots)) {
                location.gnssLocationFlags |= GnssLocation::HAS_SPEED;
                location.speedMetersPerSec = knots * 0.514444;
            }
        }

        // Bearing
        if (!tokens[8].empty()) {
            double bearing;
            if (parseDouble(tokens[8], bearing)) {
                location.gnssLocationFlags |= GnssLocation::HAS_BEARING;
                location.bearingDegrees = bearing;
            }
        }
        return true; // RMC dictates a valid 2D fix
    } 
    else if (type == "$GPGGA" || type == "$GNGGA") {
        if (tokens.size() < 10 || tokens[6] == "0") return false; // 0 = Fix not available

        location.gnssLocationFlags |= GnssLocation::HAS_LAT_LONG;
        location.latitudeDegrees = parseCoordinate(tokens[2], tokens[3]);
        location.longitudeDegrees = parseCoordinate(tokens[4], tokens[5]);

        // Altitude
        if (!tokens[9].empty()) {
            double alt;
            if (parseDouble(tokens[9], alt)) {
                location.gnssLocationFlags |= GnssLocation::HAS_ALTITUDE;
                location.altitudeMeters = alt;
            }
        }

        // GGA carries its own UTC time (field 1) but no date field. If we've
        // already seen a date from a prior RMC sentence, use it as a fallback
        // so timestamps stay fresh even on a receiver/config that emits GGA
        // without RMC.
        if (mHasDate && !tokens[1].empty()) {
            int64_t ts = parseUtcToEpochMs(tokens[1], mLatestDate);
            if (ts > 0) {
                location.timestampMillis = ts;
            }
        }
        return true; 
    }
    else if (type == "$GPGSA" || type == "$GNGSA") {
        if (tokens.size() > 16 && !tokens[16].empty()) {
            // Update the internal HDOP state (Horizontal Dilution of Precision)
            float hdop;
            if (parseFloat(tokens[16], hdop)) {
                mCurrentHdop = hdop;
            }
        }
        return false; // GSA doesn't trigger a location update by itself
    }

    return false;
}

bool NmeaParser::parseSvStatus(const std::string& sentence, std::vector<GnssSvInfo>& svInfoList) {
    std::string data = sentence;
    
    // For many NMEA receivers (especially u-blox), the last SNR field of the final satellite often looks
    // like 42*7C instead of 42 - this will cause a std::invalid_argument: stof: no conversion error - so remove *
    size_t star = data.find('*');
    if (star != std::string::npos) {
        data.erase(star);
    }

    std::vector<std::string> tokens = split(data, ',');
    if (tokens.size() < 4) return false;

    const std::string& type = tokens[0];
    
    // Look for GPS, GLONASS, or Galileo SV sentences
    if (type == "$GPGSV" || type == "$GLGSV" || type == "$GAGSV") {
        int totalMsgs, msgNum;
        if (!parseInt(tokens[1], totalMsgs) || !parseInt(tokens[2], msgNum)) {
            return false;  // malformed sequence header - drop the sentence
        }

        if (msgNum == 1) {
            mSvBuffer.clear(); // Start of a new batch
        }

        // Loop through the 4 possible SVs in this sentence
        for (size_t i = 4; i < tokens.size() && i + 3 < tokens.size(); i += 4) {
            if (tokens[i].empty()) continue; // No SV ID

            int svid;
            if (!parseInt(tokens[i], svid)) continue; // malformed SV ID, skip this entry

            GnssSvInfo sv = {};
            sv.svid = svid;

            float elevation = 0.0f, azimuth = 0.0f, cn0 = 0.0f;
            if (!tokens[i+1].empty()) parseFloat(tokens[i+1], elevation);
            if (!tokens[i+2].empty()) parseFloat(tokens[i+2], azimuth);
            if (!tokens[i+3].empty()) parseFloat(tokens[i+3], cn0);
            sv.elevationDegrees = elevation;
            sv.azimuthDegrees = azimuth;
            sv.cN0Dbhz = cn0;
            
            // Constellation mapping based on Talker ID
            if (type == "$GPGSV") sv.constellation = aidl::android::hardware::gnss::GnssConstellationType::GPS;
            else if (type == "$GLGSV") sv.constellation = aidl::android::hardware::gnss::GnssConstellationType::GLONASS;
            else if (type == "$GAGSV") sv.constellation = aidl::android::hardware::gnss::GnssConstellationType::GALILEO;
            else sv.constellation = aidl::android::hardware::gnss::GnssConstellationType::UNKNOWN;

            // svFlag is for Ephemeris/Almanac/UsedInFix in AIDL v6. 
            // GSV doesn't provide these directly, so we safely leave it at 0.
            sv.svFlag = 0; 
            
            mSvBuffer.push_back(sv);
        }

        // Safety cap: if a corrupted stream causes msgNum to never reach
        // totalMsgs, don't let this grow without bound across calls.
        constexpr size_t kMaxReasonableSvCount = 128;
        if (mSvBuffer.size() > kMaxReasonableSvCount) {
            LOG(WARNING) << "GNSS SV buffer exceeded sane limit, clearing.";
            mSvBuffer.clear();
            return false;
        }

        // If this is the last message in the sequence, emit the data
        if (msgNum == totalMsgs) {
            svInfoList = mSvBuffer;
            return true;
        }
    }
    return false;
}
