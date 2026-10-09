#include "usb/usb_desc.h"
#include "lib/string.h"

#define DT_DEVICE 1
#define DT_CONFIG 2
#define DT_STRING 3
#define DT_INTERFACE 4
#define DT_ENDPOINT 5
#define DT_HUB 0x29

bool usb_desc_device(const uint8_t *b, size_t len, struct usb_device_info *out) {
    if (len < 18 || b[0] < 18 || b[1] != DT_DEVICE)
        return false;
    out->dev_class = b[4];
    out->sub_class = b[5];
    out->protocol = b[6];
    out->mps0 = b[7];
    out->vid = (uint16_t)(b[8] | b[9] << 8);
    out->pid = (uint16_t)(b[10] | b[11] << 8);
    out->i_manufacturer = b[14];
    out->i_product = b[15];
    return true;
}

bool usb_desc_config(const uint8_t *b, size_t len, struct usb_config_info *out) {
    memset(out, 0, sizeof *out);
    if (len < 9 || b[0] < 9 || b[1] != DT_CONFIG)
        return false;
    size_t total = (size_t)(b[2] | b[3] << 8);
    if (total < 9)
        return false;
    if (total > len)
        total = len;
    out->value = b[5];

    bool in_iface = false;
    uint8_t iface = 0, alt = 0, cls = 0, sub = 0, proto = 0;
    for (size_t i = b[0]; i + 2 <= total;) {
        const uint8_t *d = b + i;
        size_t l = d[0];
        if (l < 2 || l > total - i)
            break;
        if (d[1] == DT_INTERFACE && l >= 9) {
            in_iface = true;
            iface = d[2];
            alt = d[3];
            cls = d[5];
            sub = d[6];
            proto = d[7];
            if (alt == 0 && !out->has_iface) {
                out->has_iface = true;
                out->iface_class = cls;
                out->iface_sub = sub;
                out->iface_proto = proto;
            }
        } else if (d[1] == DT_ENDPOINT && l >= 7 && in_iface && alt == 0) {
            struct usb_ep_info ep = {
                .addr = d[2],
                .mps = (uint16_t)((d[4] | d[5] << 8) & 0x7FF),
                .interval = d[6],
            };
            if ((d[3] & 3) == 3 && (ep.addr & 0x80) && ep.mps) {
                if (cls == 3 && sub == 1 && proto == 1 && !out->has_kbd) {
                    out->has_kbd = true;
                    out->kbd_iface = iface;
                    out->kbd_ep = ep;
                } else if (cls == 3 && sub == 1 && proto == 2 && !out->has_mouse) {
                    out->has_mouse = true;
                    out->mouse_iface = iface;
                    out->mouse_ep = ep;
                } else if (cls == 9 && !out->has_hub) {
                    out->has_hub = true;
                    out->hub_ep = ep;
                }
            }
        }
        i += l;
    }
    return true;
}

bool usb_desc_hub(const uint8_t *b, size_t len, struct usb_hub_desc *out) {
    if (len < 7 || b[0] < 7 || b[1] != DT_HUB)
        return false;
    out->ports = b[2] > 15 ? 15 : b[2];
    out->ttt = (b[3] >> 5) & 3;
    out->pwr_good_ms = (uint16_t)(b[5] * 2);
    return true;
}

bool usb_desc_string(const uint8_t *b, size_t len, char *out, size_t cap) {
    if (!cap)
        return false;
    out[0] = 0;
    if (len < 2 || b[0] < 2 || b[1] != DT_STRING)
        return false;
    size_t end = b[0] < len ? b[0] : len, n = 0, start = 0;
    for (size_t i = 2; i + 1 < end && n + 1 < cap; i += 2) {
        uint16_t c = (uint16_t)(b[i] | b[i + 1] << 8);
        if (c < 0x20 || c == 0x7F)
            continue;
        out[n++] = c < 0x80 ? (char)c : '?';
    }
    while (n && out[n - 1] == ' ')
        n--;
    while (start < n && out[start] == ' ')
        start++;
    memmove(out, out + start, n - start);
    out[n - start] = 0;
    return n > start;
}

uint16_t usb_desc_langid(const uint8_t *b, size_t len) {
    if (len < 4 || b[0] < 4 || b[1] != DT_STRING)
        return 0;
    return (uint16_t)(b[2] | b[3] << 8);
}

const char *usb_class_name(uint8_t cls, uint8_t sub, uint8_t proto) {
    switch (cls) {
    case 0x01: return "Audio";
    case 0x02: return "Communications";
    case 0x03: return "HID";
    case 0x05: return "Physical";
    case 0x06: return "Imaging";
    case 0x07: return "Printer";
    case 0x08: return "Mass storage";
    case 0x09: return "Hub";
    case 0x0A: return "CDC data";
    case 0x0B: return "Smart card";
    case 0x0D: return "Content security";
    case 0x0E: return "Video";
    case 0x0F: return "Healthcare";
    case 0x10: return "Audio/video";
    case 0x11: return "Billboard";
    case 0x12: return "USB-C bridge";
    case 0xDC: return "Diagnostic";
    case 0xE0: return sub == 1 && proto == 1 ? "Bluetooth" : "Wireless";
    case 0xEF: return "Miscellaneous";
    case 0xFE: return "Application specific";
    case 0xFF: return "Vendor specific";
    default: return NULL;
    }
}
