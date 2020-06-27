/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>

#include <algorithm>

#include <cstdlib>

class BulkTransferTest : public ::testing::Test {
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

    static const int        m_outEndpoint;
    static const int        m_inEndpoint;

    static const int        m_txTimeout;
    static const int        m_rxTimeout;

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
    BulkTransferTest() : m_ctx(nullptr), m_devs(nullptr), m_cnt(0),
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
        // sleep(2);

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

const uint16_t BulkTransferTest::m_vendorId = 0xdead;
const uint16_t BulkTransferTest::m_deviceId = 0xbeef;
const int BulkTransferTest::m_testConfiguration = 1;
const int BulkTransferTest::m_testInterface = 1;

/* libusb wants the direction bit in the endpoint number */
const int BulkTransferTest::m_outEndpoint = 0x01;
const int BulkTransferTest::m_inEndpoint = 0x81;

const int BulkTransferTest::m_txTimeout = 1500;
const int BulkTransferTest::m_rxTimeout = 5000;

TEST_F(BulkTransferTest, LoopbackSmall) {
    const std::vector<uint8_t> txBuf { 0x12, 0x34, 0x56, 0x78 };
    std::vector<uint8_t> rxBuf(txBuf.size());
    int rc, txLen, rxLen;

    /* USB Device's buffer is quite small */
    ASSERT_GE(512, txBuf.size());

    rc = libusb_bulk_transfer(m_dutHandle, m_outEndpoint, const_cast<unsigned char *>(txBuf.data()), txBuf.size(), &txLen, m_txTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed.";

    rc = libusb_bulk_transfer(m_dutHandle, m_inEndpoint, rxBuf.data(), std::min(static_cast<size_t>(txLen), rxBuf.size()), &rxLen, m_rxTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed.";

    EXPECT_EQ(txBuf, rxBuf);
}

TEST_F(BulkTransferTest, LoopbackLarge) {
    std::vector<uint8_t> txBuf(64);
    std::vector<uint8_t> rxBuf(txBuf.size());
    int rc, txLen, rxLen;

    /* USB Device's buffer is quite small */
    ASSERT_GE(512, txBuf.size());

    std::generate(txBuf.begin(), txBuf.end(), [&]{ return rand(); });

    rc = libusb_bulk_transfer(m_dutHandle, m_outEndpoint, txBuf.data(), txBuf.size(), &txLen, m_txTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed.";
    EXPECT_EQ(txLen, txBuf.size());

    rc = libusb_bulk_transfer(m_dutHandle, m_inEndpoint, rxBuf.data(), rxBuf.size(), &rxLen, m_rxTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed.";
    EXPECT_EQ(txBuf, rxBuf);
}

TEST_F(BulkTransferTest, LoopbackMultiple) {
    static const unsigned transferSz = 64;
    static const unsigned transferCnt = (1024 / transferSz) * 1024 * 1;

    std::vector<uint8_t> txBuf(transferSz);
    std::vector<uint8_t> rxBuf(txBuf.size());
    int rc, txLen, rxLen;

    /* USB Device's buffer is quite small */
    ASSERT_GE(512, txBuf.size());

    for (unsigned cnt = 0; cnt < transferCnt; ++cnt) {
        txBuf.clear();
        txBuf.resize(transferSz);

        std::generate(txBuf.begin(), txBuf.end(), [&]{ return rand(); });

        rxBuf.clear();
        rxBuf.resize(txBuf.size());
        ASSERT_EQ(rxBuf.size(), txBuf.size());

        rc = libusb_bulk_transfer(m_dutHandle, m_outEndpoint, const_cast<unsigned char *>(txBuf.data()), txBuf.size(), &txLen, m_txTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed in repetition #" << cnt;

        rc = libusb_bulk_transfer(m_dutHandle, m_inEndpoint, rxBuf.data(), std::min(static_cast<size_t>(txLen), rxBuf.size()), &rxLen, m_rxTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed in repetition #" << cnt;

        EXPECT_EQ(txBuf, rxBuf);
    }
}
