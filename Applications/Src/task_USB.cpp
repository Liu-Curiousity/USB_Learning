//
// Created by 26757 on 2025/7/21.
//
#include <algorithm>

#include "task_public.h"
#include "usb_otg.h"

class USB_Device {
public:
    enum class bRequestType :uint8_t {
        GET_STATUS = 0,
        CLEAR_FEATURE = 1,
        SET_FEATURE = 3,
        SET_ADDRESS = 5,
        GET_DESCRIPTOR = 6,
        SET_DESCRIPTOR = 7,
        GET_CONFIGURATION = 8,
        SET_CONFIGURATION = 9,
        GET_INTERFACE = 10,
        SET_INTERFACE = 11,
        SYNCH_FRAME = 12,
    };

    enum class DescriptorType :uint16_t {
        DEVICE = 0x0100,
        CONFIGURATION = 0x0200,
        STRING = 0x0300,
        INTERFACE = 0x0400,
        ENDPOINT = 0x0500,
        DEVICE_QUALIFIER = 0x0600,
    };

    uint8_t device_descriptor[18] = {
        0x12, // bLength
        0x01, // bDescriptorType (Device)
        0x00, // bcdUSB (USB 2.0)
        0x02, // bcdUSB (USB 2.0)
        0x00, // bDeviceClass (Defined at Interface level)
        0x00, // bDeviceSubClass
        0x00, // bDeviceProtocol
        0x40, // bMaxPacketSize0 (64 bytes)
        0x88, // Vendor ID (example value)
        0x88, // Vendor ID (example value)
        0x01, // Product ID (example value)
        0x00, // Product ID (example value)
        0x00, // bcdDevice (Version 1.0)
        0x01, // bcdDevice (Version 1.0)
        0x00, // iManufacturer (Index of Manufacturer String Descriptor)
        0x00, // iProduct (Index of Product String Descriptor)
        0x00, // iSerialNumber (Index of Serial Number String Descriptor)
        0x01, // bNumConfigurations (1 configuration)
    };

    uint8_t configure_descriptor[34] = {
        /* ================= Configuration Descriptor ================= */
        0x09,                                 // bLength
        0x02,                                 // bDescriptorType (Configuration)
        sizeof (configure_descriptor) & 0xFF, // wTotalLength
        sizeof (configure_descriptor) >> 8,   // wTotalLength
        0x01,                                 // bNumInterfaces (1 interface)
        0x01,                                 // bConfigurationValue
        0x00,                                 // iConfiguration
        0b1100'0000,                          // bmAttributes
        0x64,                                 // bMaxPower (200 mA)

        /* ================= Interface Descriptor ================= */
        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x00, // bInterfaceNumber (Interface 0)
        0x00, // bAlternateSetting
        0x01, // bNumEndpoints (1 endpoint)
        0x03, // bInterfaceClass (Example: CDC class)
        0x00, // bInterfaceSubClass
        0x00, // bInterfaceProtocol
        0b00, // iInterface (Index of Interface String Descriptor)

        /* ================= HID Descriptor ================= */
        0x09, // bLength
        0x21, // bDescriptorType (HID)
        0x10, // bcdHID (HID version 1.1)
        0x01, // bcdHID (HID version 1.1)
        0x21, // bCountryCode (Country code, USA)
        0x01, // bNumDescriptors (1 HID descriptor)
        0x22, // bDescriptorType (Report descriptor)
        0x00, // wDescriptorLength (0 bytes)
        0x00, // wDescriptorLength (0 bytes)

        /* ================= EndPort Descriptor ================= */
        0x07, // bLength
        0x05, // bDescriptorType (Endpoint)
        0x01, // bEndpointAddress (OUT endpoint 1)
        0x02, // bmAttributes (Bulk)
        0x01, // wMaxPacketSize (256 bytes)
        0x00, // wMaxPacketSize (256 bytes)
        0xFF, // bInterval
    };
};

USB_Device My_USB_Device;

void StartUSBTask(void *argument) {
    HAL_PCD_Start(&hpcd_USB_OTG_FS);
    while (true) {
        delay(10);
    }
}

// 起始帧
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) {}
// // 控制传输建立阶段
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
    const uint8_t bRequestType = hpcd->Setup[0] & 0xFF;           // 请求类型
    const uint8_t bRequest = hpcd->Setup[0] >> 8;                 // 请求码
    const uint16_t wValue = (hpcd->Setup[0] & 0xFFFF0000) >> 16;  // 请求值
    const uint16_t wIndex = hpcd->Setup[1] & 0x0000FFFF;          // 请求索引
    const uint16_t wLength = (hpcd->Setup[1] & 0xFFFF0000) >> 16; // 请求长度

    switch (static_cast<USB_Device::bRequestType>(bRequest)) {
        case USB_Device::bRequestType::GET_DESCRIPTOR: {
            // 获取描述符
            uint8_t *descriptor = nullptr;
            uint16_t descriptorLength = 0;
            switch (static_cast<USB_Device::DescriptorType>(wValue)) {
                case USB_Device::DescriptorType::DEVICE:
                    descriptor = My_USB_Device.device_descriptor;
                    descriptorLength = sizeof(My_USB_Device.device_descriptor);
                    break;
                case USB_Device::DescriptorType::CONFIGURATION:
                    descriptor = My_USB_Device.configure_descriptor;
                    descriptorLength = sizeof(My_USB_Device.configure_descriptor);
                    break;
                case USB_Device::DescriptorType::DEVICE_QUALIFIER:
                    HAL_PCD_EP_SetStall(hpcd, 0x80); // 设置端点为STALL状态
                    HAL_PCD_EP_SetStall(hpcd, 0x00); // 设置端点为STALL状态
                    break;
                default:
                    break;
            }
            if (descriptor != nullptr)
                HAL_PCD_EP_Transmit(hpcd, 0x80, descriptor, std::min(wLength, descriptorLength));
            break;
        }
        case USB_Device::bRequestType::SET_ADDRESS: // 设置地址
            HAL_PCD_EP_Transmit(hpcd, 0x00, nullptr, 0);
            // TODO: 应该改成成功发送后再设置地址
            HAL_PCD_SetAddress(hpcd, wValue); // 设置设备地址
            break;
        default:
            break;
    }
}

// USB 复位
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
    HAL_PCD_EP_Open(hpcd, 0x00, 64,EP_TYPE_CTRL); // 控制端点OUT
    HAL_PCD_EP_Open(hpcd, 0x80, 64,EP_TYPE_CTRL); // 控制端点IN
    HAL_PCD_SetAddress(hpcd, 0);                  // 设置设备地址为0
}

// 挂起事件
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {}
// 恢复事件
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {}
// USB 连接事件
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) {}
// USB 断开事件
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd) {}


// 数据 OUT 阶段回调
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    if (epnum == 0) {
        // TODO: 未处理"控制写"的控制传输
    }
}

// 数据 IN 阶段回调
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    if (epnum == 0) {
        // TODO: 不是每次都需要接收数据,不适用于"无数据写"的控制传输
        HAL_PCD_EP_Receive(hpcd, 0x00, nullptr, 0);
    }
}

// ISO OUT 不完整回调
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {}
// ISO IN 不完整回调
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {}
