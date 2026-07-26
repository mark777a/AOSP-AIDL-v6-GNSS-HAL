#include "NmeaReader.h"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <glob.h>
#include <time.h>
#include <termios.h>
#include <cerrno>
#include <android-base/logging.h>
#include <aidl/android/hardware/gnss/ElapsedRealtime.h>

// NMEA output baud rate for the attached receiver. u-blox modules commonly
// default to 9600, but many ship configured for 38400 or 115200 - confirm
// against the actual receiver/datasheet (or u-center) before deploying.
static constexpr speed_t kSerialBaudRate = B9600;

void NmeaReader::start() {
    mRunning = true;
    LOG(INFO) << "GNSS NmeaReader started";
    mThread = std::thread(&NmeaReader::readLoop, this);
}

void NmeaReader::stop() {
    mRunning = false;
    LOG(INFO) << "GNSS NmeaReader stopped";
    if (mThread.joinable()) {
        mThread.join();
    }
}

bool NmeaReader::configureSerialPort(int fd) {
    struct termios tty {};

    if (tcgetattr(fd, &tty) != 0) {
        LOG(ERROR) << "GNSS serial: tcgetattr failed, errno=" << errno;
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, kSerialBaudRate);
    cfsetospeed(&tty, kSerialBaudRate);

    tty.c_cflag |= (CLOCAL | CREAD);  // ignore modem ctrl lines, enable receiver
    tty.c_cflag &= ~PARENB;           // no parity
    tty.c_cflag &= ~CSTOPB;           // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;               // 8 data bits
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;          // no hardware flow control
#endif

    // We drive timing via poll() in readLoop(), so make read() itself
    // non-blocking at the line-discipline level too.
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        LOG(ERROR) << "GNSS serial: tcsetattr failed, errno=" << errno;
        return false;
    }

    // Discard any stale/partial bytes queued before we took ownership of the line.
    tcflush(fd, TCIFLUSH);
    return true;
}

std::string NmeaReader::findDevice() {
    // Look for USB serial. u-blox usually enumerates as ttyACM.
    glob_t glob_result;
    if (glob("/dev/ttyACM*", 0, nullptr, &glob_result) == 0 && glob_result.gl_pathc > 0) {
        std::string path = glob_result.gl_pathv[0];
        globfree(&glob_result);
        LOG(INFO) << "GNSS NmeaReader serial device found at " << path;
        return path;
    }
    // Fallback path
    return "/dev/ttyUSB0";
}

void NmeaReader::readLoop() {
    int fd = -1;
    char buffer[1024];

    while (mRunning) {
        if (fd < 0) {
            std::string path = findDevice();
            // O_RDWR (rather than O_RDONLY) so we can later send configuration
            // commands to the receiver over the same fd if needed.
            fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (fd < 0) {
                LOG(WARNING) << "GNSS device not found. Retrying in 15s...";
                std::this_thread::sleep_for(std::chrono::seconds(15));
                continue;
            }

            if (!configureSerialPort(fd)) {
                LOG(ERROR) << "GNSS serial: could not configure " << path
                           << ", retrying in 5s...";
                close(fd);
                fd = -1;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

            LOG(INFO) << "Connected to GNSS receiver at " << path;
        }

        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 1000); // 1 sec timeout to check mRunning

        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
            //LOG(INFO) << "GNSS read bytes=" << bytesRead;
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                mParser.append(buffer, bytesRead); 
                processParsedData();
            } else if (bytesRead <= 0) {
                LOG(ERROR) << "GNSS/GPS USB Disconnected (read error).";
                close(fd);
                fd = -1;
                // Avoid a tight open/close spin if the device flaps rapidly.
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        } else if (ret > 0 && (pfd.revents & (POLLERR | POLLHUP))) {
            LOG(ERROR) << "GNSS/GPS USB Disconnected (POLLERR/HUP).";
            close(fd);
            fd = -1;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    if (fd >= 0) {
        LOG(ERROR) << "GNSS/GPS NmeaReader closing. " << fd;
        close(fd);
    }
}

void NmeaReader::processParsedData() {
    std::vector<std::string> sentences = mParser.getReadySentences();
    
    for (const auto& sentence : sentences) {
    
        //LOG(INFO) << "GNSS NMEA: " << sentence;
    
        if (!mParser.verifyChecksum(sentence)) {
            LOG(WARNING) << "NMEA Checksum failed, dropping: " << sentence;
            continue;
        }

        // 1. Report raw NMEA back to the framework immediately
        // Uses standard CLOCK_REALTIME milliseconds for the raw callback
        struct timespec rt;
        clock_gettime(CLOCK_REALTIME, &rt);
        int64_t timestampMs = rt.tv_sec * 1000LL + rt.tv_nsec / 1000000LL;
        
        if (mNmeaCallback) {
            //LOG(INFO) << "Sending NMEA callback";
            mNmeaCallback(timestampMs, sentence);
        }
        
        // 2. Parse payload. If it yields a valid update (GGA/RMC), send the Location Callback
        if (mParser.parseSentence(sentence, mCurrentLocation)) {
            
            // =========================================================
            // ANDROID 16 FRAMEWORK EXPECTATION: ELAPSED REALTIME
            // Must strictly use CLOCK_BOOTTIME for Automotive VTS passes
            // =========================================================
            struct timespec boottime;
            clock_gettime(CLOCK_BOOTTIME, &boottime);

            mCurrentLocation.elapsedRealtime.flags =
                    aidl::android::hardware::gnss::ElapsedRealtime::HAS_TIMESTAMP_NS;

            mCurrentLocation.elapsedRealtime.timestampNs =
                    static_cast<int64_t>(boottime.tv_sec) * 1000000000LL +
                    boottime.tv_nsec;

            mCurrentLocation.elapsedRealtime.flags |=
                    aidl::android::hardware::gnss::ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;

            mCurrentLocation.elapsedRealtime.timeUncertaintyNs = 2000000.0;

            float hdop = mParser.getCurrentHdop();
            if (hdop > 0.0f) {
                mCurrentLocation.gnssLocationFlags |=
                        GnssLocation::HAS_HORIZONTAL_ACCURACY;

                mCurrentLocation.horizontalAccuracyMeters = 2.5f * hdop;

                if (mCurrentLocation.gnssLocationFlags &
                        GnssLocation::HAS_ALTITUDE) {
                    mCurrentLocation.gnssLocationFlags |=
                            GnssLocation::HAS_VERTICAL_ACCURACY;

                    mCurrentLocation.verticalAccuracyMeters =
                            (2.5f * hdop) * 1.5f;
                }
            }

            if (mLocationCallback) {
                mLocationCallback(mCurrentLocation);
            }

            mCurrentLocation.gnssLocationFlags = 0;        }

        
        // 3. Check for SV Status
        std::vector<aidl::android::hardware::gnss::IGnssCallback::GnssSvInfo> svList;
        if (mParser.parseSvStatus(sentence, svList)) {
            //LOG(INFO) << "Sending SV status count=" << svList.size();
            if (mSvStatusCallback) {
                mSvStatusCallback(svList);
            }
        }        
    }
}

int64_t NmeaReader::getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    LOG(INFO) << "GNSS NmeaReader getCurrentTime called " << ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

