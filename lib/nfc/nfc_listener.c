#include "nfc_listener.h"

#include <nfc/protocols/nfc_listener_defs.h>
#include <nfc/nfc_device_i.h>
#include "nfc_group_manager.h"

#include <furi.h>

typedef struct NfcListenerListElement {
    NfcProtocol protocol;
    NfcGenericInstance* listener;
    const NfcListenerBase* listener_api;
    struct NfcListenerListElement* child;
} NfcListenerListElement;

typedef struct {
    NfcListenerListElement* head;
    NfcListenerListElement* tail;
} NfcListenerList;

struct NfcListener {
    NfcProtocol protocol;
    Nfc* nfc;
    NfcListenerList list;
    NfcDevice* nfc_dev;
    bool list_valid;
};

static void nfc_listener_list_alloc(NfcListener* instance) {
    instance->list_valid = true;

    instance->list.head = malloc(sizeof(NfcListenerListElement));
    instance->list.head->protocol = instance->protocol;
    instance->list.head->listener = NULL;

    nfc_group_ensure_loaded(instance->protocol);
    instance->list.head->listener_api = nfc_listener_get_base(instance->protocol);
    instance->list.head->child = NULL;
    instance->list.tail = instance->list.head;
    if(instance->list.head->listener_api == NULL) instance->list_valid = false;

    do {
        NfcProtocol parent_protocol = nfc_protocol_get_parent(instance->list.head->protocol);
        if(parent_protocol == NfcProtocolInvalid) break;

        NfcListenerListElement* parent = malloc(sizeof(NfcListenerListElement));
        parent->protocol = parent_protocol;
        parent->listener = NULL;
        nfc_group_ensure_loaded(parent_protocol);
        parent->listener_api = nfc_listener_get_base(parent_protocol);
        parent->child = instance->list.head;

        instance->list.head = parent;
        if(parent->listener_api == NULL) instance->list_valid = false;
    } while(true);

    if(!instance->list_valid) {
        FURI_LOG_E("NfcListener", "chain build failed for protocol %d", instance->protocol);
        return;
    }

    NfcListenerListElement* iter = instance->list.head;
    NfcDeviceData* data_tmp = nfc_device_get_data_ptr(instance->nfc_dev, iter->protocol);
    iter->listener = iter->listener_api->alloc(instance->nfc, data_tmp);

    do {
        if(iter->child == NULL) break;
        data_tmp = nfc_device_get_data_ptr(instance->nfc_dev, iter->child->protocol);
        iter->child->listener = iter->child->listener_api->alloc(iter->listener, data_tmp);
        iter->listener_api->set_callback(
            iter->listener, iter->child->listener_api->run, iter->child->listener);

        iter = iter->child;
    } while(true);
}

static void nfc_listener_list_free(NfcListener* instance) {

    do {
        if(instance->list.head->listener_api != NULL && instance->list.head->listener != NULL) {
            instance->list.head->listener_api->free(instance->list.head->listener);
        }
        nfc_group_release(instance->list.head->protocol);
        NfcListenerListElement* child = instance->list.head->child;
        free(instance->list.head);
        if(child == NULL) break;
        instance->list.head = child;
    } while(true);
}

NfcListener* nfc_listener_alloc(Nfc* nfc, NfcProtocol protocol, const NfcDeviceData* data) {
    furi_check(nfc);
    furi_check(protocol < NfcProtocolNum);
    furi_check(data);
    furi_check(nfc_group_is_external(protocol) || nfc_listener_get_base(protocol));

    NfcListener* instance = malloc(sizeof(NfcListener));
    instance->nfc = nfc;
    instance->protocol = protocol;
    instance->nfc_dev = nfc_device_alloc();
    nfc_device_set_data(instance->nfc_dev, protocol, data);
    nfc_listener_list_alloc(instance);

    return instance;
}

void nfc_listener_free(NfcListener* instance) {
    furi_check(instance);

    nfc_listener_list_free(instance);
    nfc_device_free(instance->nfc_dev);
    free(instance);
}

NfcCommand nfc_listener_start_callback(NfcEvent event, void* context) {
    furi_assert(context);

    NfcListener* instance = context;
    furi_assert(instance->list.head);

    NfcCommand command = NfcCommandContinue;
    NfcGenericEvent generic_event = {
        .protocol = NfcProtocolInvalid,
        .instance = instance->nfc,
        .event_data = &event,
    };

    NfcListenerListElement* head_listener = instance->list.head;
    command = head_listener->listener_api->run(generic_event, head_listener->listener);

    return command;
}

void nfc_listener_start(NfcListener* instance, NfcGenericCallback callback, void* context) {
    furi_check(instance);
    if(!instance->list_valid) return;

    NfcListenerListElement* tail_element = instance->list.tail;
    tail_element->listener_api->set_callback(tail_element->listener, callback, context);
    nfc_start(instance->nfc, nfc_listener_start_callback, instance);
}

void nfc_listener_stop(NfcListener* instance) {
    furi_check(instance);

    nfc_stop(instance->nfc);
}

NfcProtocol nfc_listener_get_protocol(const NfcListener* instance) {
    furi_check(instance);

    return instance->protocol;
}

const NfcDeviceData* nfc_listener_get_data(const NfcListener* instance, NfcProtocol protocol) {
    furi_check(instance);
    furi_check(instance->protocol == protocol);
    if(!instance->list_valid) return NULL;

    NfcListenerListElement* tail_element = instance->list.tail;
    return tail_element->listener_api->get_data(tail_element->listener);
}
