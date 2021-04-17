/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>

#include <algorithm>

class ControlTransferTest : public ::testing::Test {
protected:
    static const uint16_t   m_vendorId;
    static const uint16_t   m_deviceId;

    libusb_context *        m_ctx;
    libusb_device **        m_devs;
    ssize_t                 m_cnt;
    libusb_device *         m_dutRef;
    libusb_device_handle *  m_dutHandle;

    static const int        m_testConfiguration;
    int                     m_activeConfiguration;

    static const int        m_testInterface;

    static const int        m_timeout;

    void SetConfiguration() {
        int cfgNum, rc;

        rc = libusb_get_configuration(m_dutHandle, &cfgNum);
        EXPECT_EQ(0, rc);

        ASSERT_EQ(0, cfgNum) << "Expected USB Device to be unconfigured but Configuration '" << cfgNum << "' is already active.";

        rc = libusb_set_configuration(m_dutHandle, m_testConfiguration);
        ASSERT_EQ(LIBUSB_SUCCESS, rc) << "Configuration '" << m_testConfiguration << "' could not be activated.";

        rc = libusb_get_configuration(m_dutHandle, &m_activeConfiguration);
        EXPECT_EQ(0, rc);

        ASSERT_EQ(m_testConfiguration, m_activeConfiguration) << "Expected USB Device Configuration '" << m_testConfiguration
          << "', but Configuration '" << m_activeConfiguration << "' is active.";
    }

    void ClearConfiguration() {
        int cfgNum, rc;

        if (!m_activeConfiguration) {
            return;
        }

        rc = libusb_get_configuration(m_dutHandle, &cfgNum);
        EXPECT_EQ(0, rc);

        ASSERT_EQ(m_activeConfiguration, cfgNum) << "Expected USB Device Configuration '" << m_activeConfiguration << "', but Configuration '" << cfgNum << "' is active.";

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

        ASSERT_EQ(0, cfgNum) << "Expected USB Device Configuration '0', but Configuration '" << cfgNum << "' is active.";
    }

protected:
    ControlTransferTest() : m_ctx(nullptr), m_devs(nullptr), m_cnt(0),
      m_dutRef(nullptr), m_dutHandle(nullptr), m_activeConfiguration(0) {

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

        this->SetConfiguration();

        rc = libusb_claim_interface(m_dutHandle, m_testInterface);
    }

    void TearDown() override {
        if (nullptr != m_dutHandle) {
            int rc = libusb_release_interface(m_dutHandle, m_testInterface);
            EXPECT_EQ(0, rc);
        }

        this->ClearConfiguration();

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
};

const uint16_t ControlTransferTest::m_vendorId = 0xdead;
const uint16_t ControlTransferTest::m_deviceId = 0xbeef;
const int ControlTransferTest::m_testConfiguration = 1;
const int ControlTransferTest::m_testInterface = 1;

const int ControlTransferTest::m_timeout = 1500;

TEST_F(ControlTransferTest, GetStatus) {
    std::vector<uint8_t> rxBuf(sizeof(uint16_t));
    int rc;

    ASSERT_GE(rxBuf.size(), sizeof(uint16_t));

    rc = libusb_control_transfer(m_dutHandle,
        (1 << 7)    /* Direction: Device to Host */
      | (0 << 5)    /* Type: 0 = Standard, 1 = Class, 2 = Vendor, 3 = Reserved */
      | (0 << 0),   /* Recipient: 0 = Device, 1 = Interface, 2 = Endpoint, 3 = Other, 4..31 = Reserved */
      0x0,          /* bRequest = Get Status */
      0x0,          /* wValue */
      0x0,          /* wIndex */
      rxBuf.data(),
      std::min(sizeof(uint16_t), rxBuf.size()), /* wLength */
      m_timeout
    );
    EXPECT_EQ(sizeof(uint16_t), rc);
}

TEST_F(ControlTransferTest, DISABLED_InvalidRequest) {
    std::vector<uint8_t> rxBuf(sizeof(uint16_t));
    int rc;

    ASSERT_GE(rxBuf.size(), sizeof(uint16_t));

    rc = libusb_control_transfer(m_dutHandle,
        (1 << 7)    /* Direction: Device to Host */
      | (3 << 5)    /* Type: 0 = Standard, 1 = Class, 2 = Vendor, 3 = Reserved */
      | (4 << 0),   /* Recipient: 0 = Device, 1 = Interface, 2 = Endpoint, 3 = Other, 4..31 = Reserved */
      0x0,          /* bRequest = Get Status */
      0x0,          /* wValue */
      0x0,          /* wIndex */
      rxBuf.data(),
      std::min(sizeof(uint16_t), rxBuf.size()), /* wLength */
      m_timeout
    );
    EXPECT_EQ(LIBUSB_ERROR_PIPE, rc);
}
