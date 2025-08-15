// base64_simple.h
// Simple Base64 encoding for WebSocket handshake

#ifndef BASE64_SIMPLE_H
#define BASE64_SIMPLE_H

#include <Arduino.h>

namespace base64 {
    
const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline String encode(const uint8_t* data, size_t length) {
    String encoded;
    encoded.reserve(((length + 2) / 3) * 4);
    
    for (size_t i = 0; i < length; i += 3) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        encoded += b64_alphabet[(triple >> 18) & 0x3F];
        encoded += b64_alphabet[(triple >> 12) & 0x3F];
        encoded += (i + 1 < length) ? b64_alphabet[(triple >> 6) & 0x3F] : '=';
        encoded += (i + 2 < length) ? b64_alphabet[triple & 0x3F] : '=';
    }
    
    return encoded;
}

} // namespace base64

#endif // BASE64_SIMPLE_H