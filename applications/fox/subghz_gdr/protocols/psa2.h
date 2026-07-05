#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * PROTOCOL NAME
 * ========================================================= */

#define SUBGHZ_PROTOCOL_PSA2_NAME "PSA OLD"

/* =========================================================
 * FORWARD DECLARATIONS — opaque handles
 * ========================================================= */

typedef struct SubGhzProtocolDecoderPSA SubGhzProtocolDecoderPSA;
typedef struct SubGhzProtocolEncoderPSA SubGhzProtocolEncoderPSA;

/* =========================================================
 * PROTOCOL DESCRIPTORS — exported singletons
 * ========================================================= */

extern const SubGhzProtocolDecoder subghz_protocol_psa_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_psa_encoder;
extern const SubGhzProtocol        subghz_protocol_psa2;

/* =========================================================
 * DECODER API
 * ========================================================= */

/**
 * Allocate a PSA decoder instance.
 *
 * @param environment   SubGHz environment (may be NULL / unused)
 * @return              Opaque pointer to SubGhzProtocolDecoderPSA
 */
void* subghz_protocol_decoder_psa2_alloc(SubGhzEnvironment* environment);

/**
 * Free a PSA decoder instance.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 */
void subghz_protocol_decoder_psa2_free(void* context);

/**
 * Reset the decoder state machine to its initial state.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 */
void subghz_protocol_decoder_psa2_reset(void* context);

/**
 * Feed one pulse/gap sample into the decoder state machine.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 * @param level     true = RF high (pulse), false = RF low (gap)
 * @param duration  Duration of this level in microseconds
 */
void subghz_protocol_decoder_psa2_feed(void* context, bool level, uint32_t duration);

/**
 * Return a one-byte hash of the most recently decoded packet.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 * @return          Hash byte
 */
uint8_t subghz_protocol_decoder_psa2_get_hash_data(void* context);

/**
 * Serialize the most recently decoded packet into a FlipperFormat stream.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 * @param ff        Open FlipperFormat file handle
 * @param preset    Radio preset in use
 * @return          SubGhzProtocolStatusOk on success
 */
SubGhzProtocolStatus subghz_protocol_decoder_psa2_serialize(
    void*             context,
    FlipperFormat*    ff,
    SubGhzRadioPreset* preset);

/**
 * Deserialize a previously saved packet from a FlipperFormat stream
 * into the decoder instance.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 * @param ff        Open FlipperFormat file handle (positioned at start)
 * @return          SubGhzProtocolStatusOk on success
 */
SubGhzProtocolStatus subghz_protocol_decoder_psa2_deserialize(
    void*          context,
    FlipperFormat* ff);

/**
 * Build a human-readable description of the most recently decoded packet.
 *
 * @param context   Pointer returned by subghz_protocol_decoder_psa_alloc()
 * @param output    FuriString to write the description into (cleared first)
 */
void subghz_protocol_decoder_psa2_get_string(void* context, FuriString* output);

/* =========================================================
 * ENCODER API
 * ========================================================= */

/**
 * Allocate a PSA encoder instance.
 *
 * @param environment   SubGHz environment (may be NULL / unused)
 * @return              Opaque pointer to SubGhzProtocolEncoderPSA
 */
void* subghz_protocol_encoder_psa2_alloc(SubGhzEnvironment* environment);

/**
 * Free a PSA encoder instance.
 *
 * @param context   Pointer returned by subghz_protocol_encoder_psa_alloc()
 */
void subghz_protocol_encoder_psa2_free(void* context);

/**
 * Load transmit data from a FlipperFormat stream into the encoder.
 * Rebuilds the upload buffer ready for transmission.
 *
 * @param context   Pointer returned by subghz_protocol_encoder_psa_alloc()
 * @param ff        Open FlipperFormat file handle (will be rewound internally)
 * @return          SubGhzProtocolStatusOk on success
 */
SubGhzProtocolStatus subghz_protocol_encoder_psa2_deserialize(
    void*          context,
    FlipperFormat* ff);

/**
 * Stop an in-progress transmission immediately.
 *
 * @param context   Pointer returned by subghz_protocol_encoder_psa_alloc()
 */
void subghz_protocol_encoder_psa2_stop(void* context);

/**
 * Yield the next LevelDuration sample from the upload buffer.
 * Called repeatedly by the SubGHz radio driver during transmission.
 *
 * @param context   Pointer returned by subghz_protocol_encoder_psa_alloc()
 * @return          Next LevelDuration, or level_duration_reset() when done
 */
LevelDuration subghz_protocol_encoder_psa2_yield(void* context);

#ifdef __cplusplus
}
#endif
