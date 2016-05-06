from plotimage import PlotImage
from .. import np

class FakeColorImage(PlotImage):

    def __init__(self,img_v,roi_v,planes) :
        super(FakeColorImage,self).__init__(img_v,roi_v,planes)
        self.name = "FakeColorImage"
    
    def __create_mat__(self):

        #lets start keeping track of which index in self.img_v corresponds
        #to which channen in the plot image

        #compressed images all have the same shape
        self.orig_mat = np.zeros(list(self.img_v[0].shape) + [3])

        for p,fill_ch in enumerate(self.planes):

            if fill_ch==-1: continue            
            
            self.orig_mat[:,:,p] = self.img_v[fill_ch]

            self.idx[fill_ch] = p

    def __set_plot_mat__(self,imin,imax):
        
        self.plot_mat = self.orig_mat.copy()

        #threshold the image
        self.plot_mat = self.plot_mat[ self.plot_mat < imin ] = 0
        self.plot_mat = self.plot_mat[ self.plot_max > imax ] = imax


        #make sure pixels do not block each other
        #not yet
        return self.plot_mat

    def __revert_image__(self): 
        pass 

    def __create_rois__(self):
        
        for ix,roi in enumerate(self.roi_v) :

            nbb = roi.BB().size()
            
            if nbb == 0: #there was no ROI continue...
                continue

            r = {}

            r['type'] = roi.Type()
            r['bbox'] = []

            for iy in xrange(nbb):
                bb = roi.BB()[iy]
                r['bbox'].append(bb)
                
            self.rois.append(r)
