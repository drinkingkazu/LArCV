/**
 * \file EventROI.h
 *
 * \ingroup core_DataFormat
 * 
 * \brief Class def header for a class larcv::EventROI
 *
 * @author kazuhiro
 */

/** \addtogroup core_DataFormat

    @{*/
#ifndef EVENTROI_H
#define EVENTROI_H

#include <iostream>
#include "EventBase.h"
#include "ROI.h"
#include "DataProductFactory.h"

namespace larcv {
  /**
    \class EventROI
    Event-wise class to store a collection of larcv::ROI
  */
  class EventROI : public EventBase {
    
  public:
    
    /// Default constructor
    EventROI(){}
    
    /// Default destructor
    ~EventROI(){}

    /// larcv::ROI array clearer
    void clear();

    /// larcv::ROI array const reference getter
    const std::vector<larcv::ROI>& ROIArray() const { return _part_v; }

    /// larcv::ROI array index accessor
    const ROI& at(ROIIndex_t id) const;

    /// larcv::ROI inserter
    void Append(const ROI& img);

    /// larcv::ROI array to replace what is stored
    void Set(const std::vector<larcv::ROI>& part_v);
#ifndef __CINT__
    /// Emplacer for larcv::ROI
    void Emplace(ROI&& img);
    /// Emplacer for larcv::ROI array
    void Emplace(std::vector<larcv::ROI>&& part_v);
#endif

  private:

    std::vector<larcv::ROI> _part_v;

  };
}

#include "IOManager.h"
namespace larcv {

  // Template instantiation for IO
  template<> inline std::string product_unique_name<larcv::EventROI>() { return "roi"; }
  template EventROI& IOManager::get_data<larcv::EventROI>(const std::string&);
  template EventROI& IOManager::get_data<larcv::EventROI>(const ProducerID_t);

  /**
     \class larcv::EventROI
     \brief A concrete factory class for larcv::EventROI
  */
  class EventROIFactory : public DataProductFactoryBase {
  public:
    /// ctor
    EventROIFactory()
    { DataProductFactory::get().add_factory(product_unique_name<larcv::EventROI>(),this); }
    /// dtor
    ~EventROIFactory() {}
    /// create method
    EventBase* create() { return new EventROI; }
  };

  
}
#endif
/** @} */ // end of doxygen group 

