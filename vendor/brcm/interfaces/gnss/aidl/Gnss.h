#pragma once

#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/IGnss.h>
#include <aidl/android/hardware/gnss/IGnssAntennaInfo.h>
#include <aidl/android/hardware/gnss/IGnssConfiguration.h>
#include <aidl/android/hardware/gnss/IGnssMeasurementInterface.h>
#include <aidl/android/hardware/gnss/IGnssPowerIndication.h>
#include <aidl/android/hardware/gnss/IGnssDebug.h>
#include <aidl/android/hardware/gnss/IGnssBatching.h>
#include <aidl/android/hardware/gnss/IGnssGeofence.h>
#include <aidl/android/hardware/gnss/IGnssNavigationMessageInterface.h>
#include <aidl/android/hardware/gnss/IGnssPsds.h>
#include <aidl/android/hardware/gnss/GnssLocation.h>
#include <aidl/android/hardware/gnss/visibility_control/IGnssVisibilityControl.h>
#include <aidl/android/hardware/gnss/measurement_corrections/IMeasurementCorrectionsInterface.h>
#include <aidl/android/hardware/gnss/gnss_assistance/IGnssAssistanceInterface.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "NmeaReader.h"

namespace aidl::android::hardware::gnss {

class Gnss : public BnGnss {
public:
    Gnss();
    ~Gnss();

    // Mandatory IGnss methods
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& callback) override;
    ndk::ScopedAStatus start() override;
    ndk::ScopedAStatus stop() override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs) override;
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus deleteAidingData(IGnss::GnssAidingData aidingDataFlags) override;
    ndk::ScopedAStatus setPositionMode(const IGnss::PositionModeOptions& in_options) override;

    // Standard AIDL Extension methods
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(std::shared_ptr<IGnssAntennaInfo>* result) override;
    ndk::ScopedAStatus getExtensionGnssMeasurement(std::shared_ptr<IGnssMeasurementInterface>* result) override;
    ndk::ScopedAStatus getExtensionGnssConfiguration(std::shared_ptr<IGnssConfiguration>* result) override;
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* result) override;
    ndk::ScopedAStatus getExtensionGnssBatching(std::shared_ptr<IGnssBatching>* result) override;
    ndk::ScopedAStatus getExtensionGnssGeofence(std::shared_ptr<IGnssGeofence>* result) override;
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(std::shared_ptr<IGnssNavigationMessageInterface>* result) override;
    ndk::ScopedAStatus getExtensionMeasurementCorrections(std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>* result) override;
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(std::shared_ptr<visibility_control::IGnssVisibilityControl>* result) override;
    ndk::ScopedAStatus getExtensionGnssPowerIndication(std::shared_ptr<IGnssPowerIndication>* result) override;
    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* result) override;
    ndk::ScopedAStatus getExtensionGnssAssistanceInterface(std::shared_ptr<gnss_assistance::IGnssAssistanceInterface>* _aidl_return) override;

    ndk::ScopedAStatus startSvStatus() override;
    ndk::ScopedAStatus stopSvStatus() override;
    ndk::ScopedAStatus startNmea() override;
    ndk::ScopedAStatus stopNmea() override;

private:
    std::shared_ptr<IGnssCallback> mCallback;
    std::mutex mMutex;
    std::unique_ptr<NmeaReader> mNmeaReader;

    void reportLocation(const GnssLocation& location);
    void reportNmea(int64_t timestamp, const std::string& nmea);
};

} // namespace aidl::android::hardware::gnss
