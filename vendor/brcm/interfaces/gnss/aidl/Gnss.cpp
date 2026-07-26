#include "Gnss.h"
#include <android-base/logging.h>

namespace aidl::android::hardware::gnss {

Gnss::Gnss() {
    //LOG(INFO) << "Starting GNSS main function...";
    mNmeaReader = std::make_unique<NmeaReader>();
    mNmeaReader->setLocationCallback([this](const GnssLocation& loc) { reportLocation(loc); });
    mNmeaReader->setNmeaCallback([this](int64_t ts, const std::string& nmea) { reportNmea(ts, nmea); });
    mNmeaReader->setSvStatusCallback([this](const std::vector<IGnssCallback::GnssSvInfo>& svList) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mCallback) {
            std::vector<IGnssCallback::GnssSvInfo> listToPass = svList;
            mCallback->gnssSvStatusCb(listToPass);
        }
    });
}

Gnss::~Gnss() {
    stop();
}

ndk::ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::start() {
    LOG(INFO) << "GNSS main function start...";
    if (mNmeaReader) mNmeaReader->start();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::stop() {
    LOG(INFO) << "GNSS main function stop...";
    if (mNmeaReader) mNmeaReader->stop();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::close() {
    LOG(INFO) << "GNSS main function close...";
    stop();
    return ndk::ScopedAStatus::ok();
}

// Return Exception for unsupportable hardware features
ndk::ScopedAStatus Gnss::injectTime(int64_t, int64_t, int32_t) { 
    //return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();

}
ndk::ScopedAStatus Gnss::injectLocation(const GnssLocation&) { 
    //return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Gnss::injectBestLocation(const GnssLocation&) { 
    //return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::deleteAidingData(IGnss::GnssAidingData aidingDataFlags) {
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    //return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Gnss::setPositionMode(const IGnss::PositionModeOptions& in_options) {
    // Implementation here
    //STANDALONE = 0,   // chip computes its own fix, no network help (ie. u blox7 usb dongle)
    //MS_BASED = 1,     // mobile-station-based AGPS
    //MS_ASSISTED = 2,  // mobile-station-assisted AGPS
    //LOG(INFO) << "GNSS main function setPositionMode... " << static_cast<int>(in_options.mode);
    if (in_options.mode != IGnss::GnssPositionMode::STANDALONE) return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    else return ndk::ScopedAStatus::ok();
}

// --- Extention Stubs (Returning nullptr means unsupported, which is perfectly valid) ---
ndk::ScopedAStatus Gnss::getExtensionAGnss(std::shared_ptr<IAGnss>* _aidl_return) {
    //*_aidl_return = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* _aidl_return) {
    //*_aidl_return = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssAntennaInfo(std::shared_ptr<IGnssAntennaInfo>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssMeasurement(std::shared_ptr<IGnssMeasurementInterface>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssConfiguration(std::shared_ptr<IGnssConfiguration>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssBatching(std::shared_ptr<IGnssBatching>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssGeofence(std::shared_ptr<IGnssGeofence>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssNavigationMessage(std::shared_ptr<IGnssNavigationMessageInterface>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionMeasurementCorrections(std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssVisibilityControl(std::shared_ptr<visibility_control::IGnssVisibilityControl>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssPowerIndication(std::shared_ptr<IGnssPowerIndication>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionPsds(std::shared_ptr<IGnssPsds>* result) {
    //*result = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Gnss::getExtensionGnssAssistanceInterface(std::shared_ptr<gnss_assistance::IGnssAssistanceInterface>* _aidl_return) {
    //*_aidl_return = nullptr; return ndk::ScopedAStatus::ok();
    //return ndk::ScopedAStatus::fromServiceSpecificError(1); // 1 = Not Supported
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

// SvStatus is now supported by our GSV parser!
ndk::ScopedAStatus Gnss::startSvStatus() { 
    // Handled passively by the reader, nothing explicit to turn on.
    return ndk::ScopedAStatus::ok(); 
}
ndk::ScopedAStatus Gnss::stopSvStatus() { 
    return ndk::ScopedAStatus::ok(); 
}

ndk::ScopedAStatus Gnss::startNmea() { return ndk::ScopedAStatus::ok(); }
ndk::ScopedAStatus Gnss::stopNmea() { return ndk::ScopedAStatus::ok(); }

void Gnss::reportLocation(const GnssLocation& location) {
    LOG(INFO) << "GNSS main function reportLocation";
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback) mCallback->gnssLocationCb(location);
}

void Gnss::reportNmea(int64_t timestamp, const std::string& nmea) {
    //LOG(INFO) << "GNSS main function reportNmea";
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback) mCallback->gnssNmeaCb(timestamp, nmea);
}

} // namespace aidl::android::hardware::gnss
