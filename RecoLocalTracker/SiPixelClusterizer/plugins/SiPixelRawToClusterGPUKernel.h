#ifndef RecoLocalTracker_SiPixelClusterizer_plugins_SiPixelRawToClusterGPUKernel_h
#define RecoLocalTracker_SiPixelClusterizer_plugins_SiPixelRawToClusterGPUKernel_h

#include <algorithm>
#include <cuda_runtime.h>
#include "cuda/api_wrappers.h"

#include "FWCore/Utilities/interface/typedefs.h"
#include "HeterogeneousCore/CUDAUtilities/interface/GPUSimpleVector.h"
#include "siPixelRawToClusterHeterogeneousProduct.h"

class SiPixelFedCablingMapGPU;
class SiPixelGainForHLTonGPU;

namespace pixelgpudetails {

  // Phase 1 geometry constants
  const uint32_t layerStartBit    = 20;
  const uint32_t ladderStartBit   = 12;
  const uint32_t moduleStartBit   = 2;

  const uint32_t panelStartBit    = 10;
  const uint32_t diskStartBit     = 18;
  const uint32_t bladeStartBit    = 12;

  const uint32_t layerMask        = 0xF;
  const uint32_t ladderMask       = 0xFF;
  const uint32_t moduleMask       = 0x3FF;
  const uint32_t panelMask        = 0x3;
  const uint32_t diskMask         = 0xF;
  const uint32_t bladeMask        = 0x3F;

  const uint32_t LINK_bits        = 6;
  const uint32_t ROC_bits         = 5;
  const uint32_t DCOL_bits        = 5;
  const uint32_t PXID_bits        = 8;
  const uint32_t ADC_bits         = 8;

  // special for layer 1
  const uint32_t LINK_bits_l1     = 6;
  const uint32_t ROC_bits_l1      = 5;
  const uint32_t COL_bits_l1      = 6;
  const uint32_t ROW_bits_l1      = 7;
  const uint32_t OMIT_ERR_bits    = 1;

  const uint32_t maxROCIndex      = 8;
  const uint32_t numRowsInRoc     = 80;
  const uint32_t numColsInRoc     = 52;

  const uint32_t MAX_WORD = 2000;

  const uint32_t ADC_shift  = 0;
  const uint32_t PXID_shift = ADC_shift + ADC_bits;
  const uint32_t DCOL_shift = PXID_shift + PXID_bits;
  const uint32_t ROC_shift  = DCOL_shift + DCOL_bits;
  const uint32_t LINK_shift = ROC_shift + ROC_bits_l1;
  // special for layer 1 ROC
  const uint32_t ROW_shift = ADC_shift + ADC_bits;
  const uint32_t COL_shift = ROW_shift + ROW_bits_l1;
  const uint32_t OMIT_ERR_shift = 20;

  const uint32_t LINK_mask = ~(~uint32_t(0) << LINK_bits_l1);
  const uint32_t ROC_mask  = ~(~uint32_t(0) << ROC_bits_l1);
  const uint32_t COL_mask  = ~(~uint32_t(0) << COL_bits_l1);
  const uint32_t ROW_mask  = ~(~uint32_t(0) << ROW_bits_l1);
  const uint32_t DCOL_mask = ~(~uint32_t(0) << DCOL_bits);
  const uint32_t PXID_mask = ~(~uint32_t(0) << PXID_bits);
  const uint32_t ADC_mask  = ~(~uint32_t(0) << ADC_bits);
  const uint32_t ERROR_mask = ~(~uint32_t(0) << ROC_bits_l1);
  const uint32_t OMIT_ERR_mask = ~(~uint32_t(0) << OMIT_ERR_bits);

  struct DetIdGPU {
    uint32_t RawId;
    uint32_t rocInDet;
    uint32_t moduleId;
  };

  struct Pixel {
   uint32_t row;
   uint32_t col;
  };

  class Packing {
  public:
    using PackedDigiType = uint32_t;
      
    // Constructor: pre-computes masks and shifts from field widths
    __host__ __device__
    inline
    constexpr Packing(unsigned int row_w, unsigned int column_w,
                      unsigned int time_w, unsigned int adc_w) :
      row_width(row_w),
      column_width(column_w),
      adc_width(adc_w),
      row_shift(0),
      column_shift(row_shift + row_w),
      time_shift(column_shift + column_w),
      adc_shift(time_shift + time_w),
      row_mask(~(~0U << row_w)),
      column_mask( ~(~0U << column_w)),
      time_mask(~(~0U << time_w)),
      adc_mask(~(~0U << adc_w)),
      rowcol_mask(~(~0U << (column_w+row_w))),
      max_row(row_mask),
      max_column(column_mask),
      max_adc(adc_mask)
    { }

    uint32_t  row_width;
    uint32_t  column_width;
    uint32_t  adc_width;

    uint32_t  row_shift;
    uint32_t  column_shift;
    uint32_t  time_shift;
    uint32_t  adc_shift;

    PackedDigiType row_mask;
    PackedDigiType column_mask;
    PackedDigiType time_mask;
    PackedDigiType adc_mask;
    PackedDigiType rowcol_mask;

    uint32_t  max_row;
    uint32_t  max_column;
    uint32_t  max_adc;
  };

  __host__ __device__
  inline
  constexpr Packing packing() {
    return Packing(11, 11, 0, 10);
  }


  __host__ __device__
  inline
  uint32_t pack(uint32_t row, uint32_t col, uint32_t adc) {
    constexpr Packing thePacking = packing();
    adc = std::min(adc, thePacking.max_adc);

    return (row << thePacking.row_shift) |
           (col << thePacking.column_shift) |
           (adc << thePacking.adc_shift);
  }

  using error_obj = siPixelRawToClusterHeterogeneousProduct::error_obj;


  class SiPixelRawToClusterGPUKernel {
  public:
    SiPixelRawToClusterGPUKernel();
    ~SiPixelRawToClusterGPUKernel();

    
    SiPixelRawToClusterGPUKernel(const SiPixelRawToClusterGPUKernel&) = delete;
    SiPixelRawToClusterGPUKernel(SiPixelRawToClusterGPUKernel&&) = delete;
    SiPixelRawToClusterGPUKernel& operator=(const SiPixelRawToClusterGPUKernel&) = delete;
    SiPixelRawToClusterGPUKernel& operator=(SiPixelRawToClusterGPUKernel&&) = delete;

    void initializeWordFed(int fedId, unsigned int wordCounterGPU, const cms_uint32_t *src, unsigned int length);
    
    // Not really very async yet...
    void makeClustersAsync(const SiPixelFedCablingMapGPU *cablingMap, const unsigned char *modToUnp,
                           const SiPixelGainForHLTonGPU *gains,
                           const uint32_t wordCounter, const uint32_t fedCounter, bool convertADCtoElectrons,
                           bool useQualityInfo, bool includeErrors, bool debug,
                           cuda::stream_t<>& stream);

    auto getProduct() const {
      return siPixelRawToClusterHeterogeneousProduct::GPUProduct{
        pdigi_h, rawIdArr_h, clus_h, adc_h, error_h,
        nDigis, nModulesActive,
        xx_d, yy_d, adc_d, moduleInd_d, moduleStart_d,clus_d, clusInModule_d, moduleId_d
      };
    }

  private:
    // input
    unsigned int *word = nullptr;        // to hold input for rawtodigi
    unsigned char *fedId_h = nullptr;    // to hold fed index for each word

    // output
    uint32_t *pdigi_h = nullptr, *rawIdArr_h = nullptr;                   // host copy of output
    uint16_t *adc_h = nullptr; int32_t *clus_h = nullptr; // host copy of calib&clus output
    pixelgpudetails::error_obj *data_h = nullptr;
    GPU::SimpleVector<pixelgpudetails::error_obj> *error_h = nullptr;
    GPU::SimpleVector<pixelgpudetails::error_obj> *error_h_tmp = nullptr;

    uint32_t nDigis = 0;
    uint32_t nModulesActive = 0;

    // scratch memory buffers
    uint32_t * word_d;
    uint8_t *  fedId_d;
    uint32_t * pdigi_d;
    uint16_t * xx_d;
    uint16_t * yy_d;
    uint16_t * adc_d;
    uint16_t * moduleInd_d;
    uint32_t * rawIdArr_d;

    GPU::SimpleVector<error_obj> * error_d;
    error_obj * data_d;

    // these are for the clusterizer (to be moved)
    uint32_t * moduleStart_d;
    int32_t *  clus_d;
    uint32_t * clusInModule_d;
    uint32_t * moduleId_d;
    uint32_t * debug_d;
  };

  
  // configuration and memory buffers alocated on the GPU
  struct context {
    uint32_t * word_d;
    uint8_t *  fedId_d;
    uint32_t * pdigi_d;
    uint16_t * xx_d;
    uint16_t * yy_d;
    uint16_t * adc_d;
    uint16_t * moduleInd_d;
    uint32_t * rawIdArr_d;

    GPU::SimpleVector<error_obj> * error_d;
    error_obj * data_d;
      
    // these are for the clusterizer (to be moved)
    uint32_t * moduleStart_d;
    int32_t *  clus_d;
    uint32_t * clusInModule_d;
    uint32_t * moduleId_d;
    uint32_t * debug_d;
  };

  // wrapper function to call RawToDigi on the GPU from host side
  void RawToDigi_wrapper(context &, const SiPixelFedCablingMapGPU* cablingMapDevice,
                         SiPixelGainForHLTonGPU * const ped,
                         const uint32_t wordCounter, uint32_t *word,
                         const uint32_t fedCounter,  uint8_t *fedId_h,
                         bool convertADCtoElectrons, uint32_t * pdigi_h,
                         uint32_t *rawIdArr_h, GPU::SimpleVector<error_obj> *error_h,
                         GPU::SimpleVector<error_obj> *error_h_tmp, error_obj *data_h,
                         uint16_t * adc_h, int32_t * clus_h,
                         bool useQualityInfo, bool includeErrors, bool debug,
                         uint32_t & nModulesActive);

  // void initCablingMap();
  context initDeviceMemory();
  void freeMemory(context &);

  // see RecoLocalTracker/SiPixelClusterizer
  // all are runtime const, should be specified in python _cfg.py
  struct ADCThreshold {
    const int     thePixelThreshold       = 1000;     // default Pixel threshold in electrons
    const int     theSeedThreshold        = 1000;     // seed thershold in electrons not used in our algo
    const float   theClusterThreshold     = 4000;     // cluster threshold in electron
    const int     ConversionFactor        =   65;     // adc to electron conversion factor

    const int     theStackADC_            =  255;     // the maximum adc count for stack layer
    const int     theFirstStack_          =    5;     // the index of the fits stack layer
    const double  theElectronPerADCGain_  =  600;     // ADC to electron conversion
  };

}

#endif // RecoLocalTracker_SiPixelClusterizer_plugins_SiPixelRawToClusterGPUKernel_h
