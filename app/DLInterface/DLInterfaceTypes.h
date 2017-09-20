#ifndef DLINTERFACETYPES_H
#define DLINTERFACETYPES_H

#include <string>

namespace larcv {

  enum ThreadFillerState_t {
    kThreadStateIdle,
    kThreadStateStarting,
    kThreadStateRunning,
    kThreadStateUnknown
  };

  enum class BatchDataType_t {
    kBatchDataUnknown,
    kBatchDataChar,
    kBatchDataShort,
    kBatchDataInt,
    kBatchDataFloat,
    kBatchDataDouble,
    kBatchDataString
  };

  std::string BatchDataTypeName(BatchDataType_t type);

  enum class BatchDataState_t {
    kBatchStateUnknown,
    kBatchStateEmpty,
    kBatchStateFilling,
    kBatchStateFilled,
    kBatchStateReleased
  };
}

#endif
