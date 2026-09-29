#include "math.h"

uint64_t subghz_protocol_blocks_reverse_key(uint64_t key, uint8_t bit_count) {
    uint64_t reverse_key = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        reverse_key = reverse_key << 1 | bit_read(key, i);
    }
    return reverse_key;
}

uint8_t subghz_protocol_blocks_get_parity(uint64_t key, uint8_t bit_count) {
    uint8_t parity = 0;
    for(uint8_t i = 0; i < bit_count; i++) {
        parity += bit_read(key, i);
    }
    return parity & 0x01;
}

uint8_t subghz_protocol_blocks_crc4(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init) {
    uint8_t remainder = init << 4;
    uint8_t poly = polynomial << 4;
    uint8_t bit;

    while(size--) {
        remainder ^= *message++;
        for(bit = 0; bit < 8; bit++) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ poly;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder >> 4 & 0x0f;
}

uint8_t subghz_protocol_blocks_crc7(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init) {
    uint8_t remainder = init << 1;
    uint8_t poly = polynomial << 1;

    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ poly;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder >> 1 & 0x7f;
}

uint8_t subghz_protocol_blocks_crc8(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init) {
    uint8_t remainder = init;

    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x80) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

uint8_t subghz_protocol_blocks_crc8le(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init) {
    uint8_t remainder = subghz_protocol_blocks_reverse_key(init, 8);
    polynomial = subghz_protocol_blocks_reverse_key(polynomial, 8);

    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 1) {
                remainder = (remainder >> 1) ^ polynomial;
            } else {
                remainder = (remainder >> 1);
            }
        }
    }
    return remainder;
}

uint16_t subghz_protocol_blocks_crc16lsb(
    uint8_t const message[],
    size_t size,
    uint16_t polynomial,
    uint16_t init) {
    uint16_t remainder = init;

    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 1) {
                remainder = (remainder >> 1) ^ polynomial;
            } else {
                remainder = (remainder >> 1);
            }
        }
    }
    return remainder;
}

uint16_t subghz_protocol_blocks_crc16(
    uint8_t const message[],
    size_t size,
    uint16_t polynomial,
    uint16_t init) {
    uint16_t remainder = init;

    for(size_t byte = 0; byte < size; ++byte) {
        remainder ^= message[byte] << 8;
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(remainder & 0x8000) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

uint8_t subghz_protocol_blocks_lfsr_digest8(
    uint8_t const message[],
    size_t size,
    uint8_t gen,
    uint8_t key) {
    uint8_t sum = 0;
    for(size_t byte = 0; byte < size; ++byte) {
        uint8_t data = message[byte];
        for(int i = 7; i >= 0; --i) {

            if((data >> i) & 1) sum ^= key;

            if(key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

uint8_t subghz_protocol_blocks_lfsr_digest8_reflect(
    uint8_t const message[],
    size_t size,
    uint8_t gen,
    uint8_t key) {
    uint8_t sum = 0;

    for(int byte = size - 1; byte >= 0; --byte) {
        uint8_t data = message[byte];

        for(uint8_t i = 0; i < 8; ++i) {

            if((data >> i) & 1) {
                sum ^= key;
            }

            if(key & 0x80)
                key = (key << 1) ^ gen;
            else
                key = (key << 1);
        }
    }
    return sum;
}

uint16_t subghz_protocol_blocks_lfsr_digest16(
    uint8_t const message[],
    size_t size,
    uint16_t gen,
    uint16_t key) {
    uint16_t sum = 0;
    for(size_t byte = 0; byte < size; ++byte) {
        uint8_t data = message[byte];
        for(int8_t i = 7; i >= 0; --i) {

            if((data >> i) & 1) sum ^= key;

            if(key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

uint8_t subghz_protocol_blocks_add_bytes(uint8_t const message[], size_t size) {
    uint32_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result += message[i];
    }
    return (uint8_t)result;
}

uint8_t subghz_protocol_blocks_parity8(uint8_t byte) {
    byte ^= byte >> 4;
    byte &= 0xf;
    return (0x6996 >> byte) & 1;
}

uint8_t subghz_protocol_blocks_parity_bytes(uint8_t const message[], size_t size) {
    uint8_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result ^= subghz_protocol_blocks_parity8(message[i]);
    }
    return result;
}

uint8_t subghz_protocol_blocks_xor_bytes(uint8_t const message[], size_t size) {
    uint8_t result = 0;
    for(size_t i = 0; i < size; ++i) {
        result ^= message[i];
    }
    return result;
}
