/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>

#include "UsbDeviceTest.hpp"

class BulkTransferTest : public UsbDeviceTest {
protected:
    void
    singleBulkTransfer(const unsigned p_nBytes) {
        std::vector<uint8_t> txBuf(p_nBytes);
        std::vector<uint8_t> rxBuf(txBuf.size());
        int rc, txLen, rxLen;

        std::generate(txBuf.begin(), txBuf.end(), [&]{ return rand(); });

        rc = libusb_bulk_transfer(m_dutHandle, m_bulkOutEndpoint->bEndpointAddress, txBuf.data(), txBuf.size(), &txLen, m_txTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed.";
        EXPECT_EQ(txLen, txBuf.size());

        rc = libusb_bulk_transfer(m_dutHandle, m_bulkInEndpoint->bEndpointAddress, rxBuf.data(), rxBuf.size(), &rxLen, m_rxTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed.";

        EXPECT_EQ(txBuf, rxBuf);
    }
};

TEST_F(BulkTransferTest, LoopbackSmall) {
    const std::vector<uint8_t> txBuf { 0x12, 0x34, 0x56, 0x78 };
    std::vector<uint8_t> rxBuf(txBuf.size());
    int rc, txLen, rxLen;

    /* USB Device's buffer is quite small */
    ASSERT_GE(m_maxBufferSz, txBuf.size());

    rc = libusb_bulk_transfer(m_dutHandle, m_bulkOutEndpoint->bEndpointAddress, const_cast<unsigned char *>(txBuf.data()), txBuf.size(), &txLen, m_txTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed.";

    rc = libusb_bulk_transfer(m_dutHandle, m_bulkInEndpoint->bEndpointAddress, rxBuf.data(), std::min(static_cast<size_t>(txLen), rxBuf.size()), &rxLen, m_rxTimeout);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed.";

    EXPECT_EQ(txBuf, rxBuf);
}

TEST_F(BulkTransferTest, SingleTransferSinglePacketMinusOne) {
    singleBulkTransfer(m_bulkOutEndpoint->wMaxPacketSize - 1);
}

TEST_F(BulkTransferTest, SingleTransferSinglePacket) {
    singleBulkTransfer(m_bulkOutEndpoint->wMaxPacketSize);
}

TEST_F(BulkTransferTest, SingleTransferSinglePacketPlusOne) {
    singleBulkTransfer(m_bulkOutEndpoint->wMaxPacketSize + 1);
}

TEST_F(BulkTransferTest, SingleTransferTwoPackets) {
    singleBulkTransfer(m_bulkOutEndpoint->wMaxPacketSize * 2);
}

TEST_F(BulkTransferTest, DISABLED_SingleTransferMultiplePackets) {
    singleBulkTransfer(m_bulkOutEndpoint->wMaxPacketSize * 3);
}

TEST_F(BulkTransferTest, SingleTransferBufferSizeMinusOne) {
    singleBulkTransfer(m_maxBufferSz - 1);
}


TEST_F(BulkTransferTest, SingleTransferBufferSize) {
    singleBulkTransfer(m_maxBufferSz);
}

TEST_F(BulkTransferTest, DISABLED_SingleTransferBufferSizePlusOne) {
    singleBulkTransfer(m_maxBufferSz + 1);
}

#if 0
TEST_F(BulkTransferTest, DISABLED_LoopbackMultiple) {
    static const unsigned transferSz = 64;
    static const unsigned transferCnt = 2; // (1024 / transferSz) * 1024 * 1;

    std::vector<uint8_t> txBuf(transferSz);
    std::vector<uint8_t> rxBuf(txBuf.size());
    int rc, txLen, rxLen;

    /* USB Device's buffer is quite small */
    ASSERT_GE(m_maxBufferSz, txBuf.size());

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
#endif
