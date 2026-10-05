#include "../mifare_fuzzer_i.h"
#include <notification/notification.h>
#include <notification/notification_messages.h>

uint8_t tick_counter = 0;
uint8_t attack_step = 0;

uint8_t id_uid_test[9][7] = {
    {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
    {0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28},
    {0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39},
    {0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a},
    {0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b},
    {0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c},
    {0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d},
    {0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e},
    {0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f},
};

static void mifare_fuzzer_scene_emulator_callback(MifareFuzzerEvent event, void* context) {

    furi_assert(context);
    MifareFuzzerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void mifare_fuzzer_scene_emulator_on_enter(void* context) {

    MifareFuzzerApp* app = context;
    MifareFuzzerEmulator* emulator = app->emulator_view;

    mifare_fuzzer_emulator_set_callback(emulator, mifare_fuzzer_scene_emulator_callback, app);

    tick_counter = 0;
    mifare_fuzzer_emulator_set_tick_num(app->emulator_view, tick_counter);
    emulator->ticks_between_cards = MIFARE_FUZZER_DEFAULT_TICKS_BETWEEN_CARDS;
    mifare_fuzzer_emulator_set_ticks_between_cards(
        app->emulator_view, emulator->ticks_between_cards);

    Iso14443_3aData nfc_data;
    nfc_data.atqa[0] = 0x00;
    nfc_data.atqa[1] = 0x00;
    nfc_data.sak = 0x00;
    if(app->card == MifareCardUltralight) {
        nfc_data.uid_len = 0x07;
    } else {
        nfc_data.uid_len = 0x04;
    }
    for(uint32_t i = 0; i < nfc_data.uid_len; i++) {
        nfc_data.uid[i] = 0x00;
    }

    mifare_fuzzer_emulator_set_nfc_data(app->emulator_view, nfc_data);

    attack_step = 0;

    view_dispatcher_switch_to_view(app->view_dispatcher, MifareFuzzerViewEmulator);
}

bool mifare_fuzzer_scene_emulator_on_event(void* context, SceneManagerEvent event) {

    MifareFuzzerApp* app = context;
    MifareFuzzerEmulator* emulator = app->emulator_view;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MifareFuzzerEventStartAttack) {
            NfcDevice* nfc_device = NULL;
            bool nfc_device_parsed = false;
            if(app->card_file_path) {
                nfc_device = app->worker->nfc_device;
                const char* path = furi_string_get_cstr(app->card_file_path);
                if(nfc_device_load(nfc_device, path)) {
                    nfc_device_parsed = true;
                    NfcProtocol protocol = nfc_device_get_protocol(nfc_device);
                    if(protocol == NfcProtocolMfClassic) {
                        const MfClassicData* mfc_data = nfc_device_get_data(nfc_device, protocol);
                        if(mfc_data->type == MfClassicType1k) {
                            app->card = MifareCardClassic1k;
                        } else if(mfc_data->type == MfClassicType4k) {
                            app->card = MifareCardClassic4k;
                        } else {
                            nfc_device_parsed = false;
                        }
                    } else if(protocol == NfcProtocolMfUltralight) {
                        app->card = MifareCardUltralight;
                    }
                    if(nfc_device_parsed) {
                        mifare_fuzzer_emulator_set_card(emulator, app->card, app->card_file_path);
                    }
                }
            }

            Iso14443_3aData* nfc_data = iso14443_3a_alloc();
            if(nfc_device_parsed) {
                iso14443_3a_copy(
                    nfc_data, nfc_device_get_data(nfc_device, NfcProtocolIso14443_3a));
            }

            mifare_fuzzer_worker_stop(app->worker);

            if(app->card == MifareCardClassic1k) {
                nfc_data->atqa[0] = 0x04;
                nfc_data->atqa[1] = 0x00;
                nfc_data->sak = 0x08;
                nfc_data->uid_len = 0x04;
            } else if(app->card == MifareCardClassic4k) {
                nfc_data->atqa[0] = 0x02;
                nfc_data->atqa[1] = 0x00;
                nfc_data->sak = 0x18;
                nfc_data->uid_len = 0x04;
            } else if(app->card == MifareCardUltralight) {
                nfc_data->atqa[0] = 0x44;
                nfc_data->atqa[1] = 0x00;
                nfc_data->sak = 0x00;
                nfc_data->uid_len = 0x07;
            }

            if(app->attack == MifareFuzzerAttackTestValues) {

                for(uint8_t i = 0; i < nfc_data->uid_len; i++) {
                    nfc_data->uid[i] = id_uid_test[attack_step][i];
                }

                if(attack_step >= 8) {
                    attack_step = 0;
                } else {
                    attack_step++;
                }
            } else if(app->attack == MifareFuzzerAttackRandomValues) {
                if(app->card == MifareCardUltralight) {

                    nfc_data->uid[0] = 0x04;
                    for(uint8_t i = 1; i < nfc_data->uid_len; i++) {
                        nfc_data->uid[i] = (furi_hal_random_get() & 0xFF);
                    }
                } else {
                    for(uint8_t i = 0; i < nfc_data->uid_len; i++) {
                        nfc_data->uid[i] = (furi_hal_random_get() & 0xFF);
                    }
                }
            } else if(app->attack == MifareFuzzerAttackLoadUidsFromFile) {

                while(true) {
                    furi_string_reset(app->uid_str);
                    if(!stream_read_line(app->uids_stream, app->uid_str)) {

                        stream_rewind(app->uids_stream);
                        continue;

                    }

                    if(furi_string_get_char(app->uid_str, 0) == '#') continue;

                    if((furi_string_size(app->uid_str) != 9) &&
                       (furi_string_size(app->uid_str) != 15))
                        continue;
                    break;
                }

                for(uint8_t i = 0; i < nfc_data->uid_len; i++) {
                    if(i <= ((furi_string_size(app->uid_str) - 1) / 2)) {
                        char temp_str[3];
                        temp_str[0] = furi_string_get_cstr(app->uid_str)[i * 2];
                        temp_str[1] = furi_string_get_cstr(app->uid_str)[i * 2 + 1];
                        temp_str[2] = '\0';
                        nfc_data->uid[i] = (uint8_t)strtol(temp_str, NULL, 16);
                    } else {
                        nfc_data->uid[i] = 0x00;
                    }
                }
            }

            mifare_fuzzer_emulator_set_nfc_data(app->emulator_view, *nfc_data);
            if(nfc_device_parsed) {
                mifare_fuzzer_worker_set_nfc_device(app->worker, nfc_device);
            } else {
                mifare_fuzzer_worker_set_nfc_data(app->worker, *nfc_data);
            }

            tick_counter = 0;
            mifare_fuzzer_emulator_set_tick_num(app->emulator_view, tick_counter);

            mifare_fuzzer_worker_start(app->worker);

            if(nfc_device_parsed) {
                notification_message(app->notifications, &sequence_blink_start_magenta);
            }
        } else if(event.event == MifareFuzzerEventStopAttack) {

            mifare_fuzzer_worker_stop(app->worker);
            notification_message(app->notifications, &sequence_blink_stop);
        } else if(event.event == MifareFuzzerEventIncrementTicks) {
            if(!emulator->is_attacking) {
                if(emulator->ticks_between_cards < MIFARE_FUZZER_MAX_TICKS_BETWEEN_CARDS) {
                    emulator->ticks_between_cards++;
                    mifare_fuzzer_emulator_set_ticks_between_cards(
                        app->emulator_view, emulator->ticks_between_cards);
                };
            };
        } else if(event.event == MifareFuzzerEventDecrementTicks) {
            if(!emulator->is_attacking) {
                if(emulator->ticks_between_cards > MIFARE_FUZZER_MIN_TICKS_BETWEEN_CARDS) {
                    emulator->ticks_between_cards--;
                    mifare_fuzzer_emulator_set_ticks_between_cards(
                        app->emulator_view, emulator->ticks_between_cards);
                };
            };
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {

        if(emulator->is_attacking) {

            tick_counter++;
            mifare_fuzzer_emulator_set_tick_num(app->emulator_view, tick_counter);

            if(tick_counter >= emulator->ticks_between_cards) {

                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MifareFuzzerEventStartAttack);
            }
        }

        consumed = true;
    }

    return consumed;
}

void mifare_fuzzer_scene_emulator_on_exit(void* context) {

    MifareFuzzerApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    mifare_fuzzer_worker_stop(app->worker);

    if(app->attack == MifareFuzzerAttackLoadUidsFromFile) {
        furi_string_reset(app->uid_str);
        stream_rewind(app->uids_stream);
        buffered_file_stream_close(app->uids_stream);
    }
}
