from plotimage import PlotImage
from .. import np


class DefaultImage(PlotImage):

    def __init__(self, img_v, roi_v, planes):
        super(DefaultImage, self).__init__(img_v, roi_v, planes)
        self.name = "DefaultImage"

    def __create_mat__(self):

        # compressed images all have the same shape
        self.orig_mat = np.zeros(list(self.img_v[0].shape) + [3])

        for p, fill_ch in enumerate(self.planes):

            if fill_ch == -1:
                continue

            self.orig_mat[:, :, p] = self.img_v[fill_ch]
            self.idx[fill_ch] = p

        self.orig_mat = self.orig_mat[:, ::-1, :]

    # def __threshold_mat__(self,imin,imax):

    #     self.orig_mat[ self.orig_mat < imin ] = 0
    #     self.orig_mat[ self.orig_mat > imax ] = imax

    def __set_plot_mat__(self, imin, imax):

        self.plot_mat = self.orig_mat.copy()
        
        # do contrast threhsolding
        self.plot_mat[self.plot_mat < imin] = 0
        self.plot_mat[self.plot_mat > imax] = imax

        #make sure pixels do not block each other
        self.plot_mat[:, :, 0][self.plot_mat[:, :, 1] > 0.0] = 0.0
        self.plot_mat[:, :, 0][self.plot_mat[:, :, 2] > 0.0] = 0.0
        self.plot_mat[:, :, 1][self.plot_mat[:, :, 2] > 0.0] = 0.0

        return self.plot_mat

    # revert back to how image was in ROOTFILE

    def __revert_image__(self):
        self.orig_mat = self.orig_mat[:, ::-1, :]

    def __create_rois__(self):

        for ix, roi in enumerate(self.roi_v):

            nbb = roi.BB().size()

            if nbb == 0:  # there was no ROI continue...
                continue

            r = {}

            r['type'] = roi.Type()
            r['bbox'] = []

            for iy in xrange(nbb):
                bb = roi.BB()[iy]
                r['bbox'].append(bb)

            self.rois.append(r)