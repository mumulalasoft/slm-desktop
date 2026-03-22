#pragma once

namespace SlmFileOperationErrors {
constexpr const char kOperationFailed[] = "operation-failed";
constexpr const char kInvalidDestination[] = "invalid-destination";
constexpr const char kCancelled[] = "cancelled";
constexpr const char kJobCancelled[] = "job-cancelled";
constexpr const char kCopyMoveFailed[] = "copy-move-failed";
constexpr const char kCopyOrMoveFailed[] = "copy-or-move-failed";
constexpr const char kDeleteTrashFailed[] = "delete-trash-failed";
constexpr const char kDeleteOrTrashFailed[] = "delete-or-trash-failed";
constexpr const char kEmptyTrashFailed[] = "empty-trash-failed";
constexpr const char kProtectedTarget[] = "protected-target";
constexpr const char kDaemonFileOpError[] = "daemon-file-op-error";
} // namespace SlmFileOperationErrors
