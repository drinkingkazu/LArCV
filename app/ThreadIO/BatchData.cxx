#ifndef BATCHDATA_CXX
#define BATCHDATA_CXX

#include "BatchData.h"
#include "Base/larcv_logger.h"
#include "Base/larbys.h"
namespace larcv {

  template<class T>
  const std::vector<T>& BatchData<T>::data() const
  {
    if(_state != BatchDataState_t::kBatchStateFilled) {
      LARCV_SCRITICAL() << "Current batch state: " << (int)_state
			<< " not ready to expose data!" << std::endl;
      throw larbys();
    }
    return _data;
  }

  template<class T>
  size_t BatchData<T>::data_size() const
  {
    if(_dim.empty()) return 0;
    if(!_data.empty()) return _data.size();
    size_t length=1;
    for(auto const& dim : _dim) length *= dim;
    return length;
  }

  template<class T>
  size_t BatchData<T>::entry_data_size() const
  {
    if(_dim.empty()) return 0;
    size_t length=1;
    for(size_t i=1; i<_dim.size(); ++i) length *= _dim[i];
    return length;
  }

  template<class T>
  void BatchData<T>::set_dim(const std::vector<int>& dim)
  {
    if(dim.empty()) {
      LARCV_SERROR() << "Dimension input has 0 length..." << std::endl;
      return;
    }
    if(_dim.size()) {
      bool allowed = (_dim.size() == dim.size());
      if(allowed) 
	for(size_t i=0; i<_dim.size(); ++i) {allowed = allowed && (_dim[i] == dim[i]);}

      if(allowed) return;
      
      LARCV_SCRITICAL() << "Dimension cannot be re-set!" << std::endl;
      throw larbys();
    }

    _dim = dim;
    reset_data();
  }

  template<class T>
  void BatchData<T>::set_entry_data(const std::vector<T>& entry_data)
  {
    if(_state != BatchDataState_t::kBatchStateFilling &&
       _state != BatchDataState_t::kBatchStateEmpty) {
      LARCV_SERROR() << "Current batch state: " << (int)(_state)
		     << " not ready for filling data..." << std::endl;
      return;
    }
    _state = BatchDataState_t::kBatchStateFilling;
    
    size_t entry_size = entry_data_size();
    if( (_current_size + entry_size) > data_size() ) {
      LARCV_SERROR() << "Current size (" << _current_size
		     << ") + entry data size (" << entry_size
		     << ") exceeds data buffer size (" << data_size()
		     << std::endl;
      return;
    }

    size_t entry_idx = 0;
    while(entry_idx<entry_data.size()) {
      _data[_current_size] = entry_data[entry_idx];
      ++entry_idx;
      ++_current_size;
    }

    if(_current_size == _data.size())
      _state = BatchDataState_t::kBatchStateFilled;
  }

  template <class T>
  void BatchData<T>::reset()
  {
    _data.clear(); _dim.clear();
    _current_size=0;
    _state = BatchDataState_t::kBatchStateEmpty;
  }

  template <class T>
  void BatchData<T>::reset_data()
  {
    if(_state == BatchDataState_t::kBatchStateFilling) {
      LARCV_SERROR() << "Cannot reset batch (is in kBatchStateFilling state!)" << std::endl;
      return;
    }
    _data.resize(data_size());
    _current_size=0;
    _state = BatchDataState_t::kBatchStateEmpty;
  }

}

template class larcv::BatchData<char>;
template class larcv::BatchData<short>;
template class larcv::BatchData<int>;
template class larcv::BatchData<long>;
template class larcv::BatchData<float>;
template class larcv::BatchData<double>;
template class larcv::BatchData<std::string>;
#endif
