#/device/brcm/rpi5/device.mk - lines added marked with + as first caracter (remove + caracter before saving)
...
# USB
PRODUCT_PACKAGES += \
    com.android.hardware.usb \
    com.android.hardware.usb.gadget.rpi5

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.software.midi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.midi.xml

+# added for GPS / GLONASS USB dongle (U-blox7) - custom Raspberry Pi NMEA HAL
+PRODUCT_PACKAGES += \
+    android.hardware.gnss-service.rpi
+
+# Expose GPS feature to Android OS
+PRODUCT_COPY_FILES += \
+    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml

# Virtualization
$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)

# Wifi
PRODUCT_PACKAGES += \
    com.android.hardware.wifi \
    com.android.hardware.wifi.hostapd.rpi5 \
    com.android.hardware.wifi.supplicant.rpi5 \
    libwpa_client \
    wificond
...