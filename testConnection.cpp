/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>

class UsbDeviceConnectionTest : public ::testing::Test {
private:

protected:
    static const uint16_t   m_vendorId;
    static const uint16_t   m_deviceId;

    libusb_context *    m_ctx;
    libusb_device **    m_devs;
    ssize_t             m_cnt;
    
    UsbDeviceConnectionTest() : m_ctx(nullptr), m_devs(nullptr), m_cnt(0) {

    }

    virtual ~UsbDeviceConnectionTest() {
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
    }

    void TearDown(void) override {
    }
};

const uint16_t UsbDeviceConnectionTest::m_vendorId = 0xdead;
const uint16_t UsbDeviceConnectionTest::m_deviceId = 0xbeef;

TEST_F(UsbDeviceConnectionTest, IsConnected) {
    bool found = false;

    for (ssize_t i = 0; (i < m_cnt) && !found; i++) {
        libusb_device_descriptor desc;

        int rc = libusb_get_device_descriptor(m_devs[i], &desc);
        EXPECT_EQ(0, rc);

        if ((desc.idVendor == m_vendorId) && (desc.idProduct == m_deviceId)) {
            found = true;
        }
    }

    EXPECT_TRUE(found) << "USB Device with Vendor ID = '0x" << std::hex << m_vendorId << "' and Device ID = '0x" << std::hex << m_deviceId << "' not found.";
}
