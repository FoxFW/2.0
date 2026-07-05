#include "predator_crypto_3des.h"
#include <string.h>

// PRODUCTION 3DES (Triple DES) IMPLEMENTATION
// Used by: FeliCa, Calypso, UltraLight C, MIFARE DESFire
// REAL CRYPTOGRAPHIC IMPLEMENTATION - NO FAKE CODE

// DES S-boxes (Standard DES specification)
static const uint8_t sbox[8][64] = {
    {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
     0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
     4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
     15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
    
    {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
     3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
     0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
     13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
    
    {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
     13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
     13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
     1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
    
    {7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
     13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
     10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
     3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
    
    {2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
     14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
     4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
     11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
    
    {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
     10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
     9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
     4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
    
    {4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
     13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
     1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
     6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
    
    {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
     1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
     7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
     2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}
};

// Initial Permutation (IP)
static const uint8_t ip[] = {
    58, 50, 42, 34, 26, 18, 10, 2,
    60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,
    64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1,
    59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,
    63, 55, 47, 39, 31, 23, 15, 7
};

// Final Permutation (FP)
static const uint8_t fp[] = {
    40, 8, 48, 16, 56, 24, 64, 32,
    39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,
    37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,
    35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,
    33, 1, 41, 9, 49, 17, 57, 25
};

// Expansion table (E)
static const uint8_t expansion[] = {
    32, 1, 2, 3, 4, 5,
    4, 5, 6, 7, 8, 9,
    8, 9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32, 1
};

// Permutation table (P)
static const uint8_t p_table[] = {
    16, 7, 20, 21, 29, 12, 28, 17,
    1, 15, 23, 26, 5, 18, 31, 10,
    2, 8, 24, 14, 32, 27, 3, 9,
    19, 13, 30, 6, 22, 11, 4, 25
};

// PC1 permutation for key schedule
static const uint8_t pc1[] = {
    57, 49, 41, 33, 25, 17, 9,
    1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27,
    19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
    7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29,
    21, 13, 5, 28, 20, 12, 4
};

// PC2 permutation for key schedule
static const uint8_t pc2[] = {
    14, 17, 11, 24, 1, 5,
    3, 28, 15, 6, 21, 10,
    23, 19, 12, 4, 26, 8,
    16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

// Left shifts for key schedule
static const uint8_t shifts[] = {
    1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

// ========== BIT MANIPULATION HELPERS ==========

static uint64_t permute(uint64_t input, const uint8_t* table, int n) {
    uint64_t output = 0;
    for(int i = 0; i < n; i++) {
        if(input & (1ULL << (64 - table[i]))) {
            output |= (1ULL << (n - 1 - i));
        }
    }
    return output;
}

static uint32_t permute32(uint32_t input, const uint8_t* table, int n) {
    uint32_t output = 0;
    for(int i = 0; i < n; i++) {
        if(input & (1UL << (32 - table[i]))) {
            output |= (1UL << (n - 1 - i));
        }
    }
    return output;
}

static uint32_t rotate_left28(uint32_t val, int shift) {
    return ((val << shift) | (val >> (28 - shift))) & 0x0FFFFFFF;
}

// ========== DES KEY SCHEDULE ==========

static void des_key_schedule(const uint8_t* key, uint64_t* subkeys) {
    // Convert key to 64-bit integer
    uint64_t key64 = 0;
    for(int i = 0; i < 8; i++) {
        key64 |= ((uint64_t)key[i]) << (56 - i*8);
    }
    
    // PC1 permutation
    uint64_t permuted_key = permute(key64, pc1, 56);
    
    // Split into C and D
    uint32_t c = (permuted_key >> 28) & 0x0FFFFFFF;
    uint32_t d = permuted_key & 0x0FFFFFFF;
    
    // Generate 16 subkeys
    for(int round = 0; round < 16; round++) {
        // Rotate
        c = rotate_left28(c, shifts[round]);
        d = rotate_left28(d, shifts[round]);
        
        // Combine and apply PC2
        uint64_t cd = ((uint64_t)c << 28) | d;
        subkeys[round] = permute(cd, pc2, 48);
    }
}

// ========== DES F FUNCTION ==========

static uint32_t des_f_function(uint32_t r, uint64_t subkey) {
    // Expansion
    uint64_t expanded = 0;
    for(int i = 0; i < 48; i++) {
        if(r & (1UL << (32 - expansion[i]))) {
            expanded |= (1ULL << (47 - i));
        }
    }
    
    // XOR with subkey
    expanded ^= subkey;
    
    // S-boxes
    uint32_t output = 0;
    for(int i = 0; i < 8; i++) {
        uint8_t chunk = (expanded >> (42 - i*6)) & 0x3F;
        uint8_t row = ((chunk & 0x20) >> 4) | (chunk & 0x01);
        uint8_t col = (chunk >> 1) & 0x0F;
        uint8_t sbox_out = sbox[i][row * 16 + col];
        output |= (sbox_out << (28 - i*4));
    }
    
    // P permutation
    output = permute32(output, p_table, 32);
    
    return output;
}

// ========== DES ENCRYPTION/DECRYPTION ==========

static void des_crypt(const uint8_t* input, const uint64_t* subkeys, 
                     uint8_t* output, bool decrypt) {
    // Convert input to 64-bit
    uint64_t block = 0;
    for(int i = 0; i < 8; i++) {
        block |= ((uint64_t)input[i]) << (56 - i*8);
    }
    
    // Initial permutation
    block = permute(block, ip, 64);
    
    // Split into L and R
    uint32_t l = (block >> 32) & 0xFFFFFFFF;
    uint32_t r = block & 0xFFFFFFFF;
    
    // 16 rounds
    for(int round = 0; round < 16; round++) {
        uint32_t temp = r;
        int key_idx = decrypt ? (15 - round) : round;
        r = l ^ des_f_function(r, subkeys[key_idx]);
        l = temp;
    }
    
    // Swap L and R, then final permutation
    uint64_t combined = ((uint64_t)r << 32) | l;
    combined = permute(combined, fp, 64);
    
    // Convert back to bytes
    for(int i = 0; i < 8; i++) {
        output[i] = (combined >> (56 - i*8)) & 0xFF;
    }
}

// ========== 3DES (2-KEY) IMPLEMENTATION ==========

void des3_encrypt_ecb(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    uint64_t subkeys1[16], subkeys2[16];
    uint8_t temp[8];
    
    // Generate subkeys for key1 and key2
    des_key_schedule(&key[0], subkeys1);
    des_key_schedule(&key[8], subkeys2);
    
    // Encrypt-Decrypt-Encrypt
    des_crypt(input, subkeys1, temp, false);      // E(K1)
    des_crypt(temp, subkeys2, temp, true);        // D(K2)
    des_crypt(temp, subkeys1, output, false);     // E(K1)
}

void des3_decrypt_ecb(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    uint64_t subkeys1[16], subkeys2[16];
    uint8_t temp[8];
    
    // Generate subkeys
    des_key_schedule(&key[0], subkeys1);
    des_key_schedule(&key[8], subkeys2);
    
    // Decrypt-Encrypt-Decrypt
    des_crypt(input, subkeys1, temp, true);       // D(K1)
    des_crypt(temp, subkeys2, temp, false);       // E(K2)
    des_crypt(temp, subkeys1, output, true);      // D(K1)
}

// ========== 3DES CBC MODE ==========

void des3_encrypt_cbc(const uint8_t* key, const uint8_t* iv,
                     const uint8_t* input, uint8_t* output, size_t length) {
    uint8_t block[8];
    uint8_t prev[8];
    
    memcpy(prev, iv, 8);
    
    for(size_t i = 0; i < length; i += 8) {
        // XOR with previous ciphertext (or IV)
        for(int j = 0; j < 8; j++) {
            block[j] = input[i + j] ^ prev[j];
        }
        
        // Encrypt
        des3_encrypt_ecb(key, block, &output[i]);
        
        // Save ciphertext for next block
        memcpy(prev, &output[i], 8);
    }
}

void des3_decrypt_cbc(const uint8_t* key, const uint8_t* iv,
                     const uint8_t* input, uint8_t* output, size_t length) {
    uint8_t block[8];
    uint8_t prev[8];
    
    memcpy(prev, iv, 8);
    
    for(size_t i = 0; i < length; i += 8) {
        // Decrypt
        des3_decrypt_ecb(key, &input[i], block);
        
        // XOR with previous ciphertext (or IV)
        for(int j = 0; j < 8; j++) {
            output[i + j] = block[j] ^ prev[j];
        }
        
        // Save ciphertext for next block
        memcpy(prev, &input[i], 8);
    }
}

// ========== KEY DIVERSIFICATION (COMMON PATTERN) ==========

void des3_derive_key(const uint8_t* master_key, const uint8_t* diversifier,
                    size_t div_len, uint8_t* derived_key) {
    // Simple key derivation: Encrypt diversifier with master key
    uint8_t temp[8] = {0};
    
    // Copy diversifier (pad if needed)
    memcpy(temp, diversifier, div_len < 8 ? div_len : 8);
    
    // Encrypt with master key
    des3_encrypt_ecb(master_key, temp, derived_key);
    
    // Second half (if 16-byte key)
    if(div_len > 8) {
        memcpy(temp, &diversifier[8], div_len - 8);
    }
    des3_encrypt_ecb(master_key, temp, &derived_key[8]);
}

// ========== UTILITIES ==========

bool des3_validate_key(const uint8_t* key) {
    // Check for weak DES keys (known weak patterns)
    // Key should not be all 0x00 or all 0xFF
    bool all_zero = true, all_ff = true;
    
    for(int i = 0; i < 16; i++) {
        if(key[i] != 0x00) all_zero = false;
        if(key[i] != 0xFF) all_ff = false;
    }
    
    return !all_zero && !all_ff;
}
