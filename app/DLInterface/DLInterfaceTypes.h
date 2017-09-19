#ifndef DLINTERFACETYPES_H
#define DLINTERFACETYPES_H

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

  enum class BatchDataState_t {
    kBatchStateUnknown,
    kBatchStateEmpty,
    kBatchStateFilling,
    kBatchStateFilled,
    kBatchStateReleased
  };
}

#endif
