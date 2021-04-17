/*-
 * $Copyright$
 */

#include "UsbDeviceTest.hpp"

#include <libusb-1.0/libusb.h>

const uint16_t  UsbDeviceTest::m_vendorId           = 0xdead;
const uint16_t  UsbDeviceTest::m_deviceId           = 0xbeef;
const uint8_t   UsbDeviceTest::m_interfaceClass     = LIBUSB_CLASS_VENDOR_SPEC;
const uint8_t   UsbDeviceTest::m_interfaceSubClass  = 0x10;
const uint8_t   UsbDeviceTest::m_interfaceProtocol  = 0x0B;

const int UsbDeviceTest::m_testConfiguration    = 1;        // Configuration Number for Loopback Test Interface

UsbDeviceTest::UsbDeviceTest(void)
  : m_ctx(nullptr),
    m_devs(nullptr),
    m_dutRef(nullptr),
    m_activeConfiguration(0),
    m_configDescriptor(nullptr), 
    m_interfaceDescriptor(nullptr),
    m_dutHandle(nullptr),
    m_bulkOutEndpoint(nullptr),
    m_bulkInEndpoint(nullptr),
    m_maxBufferSz(0)
{

}

UsbDeviceTest::~UsbDeviceTest() {

}

void
UsbDeviceTest::activateDeviceConfiguration(void) {
    int rc;

    rc = libusb_get_configuration(m_dutHandle, &m_activeConfiguration);
    EXPECT_EQ(0, rc);

    if (m_activeConfiguration == 0) {
        rc = libusb_set_configuration(m_dutHandle, m_testConfiguration);
        ASSERT_EQ(LIBUSB_SUCCESS, rc) << "Configuration '" << m_testConfiguration << "' could not be activated.";

        rc = libusb_get_configuration(m_dutHandle, &m_activeConfiguration);
        EXPECT_EQ(0, rc);
    } else {
        EXPECT_EQ(m_testConfiguration, m_activeConfiguration)
          << "Expected USB Device to already be configured with Configuration '" << m_testConfiguration
          << "' but Configuration '" << m_activeConfiguration << "' is already active.";
    }

    ASSERT_EQ(m_testConfiguration, m_activeConfiguration)
      << "Expected USB Device Configuration '" << m_testConfiguration
      << "', but Configuration '" << m_activeConfiguration << "' is active.";
}

void
UsbDeviceTest::resetDeviceConfiguration() {
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
    rc = libusb_set_configuration(m_dutHandle, -1);
    EXPECT_EQ(LIBUSB_SUCCESS, rc) << "Configuration could not be de-activated.";

    rc = libusb_get_configuration(m_dutHandle, &cfgNum);
    EXPECT_EQ(0, rc);

    ASSERT_EQ(0, cfgNum) << "Expected USB Device Configuration '0', but Configuration '" << cfgNum << "' is active.";
}

void
UsbDeviceTest::openDevice(void) {
    int rc;

    /* Find USB Device under Test */
    ssize_t cnt = libusb_get_device_list(this->m_ctx, &this->m_devs);
    ASSERT_GE(cnt, 0) << __func__ << ": Failed to obtain the list of devices.";
    ASSERT_NE(nullptr, m_devs);

    for (ssize_t i = 0; (i < cnt) && (m_dutRef == nullptr); i++) {
        libusb_device_descriptor desc;
        rc = libusb_get_device_descriptor(m_devs[i], &desc);
        EXPECT_EQ(0, rc);

        if ((desc.idVendor == m_vendorId) && (desc.idProduct == m_deviceId)) {
            m_dutRef = libusb_ref_device(m_devs[i]);
        }
    }
    ASSERT_NE(nullptr, m_dutRef)
      << "USB Device under Test not found! "
      << "Expected VendorId=0x" << std::hex << m_vendorId
      << "and DeviceId=0x" << std::hex << m_deviceId;

    rc = libusb_open(m_dutRef, &m_dutHandle);
    ASSERT_EQ(0, rc);
    ASSERT_NE(nullptr, m_dutHandle);

    rc = libusb_get_device_descriptor(m_dutRef, &m_deviceDescriptor);
    ASSERT_EQ(0, rc) << "Failed to read Device Descriptor (rc=" << rc << ")";  
}

void
UsbDeviceTest::libUsbInit(void) {
    int rc;

    /* Initialize libusb Stack */
    rc = libusb_init(&m_ctx);
    ASSERT_EQ(LIBUSB_SUCCESS, rc) << "Failed to initialize libusb";
    ASSERT_NE(nullptr, m_ctx);

    libusb_set_option(m_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

    /* Find USB Device under Test */
    openDevice();

    /* Active USB Device's Test Configuration */
    activateDeviceConfiguration();

    /* Read the active Configuration's Descriptor */
    ASSERT_NE(nullptr, m_dutRef);
    ASSERT_NE(0, m_activeConfiguration) << "Expected USB Device to be configured";
    rc = libusb_get_active_config_descriptor(m_dutRef, const_cast<libusb_config_descriptor **>(&m_configDescriptor));
    ASSERT_EQ(0, rc) << "Failed to read Configuration Descriptor (rc=" << rc << ")";

    /* Parse the USB Configuration Descriptor of Device's active Configuration */
    parseConfigDescriptor();
    parseInterfaceDescriptor();

    /* Claim the USB Device's Test Interface */
    ASSERT_NE(nullptr, m_interfaceDescriptor) << "No valid Interface Descriptor found!";
    rc = libusb_claim_interface(m_dutHandle, m_interfaceDescriptor->bInterfaceNumber);
    ASSERT_EQ(0, rc) << "Failed to claim Interface " << m_interfaceDescriptor->bInterfaceNumber << "(rc=" << rc << ")";

    /* TODO Query the Device's Characteristics */
    m_maxBufferSz   = 2 * m_bulkOutEndpoint->wMaxPacketSize;
    m_txTimeout     = 250;
    m_rxTimeout     = 250;
}

void
UsbDeviceTest::parseConfigDescriptor(void) {
    ASSERT_EQ(m_activeConfiguration, m_configDescriptor->bConfigurationValue)
      << "Expected Configuration #" << m_activeConfiguration
      << " to be active but Device announced Configuration #" << m_configDescriptor->bConfigurationValue;

    ASSERT_LT(0, m_configDescriptor->bNumInterfaces) << "Expected at least one interface in Device's Config Descriptor";
    for (unsigned idx = 0; idx < m_configDescriptor->bNumInterfaces; idx++) {
        const struct libusb_interface * const interface = m_configDescriptor->interface + idx;
        EXPECT_LT(0, interface->num_altsetting);

        const struct libusb_interface_descriptor * const interfaceDescriptor = interface->altsetting;
        if ((interfaceDescriptor->bInterfaceClass == m_interfaceClass)
          && (interfaceDescriptor->bInterfaceProtocol == m_interfaceProtocol)
          && (interfaceDescriptor->bInterfaceSubClass == m_interfaceSubClass)) {
              this->m_interfaceDescriptor = interfaceDescriptor;
              break;
        }
    }
    ASSERT_NE(nullptr, this->m_interfaceDescriptor);
}

void
UsbDeviceTest::parseInterfaceDescriptor(void) {
    ASSERT_LT(0, m_interfaceDescriptor->bNumEndpoints)
      << "Expected Device to have at least 2 Endpoints, but only found " << m_interfaceDescriptor->bNumEndpoints;
    for (unsigned idx = 0;
      (idx < m_interfaceDescriptor->bNumEndpoints) && ((nullptr == m_bulkInEndpoint) || (nullptr == m_bulkOutEndpoint));
      idx++)
    {
        const struct libusb_endpoint_descriptor * const endpt = m_interfaceDescriptor->endpoint + idx;
        ASSERT_NE(nullptr, endpt);

        libusb_endpoint_direction dir = getEndpointDirection(*endpt);
        libusb_transfer_type type = getEndpointType(*endpt);

        if (type == LIBUSB_TRANSFER_TYPE_BULK) {
            if (dir == LIBUSB_ENDPOINT_OUT) {
                m_bulkOutEndpoint = endpt;
            } else if (dir == LIBUSB_ENDPOINT_IN) {
                m_bulkInEndpoint = endpt;
            }
        }
    }
    ASSERT_NE(nullptr, m_bulkOutEndpoint);
    ASSERT_NE(nullptr, m_bulkInEndpoint);
}

void
UsbDeviceTest::closeDevice(void) {
    if (m_dutHandle != nullptr) {
        libusb_close(m_dutHandle);
    }

    if (m_dutRef != nullptr) {
        libusb_unref_device(m_dutRef);
    }

    if (m_devs != nullptr) {
        libusb_free_device_list(m_devs, 1);
    }
}

void
UsbDeviceTest::libUsbCleanUp(void) {
    /* Release the USB Device's Test Interface */
    if (nullptr != m_interfaceDescriptor) {
        ASSERT_NE(nullptr, m_dutHandle);
        int rc = libusb_release_interface(m_dutHandle, m_interfaceDescriptor->bInterfaceNumber);
        EXPECT_EQ(0, rc);
    }

    /* Free the USB Configuration Descriptor */
    if (nullptr != m_configDescriptor) {
        libusb_free_config_descriptor(const_cast<libusb_config_descriptor *>(m_configDescriptor));
    }

    /* Reset the USB Device's Configuration / Reset the Device to the "unconfigured" state */
    resetDeviceConfiguration();

    /* Close Device */
    closeDevice();

    /* Tear-down libusb Stack */
    if (m_ctx != nullptr) {
        libusb_exit(m_ctx);
    }
}
