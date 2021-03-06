#import ROOT,sys,time,os,signal
from larcv import larcv
import sys,time,os,signal
import numpy as np

class batch_pydata(object):

   _storage_id = -1
   _dtype      = None
   _npy_data   = None
   _dim_data   = None
   _time_copy  = 0
   _time_reshape = 0

   def __init__(self,dtype):
      self._storage_id = -1
      self._dtype = dtype
      self._npy_data = None
      self._dim_data = None
      self._time_copy = None
      self._time_reshape = None

   def batch_data_size(self):
      dsize=1
      for v in self._dim_data: dsize *= v
      return dsize

   def dtype(self): return self._dtype
   def data(self): return self._npy_data
   def dim(self):  return self._dim_data
   def time_copy(self): return self._time_copy
   def time_reshape(self): return self._time_reshape

   def set_data(self,storage_id,larcv_batchdata):
      self._storage_id = storage_id

      dim = larcv_batchdata.dim()

      # set dimension
      if self._dim_data is None:
         self._dim_data = np.array([dim[i] for i in xrange(dim.size())]).astype(np.int32)

      else:
         if not len(self._dim_data) == dim.size():
            sys.stderr.write('Dimension array length changed (%d => %d)\n' % (len(self._dim_data),dim.size()))
            raise TypeError
         for i in xrange(len(self._dim_data)):
            if not self._dim_data[i] == dim[i]:
               sys.stderr.write('%d-th dimension changed (%d => %d)\n' % (i,self._dim_data[i],dim[i]))
               raise ValueError
         
      # copy data into numpy array
      ctime = time.time()
      if self._npy_data is None:
         self._npy_data = np.array(larcv_batchdata.data())
      else:
         self._npy_data = self._npy_data.reshape(self.batch_data_size())
         larcv.copy_array(self._npy_data,larcv_batchdata.data())
      self._time_copy = time.time() - ctime

      ctime = time.time()
      #self._npy_data = self._npy_data.reshape(self._dim_data[0], self.batch_data_size()/self._dim_data[0]).astype(np.float32)
      self._npy_data = self._npy_data.reshape(self._dim_data[0], self.batch_data_size()/self._dim_data[0]).astype(np.float32)
      self.time_data_conv = time.time() - ctime

      return

class larcv_threadio (object):

   _instance_m={}

   @classmethod
   def exist(cls,name):
      name = str(name)
      return name in cls._instance_m

   @classmethod
   def instance_by_name(cls,name):
      return cls._instance_m[name]

   def __init__(self):
      self._proc = None
      self._name = ''
      self._verbose = False
      self._read_start_time = None
      self._read_end_time = None
      self._cfg_file = None
      self._next_storage_id = 0
      self._storage = {}

   def reset(self):
      if self._proc: self._proc.reset()

   def __del__(self):
      try:
         self.reset()
      except AttrbuteError:
         pass

   def configure(self,cfg):
      # if "this" was configured before, reset it
      if self._name: self.reset()
         
      # get name
      if not cfg['filler_name']:
         sys.stderr.write('filler_name is empty!\n')
         raise ValueError

      # ensure unique name
      if self.__class__.exist(cfg['filler_name']) and not self.__class__.instance_by_name(cfg['filler_name']) == self:
         sys.stderr.write('filler_name %s already running!' % cfg['filler_name'])
         return
      self._name = cfg['filler_name']         

      # get ThreadProcessor config file
      self._cfg_file = cfg['filler_cfg']
      if not self._cfg_file or not os.path.isfile(self._cfg_file):
         sys.stderr.write('filler_cfg file does not exist: %s\n' % self._cfg_file)
         raise ValueError
         
      # set verbosity
      if 'verbosity' in cfg:
         self._verbose = bool(cfg['verbosity'])

      # configure thread processor
      self._proc = larcv.ThreadProcessor(self._name)
      self._proc.configure(self._cfg_file)

      # fetch batch filler info
      self._storage = {}
      for i in xrange(self._proc.batch_fillers().size()):
         pid = self._proc.batch_fillers()[i]
         name = self._proc.storage_name(pid)
         dtype = larcv.BatchDataTypeName(self._proc.batch_types()[i])
         self._storage[name]=batch_pydata(dtype)

      # all success?
      # register *this* instance
      self.__class__._instance_m[self._name] = self

   def start_manager(self, batch_size):
      if not self._proc or not self._proc.configured():
         sys.stderr.write('must call configure(cfg) before start_manager()!\n')
         return
      try:
         batch_size=int(batch_size)
         if batch_size<1:
            sys.stderr.write('batch_size must be positive integer!\n')
            raise ValueError
      except TypeError, ValueError:
         sys.stderr.write('batch_size value/type error. aborting...\n')
         return

      self._batch=batch_size
      self._proc.start_manager(batch_size)
      self._next_storage_id=0

   def is_reading(self):
      return (not self._proc.storage_status_array()[self._next_storage_id] == 3)

   def next(self):
      if not self._proc or not self._proc.manager_started():
         sys.stderr.write('must call start_manager(batch_size) before next()!\n')
         return

      self._read_start_time = time.time()
      sleep_ctr=0
      while self.is_reading():
         time.sleep(0.01)
         sleep_ctr+=1
         #if sleep_ctr%100 ==0:
         #   print
         #   print 'queueing... (%d sec)' % (0.01*sleep_ctr)
      self._read_end_time = time.time()

      for name,storage in self._storage.iteritems():
         dtype = storage.dtype()
         batch_data = larcv.BatchDataStorageFactory(dtype).get().get_storage(name).get_batch(self._next_storage_id)
         storage.set_data(self._next_storage_id, batch_data)

      self._proc.release_data(self._next_storage_id)
      self._next_storage_id += 1
      if self._next_storage_id == self._proc.num_batch_storage():
         self._next_storage_id = 0

      return 

   def fetch_data(self,key):
      try:
         return self._storage[key]
      except KeyError:
         sys.stderr.write('Cannot fetch data w/ key %s (unknown)\n' % key)
         return

def sig_kill(signal,frame):
   print '\033[95mSIGINT detected.\033[00m Finishing the program gracefully.'
   for name,ptr in larcv_threadio._instance_m.iteritems():
      print 'Terminating filler:',name
      ptr.reset()

signal.signal(signal.SIGINT,  sig_kill)



