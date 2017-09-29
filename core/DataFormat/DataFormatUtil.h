#ifndef __DATAFORMAT_DATAFORMATUTIL_H__
#define __DATAFORMAT_DATAFORMATUTIL_H__

#include "DataFormatTypes.h"

namespace larcv {

  /// Return larcv::ROI type from PdgCode
  ROIType_t PdgCode2ROIType(int pdgcode);

  std::string ROIType2String(const ROIType_t type);

  ROIType_t String2ROIType(const std::string& name);


}
#endif
