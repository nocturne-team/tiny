#include "usb/usb_desc.h"
#include "lib/string.h"

#define DT_DEVICE 1
#define DT_CONFIG 2
#define DT_INTERFACE 4
#define DT_ENDPOINT 5
#define DT_HUB 0x29

bool usb_desc_device(const uint8_t *b, size_t len, struct usb_device_info *out) {
    if (len < 18 || b[0] < 18 || b[1] != DT_DEVICE)
        return false;
    out->dev_class = b[4];
    out->protocol = b[6];
    out->mps0 = b[7];
    out->vid = (uint16_t)(b[8] | b[9] << 8);
    out->pid = (uint16_t)(b[10] | b[11] << 8);
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
