/*-
 * $Copyright$
 */

#include <libusb-1.0/libusb.h>

#include <gtest/gtest.h>

#include <algorithm>

#include "UsbDeviceTest.hpp"

/******************************************************************************/
namespace usb {
/******************************************************************************/
struct SetupPacket {
    struct RequestType {
        enum class Direction : uint8_t {
            e_HostToDevice  = 0b0,
            e_DeviceToHost  = 0b1,
        };
        static_assert(sizeof(Direction) == 1);

        enum class Type : uint8_t {
            e_Standard  = 0,
            e_Class     = 1,
            e_Vendor    = 2,
            e_Reserved  = 3,
        };
        static_assert(sizeof(Type) == 1);

        enum class Recipient : uint8_t {
            e_Device    = 0,
            e_Interface = 1,
            e_Endpoint  = 2,
            e_Other     = 3,
            e_Reserved  = 31,
        };
        static_assert(sizeof(Recipient) == 1);

        Direction   m_direction : 1;
        Type        m_type      : 2;
        Recipient   m_recipient : 5;

        explicit constexpr
        operator uint8_t(void) const {
            return (
                   ((static_cast<uint8_t>(m_direction)  << 7) & 0b1000'0000)
                 | ((static_cast<uint8_t>(m_type)       << 5) & 0b0110'0000)
                 | ((static_cast<uint8_t>(m_recipient)  << 0) & 0b0001'1111)
            );
        }
    };
    static_assert(sizeof(RequestType) == 1);

    enum class Request : uint8_t {
        e_NoData    = 0x01,
        e_DataOut   = 0x02,
        e_DataIn    = 0x03,
        e_Invalid   = 0xFF
    };
    static_assert(sizeof(Request) == 1);

    struct Parameter {
        enum class ReturnCode : uint8_t {
            e_Ack       = 0,
            e_Timeout   = 1,
            e_Stall     = 2,
        };
        static_assert(sizeof(ReturnCode) == 1);

        ReturnCode  m_value;
        uint8_t     m_delayInMs;

        explicit constexpr
        operator uint16_t(void) const {
            return (
                   ((static_cast<uint16_t>(m_delayInMs) << 8) & 0xFF00)
                 | ((static_cast<uint16_t>(m_value) << 0) & 0x00FF)
            );
        }
    };
    static_assert(sizeof(Parameter) == 2);

    RequestType     m_bmRequestType;
    Request         m_bRequest;
    // union u_wValue {
    //     uint16_t    m_raw;
    //     Parameter   m_parameter;
    // };
    Parameter       m_wValue;
    uint16_t        m_wIndex;
    uint16_t        m_wLength;
};
static_assert(sizeof(SetupPacket) == 8);

/******************************************************************************/
} /* namespace usb */
/******************************************************************************/

class ControlTransferTest : public UsbDeviceTest {
    int
    ctrlTransfer(const usb::SetupPacket &p_setupPacket, std::vector<uint8_t> * const p_data) const {
        uint8_t     bmRequestType = static_cast<uint8_t>(p_setupPacket.m_bmRequestType);
        uint8_t     bRequest = static_cast<uint8_t>(p_setupPacket.m_bRequest);
        uint16_t    wValue = static_cast<uint16_t>(p_setupPacket.m_wValue);

        int rc = libusb_control_transfer(
                    m_dutHandle,
                    bmRequestType,
                    bRequest,
                    wValue,
                    p_setupPacket.m_wIndex,
                    p_data == nullptr ? nullptr : p_data->data(),
                    std::min(p_data == nullptr ? 0 : p_data->size(), static_cast<size_t>(p_setupPacket.m_wLength)),
                    m_ctrlTimeout
            );

        return rc;
    }

protected:
    int
    ctrlTransfer(::usb::SetupPacket::Request p_request, ::usb::SetupPacket::Parameter::ReturnCode p_returnCode,
      uint8_t p_delayInMs, std::vector<uint8_t> * const p_data) const {
        ::usb::SetupPacket setupPacket = {
            .m_bmRequestType = {
                .m_direction    = p_request == ::usb::SetupPacket::Request::e_DataIn ? ::usb::SetupPacket::RequestType::Direction::e_DeviceToHost : ::usb::SetupPacket::RequestType::Direction::e_HostToDevice,
                .m_type         = ::usb::SetupPacket::RequestType::Type::e_Vendor,
                .m_recipient    = ::usb::SetupPacket::RequestType::Recipient::e_Interface,
            },
            .m_bRequest         = p_request,
            .m_wValue = {
                .m_value        = p_returnCode,
                .m_delayInMs    = p_delayInMs,
            },
            .m_wIndex           = 0,
            .m_wLength          = (p_data == nullptr) ?
                                    static_cast<uint16_t>(0u) : (p_data->size() < (1 << 16)) ?
                                        static_cast<uint16_t>(p_data->size()) : static_cast<uint16_t>((1 << 16) - 1)
        };

        return ctrlTransfer(setupPacket, p_data);
    }
};

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
      m_ctrlTimeout
    );
    EXPECT_EQ(sizeof(uint16_t), rc) << libusb_error_name(rc);
}

/*****************************************************************************/
// Control Transfers without Data Stage
/*****************************************************************************/
TEST_F(ControlTransferTest, NoDataStage_StatusStage_ACK) {
    int rc = ctrlTransfer(
        ::usb::SetupPacket::Request::e_NoData,
        ::usb::SetupPacket::Parameter::ReturnCode::e_Ack,
        0,
        nullptr
    );
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << libusb_error_name(rc);
}

TEST_F(ControlTransferTest, NoDataStage_StatusStage_NAK_to_ACK) {
    int rc = ctrlTransfer(
        ::usb::SetupPacket::Request::e_NoData,
        ::usb::SetupPacket::Parameter::ReturnCode::e_Ack,
        this->m_ctrlTimeout / 2,
        nullptr
    );
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << libusb_error_name(rc);
}

TEST_F(ControlTransferTest, DISABLED_NoDataStage_StatusStage_NAK_Timeout) {
    int rc = ctrlTransfer(
        ::usb::SetupPacket::Request::e_NoData,
        ::usb::SetupPacket::Parameter::ReturnCode::e_Timeout,
        -1,
        nullptr
    );
    EXPECT_EQ(LIBUSB_ERROR_TIMEOUT, rc) << libusb_error_name(rc);
}

TEST_F(ControlTransferTest, NoDataStage_StatusStage_NAK_to_STALL) {
    int rc = ctrlTransfer(
        ::usb::SetupPacket::Request::e_NoData,
        ::usb::SetupPacket::Parameter::ReturnCode::e_Stall,
        this->m_ctrlTimeout / 2,
        nullptr
    );
    EXPECT_NE(LIBUSB_SUCCESS, rc) << libusb_error_name(rc);
}

TEST_F(ControlTransferTest, NoDataStage_StatusStage_STALL) {
    int rc = ctrlTransfer(
        ::usb::SetupPacket::Request::e_NoData,
        ::usb::SetupPacket::Parameter::ReturnCode::e_Stall,
        0,
        nullptr
    );
    EXPECT_NE(LIBUSB_SUCCESS, rc) << libusb_error_name(rc);
}

/*****************************************************************************/
// Control Transfers with Data OUT Stage (Short Packet)
/*****************************************************************************/
TEST_F(ControlTransferTest, DataStage_OUT_Short_StatusStage_ACK) {

}

TEST_F(ControlTransferTest, DataStage_OUT_Short_StatusStage_NAK_to_ACK) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_Short_StatusStage_NAK_Timeout) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_Short_StatusStage_NAK_to_STALL) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_Short_StatusStage_STALL) {
  
}

/*****************************************************************************/
// Control Transfers with Data OUT Stage (Almost Full Packet, i.e. 63 Bytes in
// case of a Full Speed Device).
/*****************************************************************************/
TEST_F(ControlTransferTest, DataStage_OUT_AlmostFullPacket_StatusStage_ACK) {

}

TEST_F(ControlTransferTest, DataStage_OUT_AlmostFullPacket_NAK_to_ACK) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_AlmostFullPacket_NAK_Timeout) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_AlmostFullPacket_NAK_to_STALL) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_AlmostFullPacket_STALL) {
  
}


/*****************************************************************************/
// Control Transfers with Data OUT Stage (Full Packet, i.e. exactly 64 Bytes
// in case of a Full Speed Device).
/*****************************************************************************/
TEST_F(ControlTransferTest, DataStage_OUT_FullPacket_StatusStage_ACK) {

}

TEST_F(ControlTransferTest, DataStage_OUT_FullPacket_NAK_to_ACK) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_FullPacket_NAK_Timeout) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_FullPacket_NAK_to_STALL) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_FullPacket_STALL) {
  
}

/*****************************************************************************/
// Control Transfers with Data OUT Stage (Multiple Packets)
/*****************************************************************************/
TEST_F(ControlTransferTest, DataStage_OUT_MultiplePackets_StatusStage_ACK) {

}

TEST_F(ControlTransferTest, DataStage_OUT_MultiplePackets_NAK_to_ACK) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_MultiplePackets_NAK_Timeout) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_MultiplePackets_NAK_to_STALL) {
  
}

TEST_F(ControlTransferTest, DataStage_OUT_MultiplePackets_STALL) {
  
}

/*****************************************************************************/
// Control Transfers with an invalid request. Should yield the same result
// as those with a STALL response.
/*****************************************************************************/
TEST_F(ControlTransferTest, InvalidRequest) {
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
      m_ctrlTimeout
    );
    EXPECT_EQ(LIBUSB_ERROR_PIPE, rc);
}
