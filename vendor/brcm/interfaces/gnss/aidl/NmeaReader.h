#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include "NmeaParser.h"
#include <aidl/android/hardware/gnss/GnssLocation.h>

class NmeaReader {
public:
    NmeaReader() : mRunning(false) {}
    ~NmeaReader() { stop(); }

    // Lifecycle control
    void start();
    void stop();

    // Callback setters (injected from Gnss.cpp)
    void setLocationCallback(std::function<void(const aidl::android::hardware::gnss::GnssLocation&)> cb) {
        std::lock_guard<std::mutex> lock(mMutex);
        mLocationCallback = cb;
    }
    
    void setNmeaCallback(std::function<void(int64_t, const std::string&)> cb) {
        std::lock_guard<std::mutex> lock(mMutex);
        mNmeaCallback = cb;
    }

    void setSvStatusCallback(std::function<void(const std::vector<aidl::android::hardware::gnss::IGnssCallback::GnssSvInfo>&)> cb) {        std::lock_guard<std::mutex> lock(mMutex);
        mSvStatusCallback = cb;
    }
    
private:
    int64_t getCurrentTime();
    void readLoop();
    std::string findDevice();
    void processParsedData();
    // Puts the serial line into raw mode at the receiver's baud rate.
    // Returns false (and logs) if the fd could not be configured.
    bool configureSerialPort(int fd);

    std::atomic<bool> mRunning;
    std::thread mThread;
    // Serialize lifecycle transitions so repeated start/stop calls cannot
    // replace a still-joinable reader thread.
    std::mutex mLifecycleMutex;
    std::mutex mMutex;
    
    NmeaParser mParser;
    aidl::android::hardware::gnss::GnssLocation mCurrentLocation;

    std::function<void(const aidl::android::hardware::gnss::GnssLocation&)> mLocationCallback;
    std::function<void(int64_t, const std::string&)> mNmeaCallback;
    std::function<void(const std::vector<aidl::android::hardware::gnss::IGnssCallback::GnssSvInfo>&)> mSvStatusCallback;
};
