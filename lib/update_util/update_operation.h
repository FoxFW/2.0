#pragma once

#include <stdbool.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_OPERATION_ROOT_DIR_PACKAGE_MAGIC 0
#define UPDATE_OPERATION_MAX_MANIFEST_PATH_LEN  255u
#define UPDATE_OPERATION_MIN_MANIFEST_VERSION   2

bool update_operation_get_package_dir_name(const char* full_path, FuriString* out_manifest_dir);

typedef enum {
    UpdatePrepareResultOK,
    UpdatePrepareResultManifestPathInvalid,
    UpdatePrepareResultManifestFolderNotFound,
    UpdatePrepareResultManifestInvalid,
    UpdatePrepareResultStageMissing,
    UpdatePrepareResultStageIntegrityError,
    UpdatePrepareResultManifestPointerCreateError,
    UpdatePrepareResultManifestPointerCheckError,
    UpdatePrepareResultTargetMismatch,
    UpdatePrepareResultOutdatedManifestVersion,
    UpdatePrepareResultIntFull,
    UpdatePrepareResultUnspecifiedError,
} UpdatePrepareResult;

const char* update_operation_describe_preparation_result(const UpdatePrepareResult value);

UpdatePrepareResult update_operation_prepare(const char* manifest_file_path);

bool update_operation_get_current_package_manifest_path(Storage* storage, FuriString* out_path);

bool update_operation_is_armed(void);

void update_operation_disarm(void);

#ifdef __cplusplus
}
#endif
