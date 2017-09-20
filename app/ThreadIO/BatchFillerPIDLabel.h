/**
 * \file BatchFillerPIDLabel.h
 *
 * \ingroup Package_Name
 * 
 * \brief Class def header for a class BatchFillerPIDLabel
 *
 * @author kazuhiro
 */

/** \addtogroup Package_Name

    @{*/
#ifndef __BATCHFILLERPIDLABEL_H__
#define __BATCHFILLERPIDLABEL_H__

#include "Processor/ProcessFactory.h"
#include "BatchFillerTemplate.h"

namespace larcv {

  /**
     \class ProcessBase
     User defined class BatchFillerPIDLabel ... these comments are used to generate
     doxygen documentation!
  */
  class BatchFillerPIDLabel : public BatchFillerTemplate<int> {

  public:
    
    /// Default constructor
    BatchFillerPIDLabel(const std::string name="BatchFillerPIDLabel");
    
    /// Default destructor
    ~BatchFillerPIDLabel(){}

    void configure(const PSet&);

    void initialize();

    bool process(IOManager& mgr);

    void _batch_begin_();

    void _batch_end_();

    void finalize();

    //BatchDataType_t data_type() const { return BatchDataType_t::kBatchDataInt; }
    
  private:
    std::string _roi_producer;
    std::vector<size_t> _roitype_to_class;
    std::vector<int> _entry_data;
  };

  /**
     \class larcv::BatchFillerPIDLabelFactory
     \brief A concrete factory class for larcv::BatchFillerPIDLabel
  */
  class BatchFillerPIDLabelProcessFactory : public ProcessFactoryBase {
  public:
    /// ctor
    BatchFillerPIDLabelProcessFactory() { ProcessFactory::get().add_factory("BatchFillerPIDLabel",this); }
    /// dtor
    ~BatchFillerPIDLabelProcessFactory() {}
    /// creation method
    ProcessBase* create(const std::string instance_name) { return new BatchFillerPIDLabel(instance_name); }
  };

}

#endif
/** @} */ // end of doxygen group 

