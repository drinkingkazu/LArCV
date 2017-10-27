/**
 * \file BatchFillerSegment2D.h
 *
 * \ingroup ThreadIO
 * 
 * \brief Class def header for a class BatchFillerSegment2D
 *
 * @author kazuhiro
 */

/** \addtogroup ThreadIO

    @{*/
#ifndef __BATCHFILLERSEGMENT2D_H__
#define __BATCHFILLERSEGMENT2D_H__

#include "Processor/ProcessFactory.h"
#include "BatchFillerTemplate.h"
#include "RandomCropper.h"
#include "DataFormat/EventImage2D.h"
namespace larcv {

  /**
     \class ProcessBase
     User defined class BatchFillerSegment2D ... these comments are used to generate
     doxygen documentation!
  */
  class BatchFillerSegment2D : public BatchFillerTemplate<float> {

  public:
    
    /// Default constructor
    BatchFillerSegment2D(const std::string name="BatchFillerSegment2D");
    
    /// Default destructor
    ~BatchFillerSegment2D(){}

    void configure(const PSet&);

    void initialize();

    bool process(IOManager& mgr);

    void finalize();

    const std::vector<bool>& mirrored() const { return _mirrored; }

  protected:

    void _batch_begin_();
    void _batch_end_();

  private:

    size_t set_image_size(const EventImage2D* image_data);
    void assert_dimension(const EventImage2D* image_data) const;
    
    bool _caffe_mode;
    std::string _image_producer;
    size_t _rows;
    size_t _cols;
    size_t _num_channels;
    std::vector<size_t> _slice_v;
    size_t _max_ch;
    std::vector<size_t> _caffe_idx_to_img_idx;
    std::vector<size_t> _mirror_caffe_idx_to_img_idx;
    std::vector<bool>   _mirrored;
    std::vector<float>  _entry_data;
    bool _mirror_image;
    bool _crop_image;
    std::vector<size_t> _roitype_to_class;
    RandomCropper _cropper;

  };

  /**
     \class larcv::BatchFillerSegment2DFactory
     \brief A concrete factory class for larcv::BatchFillerSegment2D
  */
  class BatchFillerSegment2DProcessFactory : public ProcessFactoryBase {
  public:
    /// ctor
    BatchFillerSegment2DProcessFactory() { ProcessFactory::get().add_factory("BatchFillerSegment2D",this); }
    /// dtor
    ~BatchFillerSegment2DProcessFactory() {}
    /// creation method
    ProcessBase* create(const std::string instance_name) {
      return new BatchFillerSegment2D(instance_name);
    }
  };

}

#endif
/** @} */ // end of doxygen group 

