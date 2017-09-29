#ifndef __DATAFORMAT_DATAFORMATUTIL_CXX__
#define __DATAFORMAT_DATAFORMATUTIL_CXX__

#include "DataFormatUtil.h"
#include <sstream>
#include "Base/larbys.h"

namespace larcv {

  ROIType_t PdgCode2ROIType(int pdgcode) 
  {

    if(pdgcode == 11 || pdgcode == -11) return kROIEminus;
    if(pdgcode == 321 || pdgcode == -321) return kROIKminus;
    if(pdgcode == 2212) return kROIProton;
    if(pdgcode == 13 || pdgcode == -13) return kROIMuminus;
    if(pdgcode == 211 || pdgcode == -211) return kROIPiminus;
    if(pdgcode == 22) return kROIGamma;
    if(pdgcode == 111) return kROIPizero;
    return kROIUnknown;

  }

  std::string ROIType2String(const ROIType_t type) 
  {
    switch(type) {
    case kROIUnknown: return "Unknown";
    case kROICosmic:  return "Cosmic";
    case kROIBNB:     return "BNB";
    case kROIEminus:  return "Eminus";
    case kROIGamma:   return "Gamms";
    case kROIPizero:  return "Pizero";
    case kROIMuminus: return "Muminus";
    case kROIKminus:  return "Kminus";
    case kROIPiminus: return "Piminus";
    case kROIProton:  return "Proton";
    default:
      std::stringstream ss;
      ss << "Unsupported type: " << type << std::endl;
      throw larbys(ss.str());
    }
    return "";
  }

  ROIType_t String2ROIType(const std::string& name)
  {
    if(name == "Unknown") return kROIUnknown;
    if(name == "Cosmic" ) return kROICosmic;
    if(name == "BNB"    ) return kROIBNB;
    if(name == "Eminus" ) return kROIEminus;
    if(name == "Gamma"  ) return kROIGamma;
    if(name == "Pizero" ) return kROIPizero;
    if(name == "Muminus") return kROIMuminus;
    if(name == "Kminus" ) return kROIKminus;
    if(name == "Piminus") return kROIPiminus;
    if(name == "Proton" ) return kROIProton;
    
    std::stringstream ss;
    ss << "Unsupported name: " << name << std::endl;
    throw larbys(ss.str());

    return kROIUnknown;
  }

  
}

#endif
