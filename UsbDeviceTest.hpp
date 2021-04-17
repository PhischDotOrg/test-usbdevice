/*-
 * $Copyright$
 */

#ifndef USB_DEVICE_TEST_HPP_C208FB0A_33A0_411C_91B3_F19C78ECCBA07
#define USB_DEVICE_TEST_HPP_C208FB0A_33A0_411C_91B3_F19C78ECCBA07

#include <gtest/gtest.h>
#include <libusb-1.0/libusb.h>
#include <cstdint>

class UsbDeviceTest : public ::testing::Test {
    static const uint16_t   m_vendorId;
    static const uint16_t   m_deviceId;
    static const uint8_t    m_interfaceClass;
    static const uint8_t    m_interfaceProtocol;
    static const uint8_t    m_interfaceSubClass;

    static const int        m_testConfiguration;
    static const int        m_testInterface;

    libusb_context *        m_ctx;
    libusb_device **        m_devs;
    libusb_device *         m_dutRef;

    int                     m_activeConfiguration;

    struct libusb_device_descriptor             m_deviceDescriptor;
    const struct libusb_config_descriptor *     m_configDescriptor;
    const struct libusb_interface_descriptor *  m_interfaceDescriptor;


    void libUsbInit(void);
    void openDevice(void);
    void activateDeviceConfiguration(void);
    void parseDeviceDescriptor(void);
    void parseConfigDescriptor(void);
    void parseInterfaceDescriptor(void);

    void resetDeviceConfiguration(void);
    void closeDevice(void);
    void libUsbCleanUp(void);

    static
    enum libusb_endpoint_direction
    getEndpointDirection(const struct libusb_endpoint_descriptor &p_endptDescriptor) {
        return static_cast<enum libusb_endpoint_direction>(p_endptDescriptor.bEndpointAddress & 0x80);
    }

    static
    enum libusb_endpoint_transfer_type
    getEndpointType(const struct libusb_endpoint_descriptor &p_endptDescriptor) {
        return static_cast<enum libusb_endpoint_transfer_type>(((p_endptDescriptor.bmAttributes >> 0) & 0b11));
    }


protected:
    libusb_device_handle *                      m_dutHandle;
    const struct libusb_endpoint_descriptor *   m_bulkOutEndpoint;
    const struct libusb_endpoint_descriptor *   m_bulkInEndpoint;
    unsigned                                    m_maxBufferSz;
    unsigned                                    m_txTimeout;
    unsigned                                    m_rxTimeout;


    void SetUp(void) override {
        libUsbInit();
    }

    void TearDown() override {
        libUsbCleanUp();
    }

    UsbDeviceTest(void);
    virtual ~UsbDeviceTest();
};

#endif /* USB_DEVICE_TEST_HPP_C208FB0A_33A0_411C_91B3_F19C78ECCBA0 */
