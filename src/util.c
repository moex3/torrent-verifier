#include <stdio.h>
#include "util.h"

#define B_IN_KiB 1024ull
#define B_IN_MiB B_IN_KiB * 1024
#define B_IN_GiB B_IN_MiB * 1024
#define B_IN_TiB B_IN_GiB * 1024

#define B_IN_KB 1000ull
#define B_IN_MB B_IN_KB * 1000
#define B_IN_GB B_IN_MB * 1000
#define B_IN_TB B_IN_GB * 1000


int util_byte2human(long int bytes, int binary, int precision, char* out, size_t out_len) {
#define S_CONV(t) ((binary) ? (B_IN_##t##iB) : (B_IN_##t##B))
#define S_LESS(t) (bytes < S_CONV(t))
#define S_SUFFIX(t) ((binary) ? (t "iB") : (t "B"))
    if (!out)
        return -1;
    
    int written;
    if (S_LESS(K))
        written = snprintf(out, out_len, "%ld B", bytes);
    else if (S_LESS(M))
        written = snprintf(out, out_len, "%.*f %s", (precision == -1) ? 0 : precision, bytes / (double)(S_CONV(K)), S_SUFFIX("K"));
    else if (S_LESS(G))
        written = snprintf(out, out_len, "%.*f %s", (precision == -1) ? 1 : precision, bytes / (double)(S_CONV(M)), S_SUFFIX("M"));
    else if (S_LESS(T))
        written = snprintf(out, out_len, "%.*f %s", (precision == -1) ? 2 : precision, bytes / (double)(S_CONV(G)), S_SUFFIX("G"));
    else
        written = snprintf(out, out_len, "%.*f %s", (precision == -1) ? 3 : precision, bytes / (double)(S_CONV(T)), S_SUFFIX("T"));

    return (written >= out_len) ? -1 : written;
#undef S_SUFFIX
#undef S_LESS
#undef S_CONV
}

void util_byte2hex(const unsigned char* bytes, int bytes_len, int uppercase, char* out) {
    const char* hex = (uppercase) ? "0123456789ABCDEF" : "0123456789abcdef";
    for (int i = 0; i < bytes_len; i++) {
        *out++ = hex[bytes[i] >> 4];
        *out++ = hex[bytes[i] & 0xF];
    }
    *out = '\0';
}
