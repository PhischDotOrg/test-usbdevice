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
    multipleBulkTransfers(const unsigned p_nTransfers, const unsigned p_nBytes) {
        for (unsigned idx = 0; idx < p_nTransfers; idx++) {
            singleBulkTransfer(p_nBytes);
        }
    }

    void
    singleBulkTransfer(const unsigned p_nBytes) {
        std::vector<uint8_t> txBuf(p_nBytes);

        std::generate(txBuf.begin(), txBuf.end(), [&]{ return rand(); });

        singleBulkTransfer(txBuf);
    }

    void
    singleBulkTransfer(const std::vector<uint8_t> &p_txBuf, const unsigned p_iteration = 0) {
        std::vector<uint8_t> rxBuf(p_txBuf.size());
        int rc, txLen, rxLen;

        /* Cast to non-const C-Style Pointer so our parameter can be a const Reference */
        uint8_t * const txBuf = const_cast<uint8_t * const>(p_txBuf.data());

        rc = libusb_bulk_transfer(m_dutHandle, m_bulkOutEndpoint->bEndpointAddress, txBuf, p_txBuf.size(), &txLen, m_txTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Tx transfer failed (Iteration #" << p_iteration << ")";
        EXPECT_EQ(txLen, p_txBuf.size());

        rc = libusb_bulk_transfer(m_dutHandle, m_bulkInEndpoint->bEndpointAddress, rxBuf.data(), rxBuf.size(), &rxLen, m_rxTimeout);
        EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Bulk Rx transfer failed (Iteration #" << p_iteration << ")";

        EXPECT_EQ(p_txBuf, rxBuf) << "Iteration #" << p_iteration;
    }
};

TEST_F(BulkTransferTest, SingleTransferSmall) {
    const std::vector<uint8_t> txBuf { 0x12, 0x34, 0x56, 0x78 };
    singleBulkTransfer(txBuf);
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

TEST_F(BulkTransferTest, MultiTransferSmall) {
    const std::vector<uint8_t> txBuf1 { 0x12, 0x34, 0x56, 0x78 };
    const std::vector<uint8_t> txBuf2 { 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34 };

    singleBulkTransfer(txBuf1, 0);
    // sleep(1);
    singleBulkTransfer(txBuf2, 1);
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
