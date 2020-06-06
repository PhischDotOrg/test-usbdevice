/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>

class UsbDeviceConfigurationTest : public ::testing::Test {
private:

protected:
    static const uint16_t   m_vendorId;
    static const uint16_t   m_deviceId;

    static const int        m_debugConfiguration;

    libusb_context *        m_ctx;
    libusb_device **        m_devs;
    ssize_t                 m_cnt;
    libusb_device *         m_dutRef;
    libusb_device_handle *  m_dutHandle;

    UsbDeviceConfigurationTest() : m_ctx(nullptr), m_devs(nullptr), m_cnt(0), m_dutRef(nullptr), m_dutHandle(nullptr) {

    }

    virtual ~UsbDeviceConfigurationTest() {
        if (m_dutHandle != nullptr) {
            libusb_close(m_dutHandle);
        }

        if (m_dutRef != nullptr) {
            libusb_unref_device(m_dutRef);
        }

        if (m_devs != nullptr) {
            libusb_free_device_list(m_devs, 1);
        }

        if (m_ctx != nullptr) {
            libusb_exit(m_ctx);
        }
    }

    void SetUp(void) override {
        int rc = libusb_init(&m_ctx);
        ASSERT_EQ(LIBUSB_SUCCESS, rc) << "Failed to initialize libusb";
        ASSERT_NE(nullptr, m_ctx);

        libusb_set_option(m_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

        m_cnt = libusb_get_device_list(this->m_ctx, &this->m_devs);
        ASSERT_GE(m_cnt, 0) << __func__ << ": Failed to obtain the list of devices.";
        ASSERT_NE(nullptr, m_devs);

        libusb_device_descriptor desc;
        for (ssize_t i = 0; (i < m_cnt) && (m_dutRef == nullptr); i++) {
            int rc = libusb_get_device_descriptor(m_devs[i], &desc);
            EXPECT_EQ(0, rc);

            if ((desc.idVendor == m_vendorId) && (desc.idProduct == m_deviceId)) {
                m_dutRef = libusb_ref_device(m_devs[i]);
            }
        }
        ASSERT_NE(nullptr, m_dutRef);

        rc = libusb_open(m_dutRef, &m_dutHandle);
        ASSERT_EQ(0, rc);
        ASSERT_NE(nullptr, m_dutHandle);
    }

    void TearDown(void) override {
    }
};

const uint16_t UsbDeviceConfigurationTest::m_vendorId = 0xdead;
const uint16_t UsbDeviceConfigurationTest::m_deviceId = 0xbeef;
const int UsbDeviceConfigurationTest::m_debugConfiguration = 1;

TEST_F(UsbDeviceConfigurationTest, GetConfiguration) {
    int cfgNum;

    int rc = libusb_get_configuration(m_dutHandle, &cfgNum);
    EXPECT_EQ(0, rc);

    EXPECT_EQ(0, cfgNum) << "Expected USB Device to be unconfigured but Configuration '" << cfgNum << "' is already active.";
}

TEST_F(UsbDeviceConfigurationTest, ActivateConfiguration) {
    int rc, cfgNum;

    rc = libusb_set_configuration(m_dutHandle, m_debugConfiguration);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Configuration '" << m_debugConfiguration << "' could not be activated.";

    rc = libusb_get_configuration(m_dutHandle, &cfgNum);
    EXPECT_EQ(0, rc);

    EXPECT_EQ(m_debugConfiguration, cfgNum) << "Expected USB Device Configuration '" << m_debugConfiguration << "', but Configuration '" << cfgNum << "' is active.";
}

TEST_F(UsbDeviceConfigurationTest, DeactivateConfiguration) {
    int rc, cfgNum;

    rc = libusb_get_configuration(m_dutHandle, &cfgNum);
    EXPECT_EQ(0, rc);

    EXPECT_EQ(m_debugConfiguration, cfgNum) << "Expected USB Device Configuration '" << m_debugConfiguration << "', but Configuration '" << cfgNum << "' is active.";

    /*
     * libusb Documentation says that -1 should be used to re-set the device configuration
     * because buggy USB devices may actually have a configuration #0. Unfortunately, I
     * found that this crashes (on macOS).
     * 
     * Since we're testing a device that should handle the configuration according to the
     * USB standard, this fear should not apply.
     */
    rc = libusb_set_configuration(m_dutHandle, 0);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Configuration could not be de-activated.";

    rc = libusb_get_configuration(m_dutHandle, &cfgNum);
    EXPECT_EQ(0, rc);
}
