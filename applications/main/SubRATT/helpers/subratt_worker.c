#include "subratt_worker_private.h"
#include "../subratt_device.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <toolbox/stream/stream.h>
#include <flipper_format.h>
#include <flipper_format_i.h>
#include "../protocols/protocol_items.h"

#include <lib/subghz/blocks/custom_btn.h>

#define TAG                               "SubRattWorker"
#define SUBRATT_TX_TIMEOUT               6
#define SUBRATT_MANUAL_TRANSMIT_INTERVAL 250

#define SUBRATT_KEYLOG_FLUSH_EVERY_N_KEYS 25

SubRattWorker* subratt_worker_alloc(const SubGhzDevice* radio_device) {

    SubRattWorker* instance = calloc(1, sizeof(SubRattWorker));

    instance->state = SubRattWorkerStateIDLE;
    instance->step = 0;
    instance->worker_running = false;
    instance->initiated = false;
    instance->last_time_tx_data = 0;
    instance->load_index = 0;
    instance->opencode = 0;
    instance->random_mode = false;

    instance->thread = furi_thread_alloc();
    furi_thread_set_name(instance->thread, "SubRattAttackWorker");
    furi_thread_set_stack_size(instance->thread, 2048);
    furi_thread_set_context(instance->thread, instance);
    furi_thread_set_callback(instance->thread, subratt_worker_thread);

    instance->context = NULL;
    instance->callback = NULL;

    instance->tx_timeout_ms = SUBRATT_TX_TIMEOUT;
    instance->decoder_result = NULL;
    instance->transmitter = NULL;
    instance->environment = subghz_environment_alloc();

    subghz_environment_set_protocol_registry(
        instance->environment, (void*)&subratt_subghz_protocol_registry);

    instance->transmit_mode = false;

    instance->radio_device = radio_device;

    instance->keylog_storage = NULL;
    instance->keylog_file = NULL;
    instance->keylog_count = 0;

    instance->replay_mode = false;
    instance->replay_format = NULL;
    instance->replay_stream = NULL;
    instance->replay_index = 0;
    instance->replay_total = 0;

    return instance;
}

static void subratt_worker_close_keylog(SubRattWorker* instance) {
    if(instance->keylog_file != NULL) {
        storage_file_close(instance->keylog_file);
        storage_file_free(instance->keylog_file);
        instance->keylog_file = NULL;
    }
    if(instance->keylog_storage != NULL) {
        furi_record_close(RECORD_STORAGE);
        instance->keylog_storage = NULL;
    }
}

static void subratt_worker_close_replay(SubRattWorker* instance) {
    if(instance->replay_format != NULL) {
        flipper_format_file_close(instance->replay_format);
        flipper_format_free(instance->replay_format);
        instance->replay_format = NULL;
        instance->replay_stream = NULL;
    }
}

static void subratt_worker_open_keylog_session(SubRattWorker* instance) {
    subratt_worker_close_keylog(instance);

    instance->keylog_count = 0;
    instance->keylog_storage = furi_record_open(RECORD_STORAGE);
    instance->keylog_file = storage_file_alloc(instance->keylog_storage);
    bool ok = storage_file_open(
        instance->keylog_file, SUBRATT_KEYLOG_SESSION_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        bool is_file_attack = instance->attack == SubRattAttackLoadFile;
        char header[256];
        int len = snprintf(
            header,
            sizeof(header),
            "Filetype: Flipper SubRatt Keys Log\nVersion: 1\nFrequency: %lu\nPreset: %s\n"
            "Protocol: %s\nBit: %d\nTE: %lu\nRepeat: %d\nOpencode: %d\nIsFileAttack: %d\n"
            "LoadIndex: %d\nFileKey: %016llX\nTwoBytes: %d\n",
            (unsigned long)instance->frequency,
            subratt_protocol_preset(instance->preset),
            subratt_protocol_file(instance->file),
            instance->bits,
            (unsigned long)instance->te,
            instance->repeat,
            instance->opencode,
            is_file_attack ? 1 : 0,
            instance->load_index,
            (unsigned long long)instance->file_key,
            instance->two_bytes ? 1 : 0);
        ok = len > 0 && storage_file_write(instance->keylog_file, header, len) == (size_t)len;
    }
    if(!ok) {
        FURI_LOG_E(TAG, "Could not open keylog session file %s", SUBRATT_KEYLOG_SESSION_PATH);
        subratt_worker_close_keylog(instance);
    }
}

void subratt_worker_free(SubRattWorker* instance) {
    furi_assert(instance);

    instance->decoder_result = NULL;

    if(instance->transmitter != NULL) {
        subghz_transmitter_free(instance->transmitter);
        instance->transmitter = NULL;
    }

    subratt_worker_close_keylog(instance);
    subratt_worker_close_replay(instance);

    subghz_environment_free(instance->environment);
    instance->environment = NULL;

    furi_thread_free(instance->thread);

    subghz_devices_sleep(instance->radio_device);
    subratt_radio_device_loader_end(instance->radio_device);

    free(instance);
}

uint32_t subratt_worker_get_keylog_count(SubRattWorker* instance) {
    return instance->keylog_count;
}

uint32_t subratt_worker_get_replay_index(SubRattWorker* instance) {
    return instance->replay_index;
}

uint32_t subratt_worker_get_replay_total(SubRattWorker* instance) {
    return instance->replay_total;
}

uint64_t subratt_worker_get_step(SubRattWorker* instance) {
    return instance->step;
}

bool subratt_worker_set_step(SubRattWorker* instance, uint64_t step) {
    furi_assert(instance);
    if(!subratt_worker_can_manual_transmit(instance)) {
        FURI_LOG_W(TAG, "Cannot set step during running mode");

        return false;
    }

    instance->step = step;

    return true;
}

void subratt_worker_set_opencode(SubRattWorker* instance, uint8_t opencode) {

    instance->opencode = opencode;

}

uint8_t subratt_worker_get_opencode(SubRattWorker* instance) {
    return instance->opencode;
}

bool subratt_worker_get_is_pt2262(SubRattWorker* instance) {
    if(instance->attack == SubRattAttackPT226224bit315 ||
       instance->attack == SubRattAttackPT226224bit418 ||
       instance->attack == SubRattAttackPT226224bit430 ||
       instance->attack == SubRattAttackPT226224bit4305 ||
       instance->attack == SubRattAttackPT226224bit433) {
        return true;
    } else {
        return false;
    }
}

void subratt_worker_set_random_mode(SubRattWorker* instance, bool random_mode) {
    furi_assert(instance);
    instance->random_mode = random_mode;
}

bool subratt_worker_get_random_mode(SubRattWorker* instance) {
    furi_assert(instance);
    return instance->random_mode;
}

bool subratt_worker_init_default_attack(
    SubRattWorker* instance,
    SubRattAttacks attack_type,
    uint64_t step,
    const SubRattProtocol* protocol,
    uint8_t repeats) {
    furi_assert(instance);

    if(instance->worker_running) {
        FURI_LOG_W(TAG, "Init Worker when it's running");
        subratt_worker_stop(instance);
    }

    instance->attack = attack_type;
    instance->frequency = protocol->frequency;
    instance->preset = protocol->preset;
    instance->file = protocol->file;
    instance->step = step;
    instance->bits = protocol->bits;
    instance->te = protocol->te;
    instance->opencode = protocol->opencode;
    instance->repeat = repeats;
    instance->load_index = 0;
    instance->file_key = 0;
    instance->two_bytes = false;

    instance->max_value =
        subratt_protocol_calc_max_value(instance->attack, instance->bits, instance->two_bytes);

    instance->replay_mode = false;

    instance->random_mode = false;
    subratt_worker_close_replay(instance);
    subratt_worker_open_keylog_session(instance);

    instance->initiated = true;
    instance->state = SubRattWorkerStateReady;
    subratt_worker_send_callback(instance);
#ifdef FURI_DEBUG
    FURI_LOG_I(
        TAG,
        "subratt_worker_init_default_attack: %s, bits: %d, preset: %s, file: %s, te: %ld, repeat: %d, max_value: %lld",
        subratt_protocol_name(instance->attack),
        instance->bits,
        subratt_protocol_preset(instance->preset),
        subratt_protocol_file(instance->file),
        instance->te,
        instance->repeat,
        instance->max_value);
#endif

    return true;
}

bool subratt_worker_init_file_attack(
    SubRattWorker* instance,
    uint64_t step,
    uint8_t load_index,
    uint64_t file_key,
    SubRattProtocol* protocol,
    uint8_t repeats,
    bool two_bytes) {
    furi_assert(instance);

    if(instance->worker_running) {
        FURI_LOG_W(TAG, "Init Worker when it's running");
        subratt_worker_stop(instance);
    }

    instance->attack = SubRattAttackLoadFile;
    instance->frequency = protocol->frequency;
    instance->preset = protocol->preset;
    instance->file = protocol->file;
    instance->step = step;
    instance->bits = protocol->bits;
    instance->te = protocol->te;
    instance->load_index = load_index;
    instance->repeat = repeats;
    instance->file_key = file_key;
    instance->two_bytes = two_bytes;

    instance->max_value =
        subratt_protocol_calc_max_value(instance->attack, instance->bits, instance->two_bytes);

    instance->replay_mode = false;
    subratt_worker_close_replay(instance);
    subratt_worker_open_keylog_session(instance);

    instance->initiated = true;
    instance->state = SubRattWorkerStateReady;
    subratt_worker_send_callback(instance);
#ifdef FURI_DEBUG
    FURI_LOG_I(
        TAG,
        "subratt_worker_init_file_attack: %s, bits: %d, preset: %s, file: %s, te: %ld, repeat: %d, max_value: %lld, key: %llX",
        subratt_protocol_name(instance->attack),
        instance->bits,
        subratt_protocol_preset(instance->preset),
        subratt_protocol_file(instance->file),
        instance->te,
        instance->repeat,
        instance->max_value,
        instance->file_key);
#endif

    return true;
}

bool subratt_worker_init_replay_attack(
    SubRattWorker* instance,
    uint32_t frequency,
    FuriHalSubGhzPreset preset,
    SubRattFileProtocol file,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t opencode,
    bool is_file_attack,
    uint8_t load_index,
    uint64_t file_key,
    bool two_bytes,
    uint32_t total_keys,
    const char* file_path) {
    furi_assert(instance);

    if(instance->worker_running) {
        FURI_LOG_W(TAG, "Init Worker when it's running");
        subratt_worker_stop(instance);
    }

    subratt_worker_close_keylog(instance);
    subratt_worker_close_replay(instance);

    instance->attack = is_file_attack ? SubRattAttackLoadFile : SubRattAttackCAME12bit433;
    instance->frequency = frequency;
    instance->preset = preset;
    instance->file = file;
    instance->bits = bits;
    instance->te = te;
    instance->repeat = repeat;
    instance->opencode = opencode;
    instance->load_index = load_index;
    instance->file_key = file_key;
    instance->two_bytes = two_bytes;
    instance->step = 0;
    instance->max_value = UINT64_MAX;

    instance->replay_mode = true;
    instance->replay_index = 0;
    instance->replay_total = total_keys;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    instance->replay_format = flipper_format_file_alloc(storage);
    furi_record_close(RECORD_STORAGE);

    bool ok = flipper_format_file_open_existing(instance->replay_format, file_path);
    if(ok) {

        FuriString* temp_str = furi_string_alloc();
        uint32_t temp_u32;
        ok = flipper_format_read_header(instance->replay_format, temp_str, &temp_u32) &&
             flipper_format_read_uint32(instance->replay_format, "Frequency", &temp_u32, 1) &&
             flipper_format_read_string(instance->replay_format, "Preset", temp_str) &&
             flipper_format_read_string(instance->replay_format, "Protocol", temp_str) &&
             flipper_format_read_uint32(instance->replay_format, "Bit", &temp_u32, 1);

        flipper_format_read_uint32(instance->replay_format, "TE", &temp_u32, 1);
        flipper_format_read_uint32(instance->replay_format, "Repeat", &temp_u32, 1);
        flipper_format_read_uint32(instance->replay_format, "Opencode", &temp_u32, 1);
        flipper_format_read_uint32(instance->replay_format, "IsFileAttack", &temp_u32, 1);
        flipper_format_read_uint32(instance->replay_format, "LoadIndex", &temp_u32, 1);
        flipper_format_read_string(instance->replay_format, "FileKey", temp_str);
        flipper_format_read_uint32(instance->replay_format, "TwoBytes", &temp_u32, 1);
        furi_string_free(temp_str);
        instance->replay_stream = flipper_format_get_raw_stream(instance->replay_format);
    }

    if(!ok) {
        FURI_LOG_E(TAG, "Could not re-open replay file %s", file_path);
        subratt_worker_close_replay(instance);
    }

    instance->initiated = ok;
    instance->state = SubRattWorkerStateReady;
    subratt_worker_send_callback(instance);

    return ok;
}

bool subratt_worker_start(SubRattWorker* instance) {
    furi_assert(instance);

    if(!instance->initiated) {
        FURI_LOG_W(TAG, "Worker not init!");
        return false;
    }

    if(instance->worker_running) {
        FURI_LOG_W(TAG, "Worker is already running!");
        return false;
    }
    if(instance->state != SubRattWorkerStateReady &&
       instance->state != SubRattWorkerStateFinished) {
        FURI_LOG_W(TAG, "Worker cannot start, invalid device state: %d", instance->state);
        return false;
    }

    instance->worker_running = true;
    furi_thread_start(instance->thread);

    return true;
}

void subratt_worker_stop(SubRattWorker* instance) {
    furi_assert(instance);

    if(!instance->worker_running) {
        return;
    }

    instance->worker_running = false;
    furi_thread_join(instance->thread);

    subghz_devices_idle(instance->radio_device);

    if(!instance->replay_mode) {

        subratt_worker_close_keylog(instance);
    }
}

bool subratt_worker_transmit_current_key(SubRattWorker* instance, uint64_t step) {
    furi_assert(instance);

    if(!instance->initiated) {
        FURI_LOG_W(TAG, "Worker not init!");
        return false;
    }
    if(instance->worker_running) {
        FURI_LOG_W(TAG, "Worker in running state!");
        return false;
    }
    if(instance->state != SubRattWorkerStateReady &&
       instance->state != SubRattWorkerStateFinished) {
        FURI_LOG_W(TAG, "Invalid state for running worker! State: %d", instance->state);
        return false;
    }

    uint32_t ticks = furi_get_tick();
    if((ticks - instance->last_time_tx_data) < SUBRATT_MANUAL_TRANSMIT_INTERVAL) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Need to wait, current: %ld", ticks - instance->last_time_tx_data);
#endif
        return false;
    }

    instance->last_time_tx_data = ticks;
    instance->step = step;

    bool result;
    instance->protocol_name = subratt_protocol_file(instance->file);
    FlipperFormat* flipper_format = flipper_format_string_alloc();
    Stream* stream = flipper_format_get_raw_stream(flipper_format);

    stream_clean(stream);

    if(instance->attack == SubRattAttackLoadFile) {
        subratt_protocol_file_payload(
            stream,
            step,
            instance->bits,
            instance->te,
            instance->repeat,
            instance->load_index,
            instance->file_key,
            instance->two_bytes);
    } else {
        subratt_protocol_default_payload(
            stream,
            instance->file,
            step,
            instance->bits,
            instance->te,
            instance->repeat,
            instance->opencode);
    }

    result = subratt_worker_subghz_transmit(instance, flipper_format);
    if(!result) {

        instance->state = SubRattWorkerStateIDLE;
        subratt_worker_send_callback(instance);
    }
#ifdef FURI_DEBUG
    FURI_LOG_W(TAG, "Manual transmit done");
#endif
    flipper_format_free(flipper_format);

    return result;
}

bool subratt_worker_is_running(SubRattWorker* instance) {
    return instance->worker_running;
}

bool subratt_worker_can_manual_transmit(SubRattWorker* instance) {
    furi_assert(instance);

    if(!instance->initiated) {
        FURI_LOG_W(TAG, "Worker not init!");
        return false;
    }

    return !instance->worker_running && instance->state != SubRattWorkerStateIDLE &&
           instance->state != SubRattWorkerStateTx &&
           ((furi_get_tick() - instance->last_time_tx_data) > SUBRATT_MANUAL_TRANSMIT_INTERVAL);
}

void subratt_worker_set_callback(
    SubRattWorker* instance,
    SubRattWorkerCallback callback,
    void* context) {
    furi_assert(instance);

    instance->callback = callback;
    instance->context = context;
}

bool subratt_worker_subghz_transmit(SubRattWorker* instance, FlipperFormat* flipper_format) {
    const uint8_t timeout = instance->tx_timeout_ms;
    while(instance->transmit_mode) {
        furi_delay_ms(timeout);
    }
    instance->transmit_mode = true;
    if(instance->transmitter != NULL) {
        subghz_transmitter_free(instance->transmitter);
        instance->transmitter = NULL;
    }

    instance->transmitter =
        subghz_transmitter_alloc_init(instance->environment, instance->protocol_name);

    if(instance->transmitter == NULL) {
        FURI_LOG_E(
            TAG,
            "Protocol \"%s\" is not in this firmware's active SubGhz registry - cannot transmit",
            instance->protocol_name);
        instance->transmit_mode = false;
        return false;
    }

    subghz_transmitter_deserialize(instance->transmitter, flipper_format);

    subghz_devices_reset(instance->radio_device);
    subghz_devices_idle(instance->radio_device);
    subghz_devices_load_preset(instance->radio_device, instance->preset, NULL);
    subghz_devices_set_frequency(
        instance->radio_device, instance->frequency);

    if(subghz_devices_set_tx(instance->radio_device)) {
        subghz_devices_start_async_tx(
            instance->radio_device, subghz_transmitter_yield, instance->transmitter);
        while(!subghz_devices_is_async_complete_tx(instance->radio_device)) {
            furi_delay_ms(timeout);
        }
        subghz_devices_stop_async_tx(instance->radio_device);
    }

    subghz_devices_idle(instance->radio_device);

    subghz_transmitter_stop(instance->transmitter);
    subghz_transmitter_free(instance->transmitter);
    instance->transmitter = NULL;

    instance->transmit_mode = false;

    Stream* stream = flipper_format_get_raw_stream(flipper_format);
    stream_rewind(stream);

    subghz_custom_btns_reset();

    return true;
}

void subratt_worker_send_callback(SubRattWorker* instance) {
    if(instance->callback != NULL) {
        instance->callback(instance->context, instance->state);
    }
}

int32_t subratt_worker_thread(void* context) {
    furi_assert(context);
    SubRattWorker* instance = (SubRattWorker*)context;

    if(!instance->worker_running) {
        FURI_LOG_W(TAG, "Worker is not set to running state!");

        return -1;
    }
    if(instance->state != SubRattWorkerStateReady &&
       instance->state != SubRattWorkerStateFinished) {
        FURI_LOG_W(TAG, "Invalid state for running worker! State: %d", instance->state);

        return -2;
    }
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "Worker start");
#endif

    SubRattWorkerState local_state = instance->state = SubRattWorkerStateTx;
    subratt_worker_send_callback(instance);

    instance->protocol_name = subratt_protocol_file(instance->file);

    FlipperFormat* flipper_format = flipper_format_string_alloc();
    Stream* stream = flipper_format_get_raw_stream(flipper_format);
    FuriString* replay_line = instance->replay_mode ? furi_string_alloc() : NULL;

    while(instance->worker_running) {
        if(instance->replay_mode) {

            bool have_key = false;
            while(stream_read_line(instance->replay_stream, replay_line)) {
                if(furi_string_start_with_str(replay_line, "Key:")) {
                    instance->step = strtoull(
                        furi_string_get_cstr(replay_line) + strlen("Key:"), NULL, 16);
                    have_key = true;
                    break;
                }
            }
            if(!have_key) {
#ifdef FURI_DEBUG
                FURI_LOG_I(TAG, "Replay finished - end of file");
#endif
                local_state = SubRattWorkerStateFinished;
                break;
            }
        }

        stream_clean(stream);
        if(instance->attack == SubRattAttackLoadFile) {
            subratt_protocol_file_payload(
                stream,
                instance->step,
                instance->bits,
                instance->te,
                instance->repeat,
                instance->load_index,
                instance->file_key,
                instance->two_bytes);
        } else {
            subratt_protocol_default_payload(
                stream,
                instance->file,
                instance->step,
                instance->bits,
                instance->te,
                instance->repeat,
                instance->opencode);
        }
#ifdef FURI_DEBUG

#endif

        if(!subratt_worker_subghz_transmit(instance, flipper_format)) {

            local_state = SubRattWorkerStateIDLE;
            break;
        }

        if(instance->replay_mode) {
            instance->replay_index++;
        } else {

            if(instance->keylog_file != NULL) {
                char log_line[24];
                int len = snprintf(log_line, sizeof(log_line), "Key: %016llX\n", instance->step);
                if(len > 0) {
                    storage_file_write(instance->keylog_file, log_line, len);
                    instance->keylog_count++;
                    if(instance->keylog_count % SUBRATT_KEYLOG_FLUSH_EVERY_N_KEYS == 0) {
                        storage_file_close(instance->keylog_file);
                        storage_file_open(
                            instance->keylog_file,
                            SUBRATT_KEYLOG_SESSION_PATH,
                            FSAM_WRITE,
                            FSOM_OPEN_APPEND);
                    }
                }
            }

            if(instance->random_mode) {

                instance->step = subratt_protocol_random_step(instance->file, instance->bits);
            } else {
                if(instance->step + 1 > instance->max_value) {
#ifdef FURI_DEBUG
                    FURI_LOG_I(TAG, "Worker finished to end");
#endif
                    local_state = SubRattWorkerStateFinished;

                    break;
                }
                instance->step++;
            }
        }

        furi_delay_ms(instance->tx_timeout_ms);
    }

    if(replay_line != NULL) {
        furi_string_free(replay_line);
    }
    flipper_format_free(flipper_format);

    instance->worker_running = false;
    instance->state = local_state == SubRattWorkerStateTx ? SubRattWorkerStateReady :
                                                             local_state;
    subratt_worker_send_callback(instance);

#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "Worker stop");
#endif

    return 0;
}

uint8_t subratt_worker_get_timeout(SubRattWorker* instance) {
    return instance->tx_timeout_ms;
}

void subratt_worker_set_timeout(SubRattWorker* instance, uint8_t timeout) {
    instance->tx_timeout_ms = timeout;
}

uint8_t subratt_worker_get_repeats(SubRattWorker* instance) {
    return instance->repeat;
}

void subratt_worker_set_repeats(SubRattWorker* instance, uint8_t repeats) {
    instance->repeat = repeats;
}

uint32_t subratt_worker_get_te(SubRattWorker* instance) {
    return instance->te;
}

void subratt_worker_set_te(SubRattWorker* instance, uint32_t te) {
    instance->te = te;
}

bool subratt_worker_is_tx_allowed(SubRattWorker* instance, uint32_t value) {
    furi_assert(instance);
    bool res = false;

    if(!subghz_devices_is_frequency_valid(instance->radio_device, value)) {
        return false;
    } else {
        subghz_devices_set_frequency(instance->radio_device, value);
        res = subghz_devices_set_tx(instance->radio_device);
        subghz_devices_idle(instance->radio_device);
    }

    return res;
}
