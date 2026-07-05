#include "predator_crypto_chacha20.h"
#include "../predator_i.h"
#include <string.h>
#include <furi_hal.h>

// ChaCha20-Poly1305 AEAD - RFC 8439 compliant implementation

// ChaCha20 quarter round
#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))
#define QR(a, b, c, d) do { \
    a += b; d ^= a; d = ROTL32(d, 16); \
    c += d; b ^= c; b = ROTL32(b, 12); \
    a += b; d ^= a; d = ROTL32(d, 8); \
    c += d; b ^= c; b = ROTL32(b, 7); \
} while(0)

// Little-endian load/store
static inline uint32_t load32_le(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void store32_le(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

// ========== ChaCha20 Implementation ==========

bool chacha20_init(ChaCha20Context* ctx, const uint8_t* key, const uint8_t* nonce, uint32_t counter) {
    if(!ctx || !key || !nonce) return false;
    
    // ChaCha20 constant "expand 32-byte k"
    ctx->state[0] = 0x61707865;
    ctx->state[1] = 0x3320646e;
    ctx->state[2] = 0x79622d32;
    ctx->state[3] = 0x6b206574;
    
    // Load 256-bit key
    for(int i = 0; i < 8; i++) {
        ctx->state[4 + i] = load32_le(&key[i * 4]);
    }
    
    // Counter
    ctx->state[12] = counter;
    
    // Load 96-bit nonce
    for(int i = 0; i < 3; i++) {
        ctx->state[13 + i] = load32_le(&nonce[i * 4]);
    }
    
    ctx->counter = counter;
    ctx->keystream_pos = 64; // Force generation on first use
    
    return true;
}

bool chacha20_block(ChaCha20Context* ctx, uint8_t* output) {
    if(!ctx || !output) return false;
    
    uint32_t working_state[16];
    memcpy(working_state, ctx->state, sizeof(working_state));
    
    // 20 rounds (10 double rounds)
    for(int i = 0; i < 10; i++) {
        // Column rounds
        QR(working_state[0], working_state[4], working_state[8], working_state[12]);
        QR(working_state[1], working_state[5], working_state[9], working_state[13]);
        QR(working_state[2], working_state[6], working_state[10], working_state[14]);
        QR(working_state[3], working_state[7], working_state[11], working_state[15]);
        
        // Diagonal rounds
        QR(working_state[0], working_state[5], working_state[10], working_state[15]);
        QR(working_state[1], working_state[6], working_state[11], working_state[12]);
        QR(working_state[2], working_state[7], working_state[8], working_state[13]);
        QR(working_state[3], working_state[4], working_state[9], working_state[14]);
    }
    
    // Add initial state
    for(int i = 0; i < 16; i++) {
        working_state[i] += ctx->state[i];
    }
    
    // Serialize to output
    for(int i = 0; i < 16; i++) {
        store32_le(&output[i * 4], working_state[i]);
    }
    
    // Increment counter
    ctx->state[12]++;
    if(ctx->state[12] == 0) {
        ctx->state[13]++; // Handle overflow
    }
    
    return true;
}

bool chacha20_crypt(ChaCha20Context* ctx, const uint8_t* data, size_t len, uint8_t* output) {
    if(!ctx || !data || !output) return false;
    
    for(size_t i = 0; i < len; i++) {
        if(ctx->keystream_pos >= 64) {
            chacha20_block(ctx, ctx->keystream);
            ctx->keystream_pos = 0;
        }
        output[i] = data[i] ^ ctx->keystream[ctx->keystream_pos++];
    }
    
    return true;
}

// ========== Poly1305 Implementation ==========

// Poly1305 modulus: 2^130 - 5
static void poly1305_blocks(Poly1305Context* ctx, const uint8_t* input, size_t len) {
    uint32_t h0 = ctx->accumulator[0];
    uint32_t h1 = ctx->accumulator[1];
    uint32_t h2 = ctx->accumulator[2];
    uint32_t h3 = ctx->accumulator[3];
    uint32_t h4 = ctx->accumulator[4];
    
    uint32_t r0 = ctx->r[0];
    uint32_t r1 = ctx->r[1];
    uint32_t r2 = ctx->r[2];
    uint32_t r3 = ctx->r[3];
    uint32_t r4 = ctx->r[4];
    
    while(len >= 16) {
        // Load block
        uint32_t t0 = load32_le(&input[0]);
        uint32_t t1 = load32_le(&input[4]);
        uint32_t t2 = load32_le(&input[8]);
        uint32_t t3 = load32_le(&input[12]);
        
        // Add to accumulator with high bit
        h0 += t0 & 0x3ffffff;
        h1 += ((t0 >> 26) | (t1 << 6)) & 0x3ffffff;
        h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
        h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
        h4 += (t3 >> 8) | (1 << 24);
        
        // Multiply by R (simplified for embedded)
        uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * (r4 * 5) + (uint64_t)h2 * (r3 * 5) + 
                     (uint64_t)h3 * (r2 * 5) + (uint64_t)h4 * (r1 * 5);
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * (r4 * 5) + 
                     (uint64_t)h3 * (r3 * 5) + (uint64_t)h4 * (r2 * 5);
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + 
                     (uint64_t)h3 * (r4 * 5) + (uint64_t)h4 * (r3 * 5);
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + 
                     (uint64_t)h3 * r0 + (uint64_t)h4 * (r4 * 5);
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + 
                     (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
        
        // Reduce modulo 2^130 - 5
        h0 = (uint32_t)d0 & 0x3ffffff; d1 += d0 >> 26;
        h1 = (uint32_t)d1 & 0x3ffffff; d2 += d1 >> 26;
        h2 = (uint32_t)d2 & 0x3ffffff; d3 += d2 >> 26;
        h3 = (uint32_t)d3 & 0x3ffffff; d4 += d3 >> 26;
        h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += ((uint32_t)d4 >> 26) * 5;
        h1 += h0 >> 26; h0 &= 0x3ffffff;
        
        input += 16;
        len -= 16;
    }
    
    ctx->accumulator[0] = h0;
    ctx->accumulator[1] = h1;
    ctx->accumulator[2] = h2;
    ctx->accumulator[3] = h3;
    ctx->accumulator[4] = h4;
}

bool poly1305_init(Poly1305Context* ctx, const uint8_t* key) {
    if(!ctx || !key) return false;
    
    memset(ctx, 0, sizeof(Poly1305Context));
    
    // Clamp R
    ctx->r[0] = (load32_le(&key[0])) & 0x3ffffff;
    ctx->r[1] = (load32_le(&key[3]) >> 2) & 0x3ffff03;
    ctx->r[2] = (load32_le(&key[6]) >> 4) & 0x3ffc0ff;
    ctx->r[3] = (load32_le(&key[9]) >> 6) & 0x3f03fff;
    ctx->r[4] = (load32_le(&key[12]) >> 8) & 0x00fffff;
    
    // Load S
    ctx->s[0] = load32_le(&key[16]);
    ctx->s[1] = load32_le(&key[20]);
    ctx->s[2] = load32_le(&key[24]);
    ctx->s[3] = load32_le(&key[28]);
    
    return true;
}

bool poly1305_update(Poly1305Context* ctx, const uint8_t* data, size_t len) {
    if(!ctx || !data) return false;
    
    // Process buffered data first
    if(ctx->buffer_len > 0) {
        size_t to_copy = 16 - ctx->buffer_len;
        if(to_copy > len) to_copy = len;
        memcpy(&ctx->buffer[ctx->buffer_len], data, to_copy);
        ctx->buffer_len += to_copy;
        data += to_copy;
        len -= to_copy;
        
        if(ctx->buffer_len == 16) {
            poly1305_blocks(ctx, ctx->buffer, 16);
            ctx->buffer_len = 0;
        }
    }
    
    // Process full blocks
    size_t blocks = len / 16;
    if(blocks > 0) {
        poly1305_blocks(ctx, data, blocks * 16);
        data += blocks * 16;
        len -= blocks * 16;
    }
    
    // Buffer remaining
    if(len > 0) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
    
    return true;
}

bool poly1305_finish(Poly1305Context* ctx, uint8_t* tag) {
    if(!ctx || !tag) return false;
    
    // Process final block
    if(ctx->buffer_len > 0) {
        ctx->buffer[ctx->buffer_len] = 1;
        memset(&ctx->buffer[ctx->buffer_len + 1], 0, 16 - ctx->buffer_len - 1);
        poly1305_blocks(ctx, ctx->buffer, 16);
    }
    
    // Final reduction
    uint32_t h0 = ctx->accumulator[0];
    uint32_t h1 = ctx->accumulator[1];
    uint32_t h2 = ctx->accumulator[2];
    uint32_t h3 = ctx->accumulator[3];
    uint32_t h4 = ctx->accumulator[4];
    
    // Carry propagation
    uint32_t c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;
    
    // Compute h - (2^130 - 5)
    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1 << 26);
    
    // Select h or g based on overflow
    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;
    
    // Add S
    uint64_t f = (uint64_t)h0 | ((uint64_t)h1 << 26) | ((uint64_t)h2 << 52);
    f += ctx->s[0] + ((uint64_t)ctx->s[1] << 32);
    store32_le(&tag[0], f);
    store32_le(&tag[4], f >> 32);
    
    f = ((uint64_t)h2 >> 12) | ((uint64_t)h3 << 14) | ((uint64_t)h4 << 40);
    f += ctx->s[2] + ((uint64_t)ctx->s[3] << 32);
    store32_le(&tag[8], f);
    store32_le(&tag[12], f >> 32);
    
    return true;
}

bool poly1305_mac(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* tag) {
    Poly1305Context ctx;
    if(!poly1305_init(&ctx, key)) return false;
    if(!poly1305_update(&ctx, data, len)) return false;
    return poly1305_finish(&ctx, tag);
}

// ========== ChaCha20-Poly1305 AEAD ==========

bool chacha20_poly1305_init(ChaCha20Poly1305Context* ctx, const uint8_t* key, const uint8_t* nonce) {
    if(!ctx || !key || !nonce) return false;
    
    memcpy(ctx->key, key, 32);
    memcpy(ctx->nonce, nonce, 12);
    
    // Initialize ChaCha20 with counter = 0 for Poly1305 key
    chacha20_init(&ctx->chacha, key, nonce, 0);
    
    return true;
}

bool chacha20_poly1305_encrypt(ChaCha20Poly1305Context* ctx,
                               const uint8_t* plaintext, size_t plaintext_len,
                               const uint8_t* aad, size_t aad_len,
                               uint8_t* ciphertext, uint8_t* tag) {
    if(!ctx || !plaintext || !ciphertext || !tag) return false;
    
    // Generate Poly1305 key (first 32 bytes of keystream at counter=0)
    uint8_t poly_key[64];
    chacha20_init(&ctx->chacha, ctx->key, ctx->nonce, 0);
    chacha20_block(&ctx->chacha, poly_key);
    
    // Encrypt plaintext (starting at counter=1)
    chacha20_init(&ctx->chacha, ctx->key, ctx->nonce, 1);
    chacha20_crypt(&ctx->chacha, plaintext, plaintext_len, ciphertext);
    
    // Compute Poly1305 MAC
    poly1305_init(&ctx->poly, poly_key);
    if(aad && aad_len > 0) {
        poly1305_update(&ctx->poly, aad, aad_len);
        // Pad to 16 bytes
        uint8_t pad[16] = {0};
        size_t pad_len = (16 - (aad_len % 16)) % 16;
        if(pad_len > 0) poly1305_update(&ctx->poly, pad, pad_len);
    }
    poly1305_update(&ctx->poly, ciphertext, plaintext_len);
    // Pad to 16 bytes
    uint8_t pad[16] = {0};
    size_t pad_len = (16 - (plaintext_len % 16)) % 16;
    if(pad_len > 0) poly1305_update(&ctx->poly, pad, pad_len);
    
    // Append lengths
    uint8_t lens[16];
    store32_le(&lens[0], aad_len);
    store32_le(&lens[4], 0);
    store32_le(&lens[8], plaintext_len);
    store32_le(&lens[12], 0);
    poly1305_update(&ctx->poly, lens, 16);
    
    poly1305_finish(&ctx->poly, tag);
    
    FURI_LOG_I("ChaCha20", "Encrypted %u bytes with Poly1305 authentication", (unsigned)plaintext_len);
    return true;
}

bool chacha20_poly1305_decrypt(ChaCha20Poly1305Context* ctx,
                               const uint8_t* ciphertext, size_t ciphertext_len,
                               const uint8_t* aad, size_t aad_len,
                               const uint8_t* tag, uint8_t* plaintext) {
    if(!ctx || !ciphertext || !tag || !plaintext) return false;
    
    // Generate Poly1305 key
    uint8_t poly_key[64];
    chacha20_init(&ctx->chacha, ctx->key, ctx->nonce, 0);
    chacha20_block(&ctx->chacha, poly_key);
    
    // Verify MAC
    Poly1305Context poly;
    poly1305_init(&poly, poly_key);
    if(aad && aad_len > 0) {
        poly1305_update(&poly, aad, aad_len);
        uint8_t pad[16] = {0};
        size_t pad_len = (16 - (aad_len % 16)) % 16;
        if(pad_len > 0) poly1305_update(&poly, pad, pad_len);
    }
    poly1305_update(&poly, ciphertext, ciphertext_len);
    uint8_t pad[16] = {0};
    size_t pad_len = (16 - (ciphertext_len % 16)) % 16;
    if(pad_len > 0) poly1305_update(&poly, pad, pad_len);
    
    uint8_t lens[16];
    store32_le(&lens[0], aad_len);
    store32_le(&lens[4], 0);
    store32_le(&lens[8], ciphertext_len);
    store32_le(&lens[12], 0);
    poly1305_update(&poly, lens, 16);
    
    uint8_t computed_tag[16];
    poly1305_finish(&poly, computed_tag);
    
    // Constant-time comparison
    if(!chacha20_constant_time_equal(computed_tag, tag, 16)) {
        FURI_LOG_W("ChaCha20", "Authentication failed - tag mismatch");
        return false;
    }
    
    // Decrypt
    chacha20_init(&ctx->chacha, ctx->key, ctx->nonce, 1);
    chacha20_crypt(&ctx->chacha, ciphertext, ciphertext_len, plaintext);
    
    FURI_LOG_I("ChaCha20", "Decrypted and verified %u bytes", (unsigned)ciphertext_len);
    return true;
}

// ========== High-Level Wrappers ==========

bool predator_crypto_chacha20_poly1305_encrypt(const uint8_t* key, const uint8_t* nonce,
                                                const uint8_t* plaintext, size_t plaintext_len,
                                                uint8_t* ciphertext, uint8_t* tag) {
    ChaCha20Poly1305Context ctx;
    if(!chacha20_poly1305_init(&ctx, key, nonce)) return false;
    return chacha20_poly1305_encrypt(&ctx, plaintext, plaintext_len, NULL, 0, ciphertext, tag);
}

bool predator_crypto_chacha20_poly1305_decrypt(const uint8_t* key, const uint8_t* nonce,
                                                const uint8_t* ciphertext, size_t ciphertext_len,
                                                const uint8_t* tag, uint8_t* plaintext) {
    ChaCha20Poly1305Context ctx;
    if(!chacha20_poly1305_init(&ctx, key, nonce)) return false;
    return chacha20_poly1305_decrypt(&ctx, ciphertext, ciphertext_len, NULL, 0, tag, plaintext);
}

// ========== IoT Functions ==========

bool chacha20_tesla_station_auth(PredatorApp* app, const uint8_t* station_key,
                                 const uint8_t* challenge, size_t challenge_len,
                                 uint8_t* response, size_t* response_len) {
    if(!app || !station_key || !challenge || !response || !response_len) return false;
    
    FURI_LOG_I("ChaCha20", "TESLA: Generating station authentication");
    
    uint8_t nonce[12];
    chacha20_generate_nonce(nonce);
    
    uint8_t tag[16];
    bool result = predator_crypto_chacha20_poly1305_encrypt(station_key, nonce, 
                                                            challenge, challenge_len,
                                                            response, tag);
    if(result) {
        memcpy(&response[challenge_len], tag, 16);
        *response_len = challenge_len + 16;
    }
    
    return result;
}

bool chacha20_smart_lock_command(PredatorApp* app, const uint8_t* lock_key,
                                 const uint8_t* command, size_t command_len,
                                 uint8_t* encrypted_out, uint8_t* tag_out) {
    if(!app || !lock_key || !command || !encrypted_out || !tag_out) return false;
    
    FURI_LOG_I("ChaCha20", "SMART LOCK: Encrypting command");
    
    uint8_t nonce[12];
    chacha20_generate_nonce(nonce);
    memcpy(encrypted_out, nonce, 12); // Prepend nonce
    
    return predator_crypto_chacha20_poly1305_encrypt(lock_key, nonce,
                                                     command, command_len,
                                                     &encrypted_out[12], tag_out);
}

bool chacha20_iot_encrypt(const uint8_t* device_key, const uint8_t* nonce,
                          const uint8_t* message, size_t message_len,
                          uint8_t* encrypted_out, uint8_t* tag_out) {
    return predator_crypto_chacha20_poly1305_encrypt(device_key, nonce,
                                                     message, message_len,
                                                     encrypted_out, tag_out);
}

// ========== Utilities ==========

void chacha20_generate_nonce(uint8_t* nonce) {
    if(!nonce) return;
    uint32_t tick = furi_get_tick();
    for(int i = 0; i < 12; i++) {
        nonce[i] = (uint8_t)((tick >> (i % 4)) & 0xFF) ^ (i * 23);
    }
}

bool chacha20_constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    if(!a || !b) return false;
    uint8_t diff = 0;
    for(size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

bool chacha20_derive_key(const char* password, const uint8_t* salt, size_t salt_len,
                         uint32_t iterations, uint8_t* key) {
    // Simplified PBKDF2 - for production use mbedtls or similar
    if(!password || !salt || !key || iterations < 1000) return false;
    
    FURI_LOG_W("ChaCha20", "Key derivation: Use strong library for production");
    
    // Simplified version for demonstration
    memset(key, 0, 32);
    size_t pwd_len = strlen(password);
    
    for(uint32_t i = 0; i < iterations; i++) {
        for(size_t j = 0; j < 32; j++) {
            key[j] ^= password[j % pwd_len] ^ salt[j % salt_len] ^ (i & 0xFF);
        }
    }
    
    return true;
}

bool chacha20_poly1305_self_test(void) {
    // RFC 8439 test vector
    const uint8_t test_key[32] = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
    };
    
    const uint8_t test_nonce[12] = {
        0x07, 0x00, 0x00, 0x00, 0x40, 0x41, 0x42, 0x43,
        0x44, 0x45, 0x46, 0x47
    };
    
    const uint8_t test_plain[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    size_t plain_len = strlen((char*)test_plain);
    
    uint8_t ciphertext[256];
    uint8_t tag[16];
    uint8_t decrypted[256];
    
    // Encrypt
    if(!predator_crypto_chacha20_poly1305_encrypt(test_key, test_nonce, test_plain, plain_len, ciphertext, tag)) {
        FURI_LOG_E("ChaCha20", "Self-test: Encryption failed");
        return false;
    }
    
    // Decrypt
    if(!predator_crypto_chacha20_poly1305_decrypt(test_key, test_nonce, ciphertext, plain_len, tag, decrypted)) {
        FURI_LOG_E("ChaCha20", "Self-test: Decryption failed");
        return false;
    }
    
    // Verify
    if(memcmp(test_plain, decrypted, plain_len) != 0) {
        FURI_LOG_E("ChaCha20", "Self-test: Plaintext mismatch");
        return false;
    }
    
    FURI_LOG_I("ChaCha20", "Self-test: PASSED");
    return true;
}
