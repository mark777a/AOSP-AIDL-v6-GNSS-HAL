#include "Gnss.h"
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::gnss::Gnss;

int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger(android::base::SYSTEM));
    LOG(INFO) << "Starting RPi GNSS AIDL v6 HAL...";

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    std::shared_ptr<Gnss> gnss = ndk::SharedRefBase::make<Gnss>();
    const std::string instance = std::string() + Gnss::descriptor + "/default";
    
    binder_status_t status = AServiceManager_addService(gnss->asBinder().get(), instance.c_str());

    if (status != STATUS_OK) {
        LOG(ERROR) << "Failed to register GNSS HAL service: " << status;
        return EXIT_FAILURE;
    }
    
    LOG(INFO) << "Raspberry Pi NMEA GNSS HAL started";

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE; // Should not reach here
}

