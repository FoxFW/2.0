// scenes/gdr_scene_config.h

#include "../defines.h"

ADD_SCENE(gdr, start, Start)
#ifdef ENABLE_SUB_DECODE_SCENE
ADD_SCENE(gdr, sub_decode, SubDecode)
#endif
#ifdef ENABLE_DUAL_RX_SCENE
ADD_SCENE(gdr, dual_receiver, DualReceiver)
ADD_SCENE(gdr, dual_receiver_config, DualReceiverConfig)
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
ADD_SCENE(gdr, shield_receiver, ShieldReceiver)
ADD_SCENE(gdr, shield_receiver_config, ShieldReceiverConfig)
#endif
ADD_SCENE(gdr, receiver, Receiver)
ADD_SCENE(gdr, receiver_config, ReceiverConfig)
ADD_SCENE(gdr, receiver_info, ReceiverInfo)
ADD_SCENE(gdr, need_saving, NeedSaving)
ADD_SCENE(gdr, saved, Saved)
ADD_SCENE(gdr, saved_info, SavedInfo)
#ifdef ENABLE_EMULATE_FEATURE
ADD_SCENE(gdr, emulate, Emulate)
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
ADD_SCENE(gdr, timing_tuner, TimingTuner)
#endif
