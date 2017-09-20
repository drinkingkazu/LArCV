#ifndef THREADPROCESSOR_CXX
#define THREADPROCESSOR_CXX

#include "ThreadProcessor.h"
#include "Base/LArCVBaseUtilFunc.h"
#include <random>
#include <sstream>
#include <unistd.h>
#include "BatchFillerTemplate.h"
#include "BatchDataStorageFactory.h"
//#include <stdlib.h>
#include <mutex>

std::mutex __thproc_starter_mt;

namespace larcv {
  ThreadProcessor::ThreadProcessor(std::string name)
    : larcv_base(name)
    , _run_manager_thread(false)
    , _processing(false)
    , _configured(false)
    , _enable_filter(false)
    , _optional_next_index(kINVALID_SIZE)
    , _next_entry(0)
    , _num_batch_storage(0)
    , _batch_global_counter(0)
  {}

  ThreadProcessor::~ThreadProcessor()
  { reset(); }

  void ThreadProcessor::terminate_threads()
  {
    if(_run_manager_thread) stop_manager();
    for(auto& th : _thread_v)
      if (th.joinable()) th.join();
    for(auto& driver : _driver_v) {
      if (driver->processing()) 
	driver->finalize();
      delete driver;
    }    
  }

  const std::string& ThreadProcessor::storage_name(size_t process_id) const
  {
    if(process_id > _process_name_v.size()) {
      LARCV_CRITICAL() << "Process ID " << process_id << " is invalid!" << std::endl;
      throw larbys();
    }
    return _process_name_v[process_id];
  }
  
  size_t ThreadProcessor::process_id(const std::string& name) const
  {
    for(size_t id=0; id<_process_name_v.size(); ++id)
      if(name == _process_name_v[id]) return id;
    LARCV_ERROR() << "Could not locate process name: " << name << std::endl;
    return kINVALID_SIZE;
  }

  void ThreadProcessor::start_manager(size_t batch_size) {
    if(!_configured) {
      LARCV_ERROR() << "Cannot start manager before configuration..." << std::endl;
      return;
    }
    if(_run_manager_thread) {
      LARCV_NORMAL() << "Manager thread already running..." << std::endl;
      return;
    }
    if(_manager_thread.joinable()) _manager_thread.join();
    
    std::thread t(&ThreadProcessor::manager_batch_process, this, batch_size);
    _manager_thread = std::move(t);
    usleep(5000);
    return;
  }

  void ThreadProcessor::stop_manager() {
    _run_manager_thread = false;
    usleep(5000);
    if(_manager_thread.joinable()) _manager_thread.join();
  }

  void ThreadProcessor::manager_batch_process(size_t batch_size)
  {
    _run_manager_thread = true;
    while(_run_manager_thread) {
      auto state = batch_process(batch_size);
      
      if(!state) {
	// somehow new thread did not start for reasons. sleep a bit (say 100ms)
	usleep(100000);
      }else
	usleep(10000);
    }
  }

  size_t ThreadProcessor::process_counter(size_t thread_id) const
  {
    if(thread_id >= _num_threads) {
      LARCV_ERROR() << "Requested state of an invalid thread id: " << thread_id << std::endl;
      return 0;
    }
    return _lifetime_valid_counter_v[thread_id];
  }

  size_t ThreadProcessor::process_counter() const
  {
    size_t total=0;
    for(size_t id=0; id<_num_threads; ++id) total += _lifetime_valid_counter_v[id];
    return total;
  }

  size_t ThreadProcessor::num_thread_running() const
  {
    size_t num=0;
    for(size_t thread_id=0; thread_id<_num_threads; ++thread_id)
      if(_thread_state_v[thread_id] != kThreadStateIdle) num++;
    return num;
  }

  void ThreadProcessor::status_dump() const
  {
    std::stringstream ss;
    for(size_t id=0; id<_num_threads; ++id) {
      ss << "    Thread ID " << id
	 << " # call " << _thread_exec_ctr_v[id]
	 << " Current batch " << _valid_counter_v[id] << "/" << _batch_size_v[id] << " entries"
	 << " Total processed " << _lifetime_valid_counter_v[id] << " entries" << std::endl;
    }
    LARCV_NORMAL() << "Status Summary: " << std::endl << ss.str();
  }

  bool ThreadProcessor::thread_running() const
  {
    for(size_t id=0; id<_num_threads; ++id)
      if(thread_running(id)) return true;
    return false;
  }

  bool ThreadProcessor::thread_running(size_t thread_id) const
  {
    if(thread_id >= _num_threads) {
      LARCV_ERROR() << "Requested state of an invalid thread id: " << thread_id << std::endl;
      return false;
    }
    return (_thread_state_v[thread_id] != kThreadStateIdle);
  }

  const std::vector<size_t>& ThreadProcessor::processed_entries(size_t storage_id) const
  {
    if(storage_id >= _num_batch_storage) {
      LARCV_CRITICAL() << "Requested state of an invalid storage id: " << storage_id << std::endl;
      throw larbys();
    }
    return _batch_entries_v[storage_id];
  }
  
  const std::vector<larcv::EventBase>& ThreadProcessor::processed_events(size_t storage_id) const
  {
    if(storage_id >= _num_batch_storage) {
      LARCV_CRITICAL() << "Requested state of an invalid storage id: " << storage_id << std::endl;
      throw larbys();
    }
    return _batch_events_v[storage_id];
  }

  size_t ThreadProcessor::get_n_entries() const
  {
    return _driver_v.front()->io().get_n_entries();
  }

  const ProcessDriver* ThreadProcessor::pd(size_t thread_id)
  {
    if(thread_id >= _num_threads) {
      LARCV_ERROR() << "Requested state of an invalid thread id: " << thread_id << std::endl;
      return nullptr;
    }
    return _driver_v[thread_id];
  }

  void ThreadProcessor::set_next_index(size_t index)
  {
    if (thread_running()) {
      LARCV_CRITICAL() << "Cannot set next index while thread is running!" << std::endl;
      throw larbys();
    }
    /*
    if( _optional_next_index_v.size() ) {
      LARCV_CRITICAL() << "Next batch indecies already set! Cannot call this function..." << std::endl;
      throw larbys();
    }
    */
    _optional_next_index = index;
  }


  void ThreadProcessor::set_next_batch(const std::vector<size_t>& index_v)
  {
    if (thread_running()) {
      LARCV_ERROR() << "Cannot set next index while thread is running!" << std::endl;
      return;
    }
    if( _optional_next_index != kINVALID_SIZE ) {
      LARCV_ERROR() << "Next batch indecies already set! Cannot call this function..." << std::endl;
      return;
    }
    if( _enable_filter ) {
      LARCV_ERROR() << "Cannot set a specific index array when filter mode is enabled!" << std::endl;
      return;
    }
    _optional_next_index_v = index_v;
  }

  size_t ThreadProcessor::batch_id(size_t storage_id) const
  {
    if(storage_id > _num_batch_storage) {
      LARCV_ERROR() << "Storage id " << storage_id << " is invalid" <<std::endl;
      return kINVALID_SIZE;
    }
    return _batch_global_id[storage_id];
  }

  void ThreadProcessor::reset()
  {
    terminate_threads();
    // per-thread variables
    _thread_state_v.clear();
    _driver_v.clear();
    _thread_v.clear();
    _current_storage_id.clear();
    _thread_exec_ctr_v.clear();
    _batch_size_v.clear();
    _valid_counter_v.clear();
    _lifetime_valid_counter_v.clear();
    // per-storage variables
    _batch_state_v.clear();
    _batch_entries_v.clear();
    _batch_events_v.clear();
    _batch_global_id.clear();
    // per-process variables
    _process_name_v.clear();

    // others
    _configured = false;
    _optional_next_index = kINVALID_SIZE;
    _num_batch_storage = 1;
    _num_threads = 1;
    _next_entry = kINVALID_SIZE;
    _batch_global_counter = 0;
  }

  void ThreadProcessor::configure(const std::string config_file)
  {
    LARCV_DEBUG() << "Called" << std::endl;
    // check state
    if (_processing) {
      LARCV_CRITICAL() << "Must call finalize() before calling initialize() after starting to process..." << std::endl;
      throw larbys();
    }
    // check cfg file
    if (config_file.empty()) {
      LARCV_CRITICAL() << "Config file not set!" << std::endl;
      throw larbys();
    }

    // check cfg content top level
    auto main_cfg = CreatePSetFromFile(config_file);
    if (!main_cfg.contains_pset(name())) {
      LARCV_CRITICAL() << "ThreadProcessor configuration (" << name() << ") not found in the config file (dump below)" << std::endl
                       << main_cfg.dump()
                       << std::endl;
      throw larbys();
    }
    auto const cfg = main_cfg.get<larcv::PSet>(name());
    configure(cfg);
  }

  void ThreadProcessor::release_data(size_t storage_id)
  {
    if(storage_id >= _batch_state_v.size()) {
      LARCV_ERROR() << "Cannot release storage ID " << storage_id
		    << " (exceeding # storage = " << _batch_state_v.size()
		    << ")" << std::endl;
      return;
    }
    if(_batch_state_v[storage_id] == BatchDataState_t::kBatchStateFilling) {
      LARCV_ERROR() << "Cannot release storage data " << storage_id
		    << " while it is being filled!" << std::endl;
      throw larbys();
      return;
    }
    _batch_state_v[storage_id] = BatchDataState_t::kBatchStateReleased;
    LARCV_NORMAL() << "Storage data (id=" << storage_id <<") released" << std::endl;
  }

  void ThreadProcessor::configure(const PSet& orig_cfg)
  {
    reset();
    /*
    PSet cfg(name());
    for (auto const& value_key : orig_cfg.value_keys())
      cfg.add_value(value_key, orig_cfg.get<std::string>(value_key));
    */
    std::cout<<"\033[93m setting verbosity \033[00m" << orig_cfg.get<unsigned short>("Verbosity", 2) << std::endl;
    set_verbosity( (msg::Level_t)(orig_cfg.get<unsigned short>("Verbosity", 2)) );
    _enable_filter = orig_cfg.get<bool>("EnableFilter");
    //_use_threading = orig_cfg.get<bool>("UseThread", true);
    _num_threads   = orig_cfg.get<size_t>("NumThreads",1);
    _input_fname_v = orig_cfg.get<std::vector<std::string> >("InputFiles");
    _num_batch_storage = orig_cfg.get<size_t>("NumBatchStorage",_num_threads);

    LARCV_INFO() << "Number of threads: " << _num_threads << " ... Number of batch storage: " << _num_batch_storage << std::endl;

    // Initialize NumStorages related variables
    _batch_entries_v.clear();
    _batch_entries_v.resize(_num_batch_storage);
    _batch_events_v.clear();
    _batch_events_v.resize(_num_batch_storage);
    _batch_state_v.clear();
    _batch_state_v.resize(_num_batch_storage,BatchDataState_t::kBatchStateEmpty);
    _batch_global_id.clear();
    _batch_global_id.resize(_num_batch_storage,kINVALID_SIZE);
    // Initialize NumThreads related variables
    _driver_v.clear();
    _process_name_v.clear();
    _thread_state_v.clear();
    _thread_state_v.resize(_num_threads,kThreadStateIdle);
    _current_storage_id.clear();
    _current_storage_id.resize(_num_threads,kINVALID_SIZE);
    _batch_size_v.clear();
    _batch_size_v.resize(_num_threads,0);
    _thread_exec_ctr_v.clear();
    _thread_exec_ctr_v.resize(_num_threads,0);
    _valid_counter_v.clear();
    _valid_counter_v.resize(_num_threads,0);
    _lifetime_valid_counter_v.clear();
    _lifetime_valid_counter_v.resize(_num_threads,0);
    
    for(size_t thread_id=0; thread_id<_num_threads; ++thread_id) {

      std::stringstream ss_tmp1;
      ss_tmp1 << name() << thread_id;

      std::string proc_name(ss_tmp1.str());
      std::string io_cfg_name = proc_name + "IOManager";

      LARCV_INFO() << "Constructing Processor config: " << proc_name << std::endl;      
      PSet proc_cfg(proc_name);
      for (auto const& value_key : orig_cfg.value_keys()) {
	if(value_key == "ProcessName") {
	  std::stringstream ss_tmp2;
	  bool first=true;
	  for(auto const& unit_name : orig_cfg.get<std::vector<std::string> >("ProcessName")) {
	    if(first) {
	      ss_tmp2 << "[\"" << unit_name << "_t" << thread_id << "\"";
	      first = false;
	    }
	    else ss_tmp2 << ",\"" <<  unit_name << "_t" << thread_id << "\"";
	    if(thread_id==0) _process_name_v.push_back(unit_name);
	  }
	  ss_tmp2 << "]";
	  proc_cfg.add_value(value_key,ss_tmp2.str());
	}
	else
	  proc_cfg.add_value(value_key, orig_cfg.get<std::string>(value_key));
      }

      // Brew read-only configuration
      PSet io_cfg(io_cfg_name);
      io_cfg.add_value("Verbosity", std::string(std::to_string(logger().level())));
      io_cfg.add_value("Name", io_cfg_name);
      io_cfg.add_value("IOMode", "0");
      io_cfg.add_value("OutFileName", "");
      io_cfg.add_value("StoreOnlyType", "[]");
      io_cfg.add_value("StoreOnlyName", "[]");

      LARCV_INFO() << "Constructing IO configuration: " << io_cfg_name << std::endl;
    
      for (auto const& pset_key : orig_cfg.pset_keys()) {
	if (pset_key == "IOManager") {
	  auto const& orig_io_cfg = orig_cfg.get_pset(pset_key);
	  if(orig_io_cfg.contains_value("ReadOnlyName"))
	    io_cfg.add_value("ReadOnlyName", orig_io_cfg.get<std::string>("ReadOnlyName"));
	  if(orig_io_cfg.contains_value("ReadOnlyType"))
	    io_cfg.add_value("ReadOnlyType", orig_io_cfg.get<std::string>("ReadOnlyType"));
	  LARCV_NORMAL() << "IOManager configuration will be ignored..." << std::endl;
	}
	else if(pset_key == "ProcessList") {
	  auto const& orig_thread_plist = orig_cfg.get<larcv::PSet>(pset_key);
	  PSet thread_plist("ProcessList");
	  for(auto const& plist_value_key : orig_thread_plist.value_keys())
	    thread_plist.add_value(plist_value_key,orig_thread_plist.get<std::string>(plist_value_key));
	  for(auto const& plist_pset_key : orig_thread_plist.pset_keys()) {
	    std::stringstream ss_tmp3;
	    ss_tmp3 << plist_pset_key << "_t" << thread_id;
	    PSet thread_pcfg(orig_thread_plist.get<larcv::PSet>(plist_pset_key));
	    thread_pcfg.rename(ss_tmp3.str());
	    thread_plist.add_pset(thread_pcfg);
	  }
	  proc_cfg.add_pset(thread_plist);
	}
	else
	  proc_cfg.add_pset(orig_cfg.get_pset(pset_key)); 
      }
      proc_cfg.add_pset(io_cfg);

      LARCV_INFO() << "Enforcing configuration ..." << std::endl;

      LARCV_INFO() << proc_cfg.dump() << std::endl;

      //throw larbys();
      // configure the driver
      auto driver = new ProcessDriver(proc_name);
      driver->configure(proc_cfg);
      driver->override_input_file(_input_fname_v);

      LARCV_NORMAL() << "Done configuration ..." << std::endl;

      // Check & report batch filler's presence
      for (auto const& process_name : driver->process_names()) {
	ProcessID_t id = driver->process_id(process_name);

	auto ptr = driver->process_ptr(id);

	LARCV_INFO() << "Process " << process_name << " ... ID=" << id << "... BatchFiller? : " << ptr->is("BatchFiller") << std::endl;
      }
      driver->initialize();
      _driver_v.emplace_back(driver);
      _thread_state_v[thread_id] = kThreadStateIdle;

      // only-once-operation among all threads: initialize storage
      if(thread_id) continue;
      _batch_filler_id_v.clear();
      _batch_data_type_v.clear();
      for(size_t pid=0; pid<_process_name_v.size(); ++pid) {
	auto proc_ptr = driver->process_ptr(pid);
	if(!(proc_ptr->is("BatchFiller"))) continue;
	_batch_filler_id_v.push_back(pid);
	_batch_data_type_v.push_back( ((BatchHolder*)(proc_ptr))->data_type() );
	auto const& name = _process_name_v[pid];
	switch( _batch_data_type_v.back() ) {
	case BatchDataType_t::kBatchDataChar:
	  BatchDataStorageFactory<char>::get_writeable().make_storage(name,_num_batch_storage); break;
	case BatchDataType_t::kBatchDataShort:
	  BatchDataStorageFactory<short>::get_writeable().make_storage(name,_num_batch_storage); break;
	case BatchDataType_t::kBatchDataInt:
	  BatchDataStorageFactory<int>::get_writeable().make_storage(name,_num_batch_storage); break;
	case BatchDataType_t::kBatchDataFloat:
	  BatchDataStorageFactory<float>::get_writeable().make_storage(name,_num_batch_storage); break;
	case BatchDataType_t::kBatchDataDouble:
	  BatchDataStorageFactory<double>::get_writeable().make_storage(name,_num_batch_storage); break;
	case BatchDataType_t::kBatchDataString:
	  BatchDataStorageFactory<std::string>::get_writeable().make_storage(name,_num_batch_storage); break;
	default:
	  LARCV_CRITICAL() << "Process name " << name
			   << " encountered none-supported BatchDataType_t: " << (int)(((BatchHolder*)(proc_ptr))->data_type()) << std::endl;
	  throw larbys();
	}
      }
    }
    _configured = true;
  }

  bool ThreadProcessor::batch_process(size_t nentries) {

    LARCV_DEBUG() << " start" << std::endl;

    // must be configured
    if (!_configured) {
      LARCV_ERROR() << "Must call configure() before run process!" << std::endl;
      return false;
    }
    // must be non-zero entries to process
    if(!nentries) {
      LARCV_ERROR() << "nentries must be positive integer..." << std::endl;
      return false;
    }

    // figure out next storage id to be filled
    size_t storage_id = 0;
    if(_batch_global_counter==kINVALID_SIZE) _batch_global_counter = 0;
    storage_id = _batch_global_counter % _num_batch_storage;

    // check if the storage is ready-to-be-used
    if(_batch_state_v[storage_id] != BatchDataState_t::kBatchStateEmpty &&
       _batch_state_v[storage_id] != BatchDataState_t::kBatchStateReleased) {
      LARCV_INFO() << "Target storage id " << storage_id
		   << " / " << _batch_global_counter
		   << " status " << (int)(_batch_state_v[storage_id])
		   << " ... not ready (must be kBatchStateEmpty="
		   << (int)(BatchDataState_t::kBatchStateEmpty)
		   << " or kBatchStateReleased="
		   << (int)(BatchDataState_t::kBatchStateReleased) << ")" << std::endl;
      return false;
    }

    // next, figure out thread_id that can be used
    size_t thread_id = kINVALID_SIZE;
    for(size_t id=0; id < _num_threads; ++id) {
      if(_thread_state_v[id] != kThreadStateIdle) continue;
      thread_id = id;
      break;
    }
    if(thread_id == kINVALID_SIZE) {
      LARCV_NORMAL() << "Skip running next batch: no thread is ready to take a job" << std::endl;
      return false;
    }

    //
    // execute
    //
    _processing = true;
    _current_storage_id[thread_id] = storage_id;
    // set the "last storage id" and "global storage id"
    _batch_global_id[storage_id] = _batch_global_counter;
    _batch_global_counter += 1;
    
    if(_thread_v.size() > thread_id && _thread_v[thread_id].joinable()) {
      LARCV_INFO() << "Thread has finished running but not joined. "
                   << "You might want to retrieve data?" << std::endl;
      _thread_v[thread_id].join();
    }

    // figure out "start entry"
    size_t start_entry = _next_entry;
    if(_optional_next_index != kINVALID_SIZE) {
      start_entry = _optional_next_index;
      _optional_next_index = kINVALID_SIZE;
    }
    if(start_entry == kINVALID_SIZE)
      start_entry = 0;
    
    LARCV_NORMAL() << "Instantiating thread ID " << thread_id
		   << " (exec counter " << _thread_exec_ctr_v[thread_id] << ")"
		   << " for storage id " << storage_id
		   << " (from entry " << start_entry
		   << ", for " << nentries << " entries)" << std::endl;

    _next_entry = start_entry + nentries;
    
    //
    // Assign appropriate batch data storage pointer
    //
    auto& driver = _driver_v[thread_id];
    for(size_t pid=0; pid < _process_name_v.size(); ++pid) {
      auto proc_ptr = driver->process_ptr(pid);
      if(!(proc_ptr->is("BatchFiller"))) continue;

      auto const& name = _process_name_v[pid];
      BatchDataState_t batch_state = BatchDataState_t::kBatchStateUnknown;
      switch( ((BatchHolder*)(proc_ptr))->data_type() ) {
      case BatchDataType_t::kBatchDataChar:
	((BatchFillerTemplate<char>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<char>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<char>*)proc_ptr)->_batch_data_ptr->state();
	break;
      case BatchDataType_t::kBatchDataShort:
	((BatchFillerTemplate<short>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<short>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<short>*)proc_ptr)->_batch_data_ptr->state();
	break;
      case BatchDataType_t::kBatchDataInt:
	((BatchFillerTemplate<int>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<int>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<int>*)proc_ptr)->_batch_data_ptr->state();
	break;
      case BatchDataType_t::kBatchDataFloat:
	((BatchFillerTemplate<float>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<float>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<float>*)proc_ptr)->_batch_data_ptr->state();
	break;
      case BatchDataType_t::kBatchDataDouble:
	((BatchFillerTemplate<double>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<double>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<double>*)proc_ptr)->_batch_data_ptr->state();
	break;
      case BatchDataType_t::kBatchDataString:
	((BatchFillerTemplate<std::string>*)proc_ptr)->_batch_data_ptr
	  = &(BatchDataStorageFactory<std::string>::get_writeable().get_storage_writeable(name).get_batch_writeable(storage_id));
	batch_state = ((BatchFillerTemplate<std::string>*)proc_ptr)->_batch_data_ptr->state();
	break;
      default:
	LARCV_CRITICAL() << "Process name " << name
			 << " encountered none-supported BatchDataType_t: " << (int)(((BatchHolder*)(proc_ptr))->data_type()) << std::endl;
	throw larbys();
      }

      // check to make sure BatchData is ready to be filled
      if(batch_state != BatchDataState_t::kBatchStateEmpty &&
	 batch_state != BatchDataState_t::kBatchStateUnknown &&
	 batch_state != BatchDataState_t::kBatchStateFilled ) {
	LARCV_CRITICAL() << "Thread ID " << thread_id
			 << " cannot fill storage id " << storage_id
			 << " because its state (" << (int)batch_state
			 << ") is neither kBatchStateUnknown nor kBatchStateEmpty nor kBatchStateFilled!" << std::endl;
	throw larbys();
      }
    }
    // set storage status to filling
    _batch_state_v[storage_id]  = BatchDataState_t::kBatchStateFilling;
    
    _thread_state_v[thread_id] = kThreadStateStarting;
    _batch_size_v[thread_id] = nentries;
    _valid_counter_v[thread_id] = 0;
    _thread_exec_ctr_v[thread_id] += 1;
    std::thread t(&ThreadProcessor::_batch_process_, this, start_entry, nentries, thread_id);
    if(_thread_v.size()<=thread_id) _thread_v.resize(thread_id+1);
    //_thread_v[thread_id] = std::move(t);
    std::swap(_thread_v[thread_id],t);
    usleep(1000);
    while(_thread_state_v[thread_id]==kThreadStateStarting) usleep(500);
    
    return true;
  }

  bool ThreadProcessor::_batch_process_(size_t start_entry, size_t nentries, size_t thread_id)
  {
    LARCV_DEBUG() << " start" << std::endl;
    _thread_state_v[thread_id] = kThreadStateRunning;
    auto driver = _driver_v[thread_id];
    /*
    if (!(driver->processing())) {
      LARCV_INFO() << "Initializing for 1st time processing" << std::endl;
      driver->initialize();
    }
    */
    auto const& storage_id = _current_storage_id[thread_id];
    auto& batch_entries = _batch_entries_v[storage_id];
    auto& batch_events  = _batch_events_v[storage_id];
    
    batch_entries.resize(nentries,0);
    batch_events.clear();
    batch_events.reserve(nentries);

    for (auto const& process_name : driver->process_names()) {
      ProcessID_t id = driver->process_id(process_name);
      auto ptr = driver->process_ptr(id);      
      if(!(ptr->is("BatchFiller"))) continue;
      ((BatchHolder*)(ptr))->_batch_size = nentries;
      switch( ((BatchHolder*)(ptr))->data_type() ) {
      case BatchDataType_t::kBatchDataChar:
	((BatchFillerTemplate<char>*)ptr)->batch_begin(); break;
      case BatchDataType_t::kBatchDataShort:
	((BatchFillerTemplate<short>*)ptr)->batch_begin(); break;
      case BatchDataType_t::kBatchDataInt:
	((BatchFillerTemplate<int>*)ptr)->batch_begin(); break;
      case BatchDataType_t::kBatchDataFloat:
	((BatchFillerTemplate<float>*)ptr)->batch_begin(); break;
      case BatchDataType_t::kBatchDataDouble:
	((BatchFillerTemplate<double>*)ptr)->batch_begin(); break;
      case BatchDataType_t::kBatchDataString:
	((BatchFillerTemplate<std::string>*)ptr)->batch_begin(); break;
      default:
	LARCV_CRITICAL() << "Thread ID " << thread_id
			 << " encountered none-supported BatchDataType_t: " << (int)(((BatchHolder*)(ptr))->data_type()) << std::endl;
	throw larbys();
      }
    }
    
    auto& valid_ctr = _valid_counter_v[thread_id];
    auto& lifetime_valid_ctr = _lifetime_valid_counter_v[thread_id];
    size_t next_entry = start_entry;
    
    LARCV_INFO() << "Entering process loop" << std::endl;
    while (valid_ctr < nentries) {
      size_t entry = next_entry;
      if(_optional_next_index_v.empty()) {
	entry = entry % driver->io().get_n_entries();
	next_entry = entry + 1;
      }
      else {
	entry = entry % _optional_next_index_v.size();
	next_entry = entry + 1;
	entry = _optional_next_index_v[entry];
      }
      
      LARCV_INFO() << "Processing entry: " << entry
		   << " (tree index=" << driver->get_tree_index( entry ) << ")" << std::endl;
      
      bool good_status = driver->process_entry(entry, true);
      if (_enable_filter && !good_status) {
        LARCV_INFO() << "Filter enabled: bad event found" << std::endl;
        continue;
      }
      LARCV_INFO() << "Finished processing event id: " << driver->event_id().event_key() << std::endl;

      batch_entries[valid_ctr] = driver->get_tree_index( entry );
      batch_events.push_back(driver->event_id());
      ++valid_ctr;
      ++lifetime_valid_ctr;
      LARCV_INFO() << "Processed good event: valid entry counter = " << valid_ctr << " : " << batch_events.size() << std::endl;
    }

    for (auto const& process_name : driver->process_names()) {
      ProcessID_t id = driver->process_id(process_name);
      auto ptr = driver->process_ptr(id);
      if(!(ptr->is("BatchFiller"))) continue;
      
      switch( ((BatchHolder*)(ptr))->data_type() ) {
      case BatchDataType_t::kBatchDataChar:
	((BatchFillerTemplate<char>*)ptr)->batch_end(); break;
      case BatchDataType_t::kBatchDataShort:
	((BatchFillerTemplate<short>*)ptr)->batch_end(); break;
      case BatchDataType_t::kBatchDataInt:
	((BatchFillerTemplate<int>*)ptr)->batch_end(); break;
      case BatchDataType_t::kBatchDataFloat:
	((BatchFillerTemplate<float>*)ptr)->batch_end(); break;
      case BatchDataType_t::kBatchDataDouble:
	((BatchFillerTemplate<double>*)ptr)->batch_end(); break;
      case BatchDataType_t::kBatchDataString:
	((BatchFillerTemplate<std::string>*)ptr)->batch_end(); break;
      default:
	LARCV_CRITICAL() << "Thread ID " << thread_id
			 << " encountered none-supported BatchDataType_t: " << (int)(((BatchHolder*)(ptr))->data_type()) << std::endl;
	throw larbys();
      }
    }
    _thread_state_v[thread_id] = kThreadStateIdle;
    _batch_state_v[storage_id] = BatchDataState_t::kBatchStateFilled;
    _optional_next_index = kINVALID_SIZE;
    LARCV_DEBUG() << " end" << std::endl;
    return true;
  }
}

#endif
