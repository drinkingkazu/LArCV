#ifndef __PARTICLECOUNTFILTER_CXX__
#define __PARTICLECOUNTFILTER_CXX__

#include "ParticleCountFilter.h"
#include "DataFormat/EventParticle.h"

namespace larcv {

  static ParticleCountFilterProcessFactory __global_ParticleCountFilterProcessFactory__;

  ParticleCountFilter::ParticleCountFilter(const std::string name)
    : ProcessBase(name)
  {}
    
  void ParticleCountFilter::configure(const PSet& cfg)
  {
    _part_producer = cfg.get<std::string>("ParticleProducer");
    _max_part_count = cfg.get<size_t>("MaxCount");
    _min_part_count = cfg.get<size_t>("MinCount",0);
  }

  void ParticleCountFilter::initialize()
  {}

  bool ParticleCountFilter::process(IOManager& mgr)
  {
    auto const& ev_part = mgr.get_data<larcv::EventParticle>(_part_producer);
    if(ev_part.size() >= _part_count_v.size()) {
      _part_count_v.resize(ev_part.size()+1,0);
    }
    _part_count_v[ev_part.size()] += 1;
    
    return (_min_part_count <= ev_part.size() && ev_part.size() <= _max_part_count);
  }

  void ParticleCountFilter::finalize()
  {
    double total_count = 0;
    for(auto const& v : _part_count_v) total_count += v;
    LARCV_NORMAL() << "Reporting Particle counts. Total events processed: " << (int)total_count << std::endl;
    for(size_t i=0; i<_part_count_v.size(); ++i) 
      LARCV_NORMAL() << "    Multi=" << i 
		     << " ... " << _part_count_v[i] << " events (" 
		     << _part_count_v[i] / total_count * 100 << " %)" << std::endl;
  }

}
#endif
