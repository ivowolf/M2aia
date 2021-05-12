/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/

#include <m2Baseline.h>
#include <m2FsmSpectrumImage.h>
#include <m2Normalization.h>
#include <m2PeakDetection.h>
#include <m2Pooling.h>
#include <m2Process.hpp>
#include <m2RunningMedian.h>
#include <m2Smoothing.h>
#include <mitkImagePixelReadAccessor.h>
#include <mitkImagePixelWriteAccessor.h>
#include <mitkLabelSetImage.h>
#include <mitkProperties.h>
#include <mitkTimer.h>

template <class XAxisType, class IntensityType>
void m2::FsmSpectrumImage::FsmProcessor<XAxisType, IntensityType>::GrabIonImagePrivate(double cmInv,
                                                                                       double tol,
                                                                                       const mitk::Image *mask,
                                                                                       mitk::Image *destImage) const
{
  AccessByItk(destImage, [](auto itkImg) { itkImg->FillBuffer(0); });
  using namespace m2;
  // accessors
  mitk::ImagePixelWriteAccessor<DisplayImagePixelType, 3> imageAccess(destImage);
  mitk::ImagePixelReadAccessor<NormImagePixelType, 3> normAccess(p->GetNormalizationImage());
  std::shared_ptr<mitk::ImagePixelReadAccessor<mitk::LabelSetImage::PixelType, 3>> maskAccess;

  const auto _NormalizationStrategy = p->GetNormalizationStrategy();

  if (mask)
  {
    maskAccess.reset(new mitk::ImagePixelReadAccessor<mitk::LabelSetImage::PixelType, 3>(mask));
    MITK_INFO << "> Use mask image";
  }
  p->SetProperty("cm^-1", mitk::DoubleProperty::New(cmInv));
  p->SetProperty("tol", mitk::DoubleProperty::New(tol));
  auto mdMz = itk::MetaDataObject<double>::New();
  mdMz->SetMetaDataObjectValue(cmInv);
  auto mdTol = itk::MetaDataObject<double>::New();
  mdTol->SetMetaDataObjectValue(tol);
  p->GetMetaDataDictionary()["cm^-1"] = mdMz;
  p->GetMetaDataDictionary()["tol"] = mdTol;

  std::vector<XAxisType> xs = p->GetXAxis();
  std::vector<double> kernel;

  // Profile (continuous) spectrum

  const auto subRes = m2::Signal::Subrange(xs, cmInv - tol, cmInv + tol);
  const auto n = p->GetSpectra().size();
  // map all spectra to several threads for processing
  const unsigned t = p->GetNumberOfThreads();

  m2::Process::Map(n, t, [&](auto /*id*/, auto a, auto b) {
    const auto Smoother =
      m2::Signal::CreateSmoother<IntensityType>(p->GetSmoothingStrategy(), p->GetSmoothingHalfWindowSize(), false);

    auto BaselineSubstractor = m2::Signal::CreateSubstractBaselineConverter<IntensityType>(
      p->GetBaselineCorrectionStrategy(), p->GetBaseLineCorrectionHalfWindowSize());

    const auto Normalizor = m2::Signal::CreateNormalizor<IntensityType, XAxisType>(p->GetNormalizationStrategy());

    std::vector<IntensityType> ys(xs.size());

    const auto &_Spectra = p->GetSpectra();
    for (unsigned int i = a; i < b; ++i)
    {
      const auto &spectrum = _Spectra[i];
      std::copy(std::cbegin(spectrum.data), std::cend(spectrum.data), std::begin(ys));

      auto s = std::next(std::begin(ys), subRes.first);
      auto e = std::next(std::begin(ys), subRes.first + subRes.second);

      if (maskAccess && maskAccess->GetPixelByIndex(spectrum.index) == 0)
      {
        imageAccess.SetPixelByIndex(spectrum.index, 0);
        continue;
      }

      // if a normalization image exist, apply normalization before any further calculations are performed
      if (_NormalizationStrategy != m2::NormalizationStrategyType::None)
      {
        IntensityType norm = normAccess.GetPixelByIndex(spectrum.index);
        std::transform(std::begin(ys), std::end(ys), std::begin(ys), [&norm](auto &v) { return v / norm; });
      }

      Smoother(ys);
      BaselineSubstractor(ys);

      auto val = Signal::RangePooling<IntensityType>(s, e, p->GetRangePoolingStrategy());

      imageAccess.SetPixelByIndex(spectrum.index, val);
    }
  });
}

void m2::FsmSpectrumImage::InitializeProcessor()
{
  this->m_Processor.reset((m2::SpectrumImageBase::ProcessorBase *)new FsmProcessor<double, float>(this));
}

void m2::FsmSpectrumImage::InitializeGeometry()
{
  if (!m_Processor)
    this->InitializeProcessor();
  this->m_Processor->InitializeGeometry();
  this->SetImageGeometryInitialized(true);
}

void m2::FsmSpectrumImage::InitializeImageAccess()
{
  this->m_Processor->InitializeImageAccess();
  this->SetImageAccessInitialized(true);
}

template <class XAxisType, class IntensityType>
void m2::FsmSpectrumImage::FsmProcessor<XAxisType, IntensityType>::InitializeGeometry()
{
  auto &imageArtifacts = p->GetImageArtifacts();

  std::array<itk::SizeValueType, 3> imageSize = {p->GetPropertyValue<unsigned>("dim_x"), // n_x
                                                 p->GetPropertyValue<unsigned>("dim_y"), // n_y
                                                 p->GetPropertyValue<unsigned>("dim_z")};

  std::array<double, 3> imageOrigin = {p->GetPropertyValue<double>("origin x") * 0.001, // x_init
                                       p->GetPropertyValue<double>("origin y") * 0.001, // y_init
                                       p->GetPropertyValue<double>("origin z") * 0.001};

  using ImageType = itk::Image<m2::DisplayImagePixelType, 3>;
  auto itkIonImage = ImageType::New();
  ImageType::IndexType idx;
  ImageType::SizeType size;

  idx.Fill(0);

  for (unsigned int i = 0; i < imageSize.size(); i++)
    size[i] = imageSize[i];
  ImageType::RegionType region(idx, size);
  itkIonImage->SetRegions(region);
  itkIonImage->Allocate();
  itkIonImage->FillBuffer(0);

  auto s = itkIonImage->GetSpacing();
  auto o = itkIonImage->GetOrigin();
  o[0] = imageOrigin[0];
  o[1] = imageOrigin[1];
  o[2] = imageOrigin[2];

  //
  s[0] = p->GetPropertyValue<double>("spacing_x"); // x_delta
  s[1] = p->GetPropertyValue<double>("spacing_y"); // y_delta
  s[2] = p->GetPropertyValue<double>("spacing_z");

  auto d = itkIonImage->GetDirection();
  d[0][0] = -1;

  itkIonImage->SetSpacing(s);
  itkIonImage->SetOrigin(o);
  itkIonImage->SetDirection(d);

  {
    using LocalImageType = itk::Image<m2::DisplayImagePixelType, 3>;
    auto caster = itk::CastImageFilter<ImageType, LocalImageType>::New();
    caster->SetInput(itkIonImage);
    caster->Update();
    p->InitializeByItk(caster->GetOutput());

    mitk::ImagePixelWriteAccessor<m2::DisplayImagePixelType, 3> acc(p);
    std::memset(acc.GetData(), 0, imageSize[0] * imageSize[1] * imageSize[2] * sizeof(m2::DisplayImagePixelType));
  }

  {
    using LocalImageType = itk::Image<m2::IndexImagePixelType, 3>;
    auto caster = itk::CastImageFilter<ImageType, LocalImageType>::New();
    caster->SetInput(itkIonImage);
    caster->Update();
    auto indexImage = mitk::Image::New();
    imageArtifacts["index"] = indexImage;
    indexImage->InitializeByItk(caster->GetOutput());

    mitk::ImagePixelWriteAccessor<m2::IndexImagePixelType, 3> acc(indexImage);
    std::memset(acc.GetData(), 0, imageSize[0] * imageSize[1] * imageSize[2] * sizeof(m2::IndexImagePixelType));
  }

  {
    mitk::LabelSetImage::Pointer image = mitk::LabelSetImage::New();
    imageArtifacts["mask"] = image.GetPointer();

    image->Initialize((mitk::Image *)p);
    auto ls = image->GetActiveLabelSet();

    mitk::Color color;
    color.Set(0, 1, 0);
    auto label = mitk::Label::New();
    label->SetColor(color);
    label->SetName("Valid Spectrum");
    label->SetOpacity(0.0);
    label->SetLocked(true);
    label->SetValue(1);
    ls->AddLabel(label);
  }

  {
    using LocalImageType = itk::Image<m2::NormImagePixelType, 3>;
    auto caster = itk::CastImageFilter<ImageType, LocalImageType>::New();
    caster->SetInput(itkIonImage);
    caster->Update();
    auto normImage = mitk::Image::New();
    imageArtifacts["NormalizationImage"] = normImage;
    normImage->InitializeByItk(caster->GetOutput());

    mitk::ImagePixelWriteAccessor<m2::NormImagePixelType, 3> acc(normImage);
    std::memset(acc.GetData(), 1, imageSize[0] * imageSize[1] * imageSize[2] * sizeof(m2::NormImagePixelType));
  }

  mitk::ImagePixelWriteAccessor<m2::DisplayImagePixelType, 3> acc(p);
  auto max_dim0 = p->GetDimensions()[0];
  auto max_dim1 = p->GetDimensions()[1];
  acc.SetPixelByIndex({0, 0, 0}, 1);
  acc.SetPixelByIndex({0, max_dim1 - 1, 0}, max_dim1 / 2);
  acc.SetPixelByIndex({max_dim0 - 1, 0, 0}, max_dim0 / 2);
  acc.SetPixelByIndex({max_dim0 - 1, max_dim1 - 1, 0}, max_dim1 + max_dim0);
}

template <class XAxisType, class IntensityType>
void m2::FsmSpectrumImage::FsmProcessor<XAxisType, IntensityType>::InitializeImageAccess()
{
  using namespace m2;

  auto accMask = std::make_shared<mitk::ImagePixelWriteAccessor<mitk::LabelSetImage::PixelType, 3>>(p->GetMaskImage());
  auto accIndex = std::make_shared<mitk::ImagePixelWriteAccessor<m2::IndexImagePixelType, 3>>(p->GetIndexImage());
  auto accNorm = std::make_shared<mitk::ImagePixelWriteAccessor<m2::NormImagePixelType, 3>>(p->GetNormalizationImage());

  std::vector<XAxisType> xs = p->GetXAxis();

  // ----- PreProcess -----

  // if the data are available as continuous data with equivalent mz axis for all
  // spectra, we can calculate the skyline, sum and mean spectrum over the image
  std::vector<std::vector<double>> skylineT;
  std::vector<std::vector<double>> sumT;

  p->SetPropertyValue<unsigned>("spectral depth", xs.size());
  p->SetPropertyValue<double>("xs_start", xs.front());
  p->SetPropertyValue<double>("xs_end", xs.back());

  skylineT.resize(p->GetNumberOfThreads(), std::vector<double>(xs.size(), 0));
  sumT.resize(p->GetNumberOfThreads(), std::vector<double>(xs.size(), 0));

  mitk::Timer t("Initialize image");

  m2::Process::Map(
    p->GetSpectra().size(), p->GetNumberOfThreads(), [&](unsigned int t, unsigned int a, unsigned int b) {
      const auto Smoother =
        m2::Signal::CreateSmoother<IntensityType>(p->GetSmoothingStrategy(), p->GetSmoothingHalfWindowSize(), false);

      auto BaselineSubstractor = m2::Signal::CreateSubstractBaselineConverter<IntensityType>(
        p->GetBaselineCorrectionStrategy(), p->GetBaseLineCorrectionHalfWindowSize());

      const auto Normalizor = m2::Signal::CreateNormalizor<XAxisType, IntensityType>(p->GetNormalizationStrategy());

      IntensityType val = 1;
      std::vector<IntensityType> ys(xs.size(), 0);
      std::vector<IntensityType> baseline(xs.size(), 0);

      const auto &spectra = p->GetSpectra();

      const auto divides = [&val](const auto &a) { return a / val; };
      const auto maximum = [](const auto &a, const auto &b) { return a > b ? a : b; };
      const auto plus = std::plus<>();

      for (unsigned long int i = a; i < b; i++)
      {
        const auto &spectrum = spectra[i];
        if (p->GetUseExternalMask())
        {
          auto v = accMask->GetPixelByIndex(spectrum.index);
          if (v == 0)
            continue;
        }

        std::copy(std::begin(spectra[i].data), std::end(spectra[i].data), std::begin(ys));

        if (p->GetUseExternalNormalization())
        {
          // Normalization-image content was set elsewhere
          val = accNorm->GetPixelByIndex(spectrum.index);
          std::transform(std::begin(ys), std::end(ys), std::begin(ys), divides);
        }
        else
        {
          accNorm->SetPixelByIndex(spectrum.index, 1);
          // val = Normalizor(xs, ys, accNorm->GetPixelByIndex(spectrum.index + source._offset));
          // accNorm->SetPixelByIndex(spectrum.index + source._offset, val); // Set normalization image pixel value
        }

        Smoother(ys);
        BaselineSubstractor(ys);

        std::transform(std::begin(ys), std::end(ys), sumT.at(t).begin(), sumT.at(t).begin(), plus);
        std::transform(std::begin(ys), std::end(ys), skylineT.at(t).begin(), skylineT.at(t).begin(), maximum);
      }
    });

  auto &skyline = p->SkylineSpectrum();
  skyline.resize(xs.size(), 0);
  for (unsigned int t = 0; t < p->GetNumberOfThreads(); ++t)
    std::transform(skylineT[t].begin(), skylineT[t].end(), skyline.begin(), skyline.begin(), [](auto &a, auto &b) {
      return a > b ? a : b;
    });

  auto &mean = p->MeanSpectrum();
  auto &sum = p->SumSpectrum();

  mean.resize(xs.size(), 0);
  sum.resize(xs.size(), 0);

  // accumulate valid spectra defined by mask image
  auto N = std::accumulate(accMask->GetData(),
                           accMask->GetData() + p->GetSpectra().size(),
                           mitk::LabelSetImage::PixelType(0),
                           [](const auto & a, const auto & b) -> mitk::LabelSetImage::PixelType { return a + (b > 0); });

  for (unsigned int t = 0; t < p->GetNumberOfThreads(); ++t)
    std::transform(sumT[t].begin(), sumT[t].end(), sum.begin(), sum.begin(), [](const auto &a, const auto &b) { return a + b; });
  std::transform(sum.begin(), sum.end(), mean.begin(), [&N](const auto &a) { return a / double(N); });

  //////////---------------------------

  const auto &spectra = p->GetSpectra();
  m2::Process::Map(spectra.size(), p->GetNumberOfThreads(), [&](unsigned int /*t*/, unsigned int a, unsigned int b) {
    for (unsigned int i = a; i < b; i++)
    {
      const auto &spectrum = spectra[i];

      accIndex->SetPixelByIndex(spectrum.index, i);
      if (p->GetUseExternalMask())
        accMask->SetPixelByIndex(spectrum.index, 1);
    }
  });
}

template <class XAxisType, class IntensityType>
void m2::FsmSpectrumImage::FsmProcessor<XAxisType, IntensityType>::GrabIntensityPrivate(unsigned long int index,
                                                                                        std::vector<double> &ys,
                                                                                        unsigned int) const
{
  mitk::ImagePixelReadAccessor<m2::NormImagePixelType, 3> normAcc(p->GetNormalizationImage());

  const auto &spectrum = p->GetSpectra()[index];
  // const auto kernel = m2::Signal::savitzkyGolayKernel(p->m_SmoothingHalfWindowSize, 3);

  ys.clear();
  std::copy(std::begin(spectrum.data), std::end(spectrum.data), std::back_inserter(ys));

  double d;
  auto divide = [&d](auto &val) { return val / d; };
  auto Smoother = m2::Signal::CreateSmoother<double>(p->GetSmoothingStrategy(), p->GetSmoothingHalfWindowSize(), false);
  auto BaselineSubstractor = m2::Signal::CreateSubstractBaselineConverter<double>(
    p->GetBaselineCorrectionStrategy(), p->GetBaseLineCorrectionHalfWindowSize());

  if (p->GetNormalizationStrategy() != m2::NormalizationStrategyType::None)
  {
    d = normAcc.GetPixelByIndex(spectrum.index);
    std::transform(ys.begin(), ys.end(), ys.begin(), divide);
  }

  Smoother(ys);
  BaselineSubstractor(ys);
}

template <class XAxisType, class IntensityType>
void m2::FsmSpectrumImage::FsmProcessor<XAxisType, IntensityType>::GrabMassPrivate(unsigned long int,
                                                                                   std::vector<double> &mzs,
                                                                                   unsigned int) const
{
  mzs.clear();
  std::copy(std::begin(p->GetXAxis()), std::end(p->GetXAxis()), std::back_inserter(mzs));
}

m2::FsmSpectrumImage::~FsmSpectrumImage()
{
  MITK_INFO << GetStaticNameOfClass() << " destroyed!";
}

m2::FsmSpectrumImage::FsmSpectrumImage()
{
  MITK_INFO << GetStaticNameOfClass() << " created!";
  this->SetPropertyValue<std::string>("x_label", "cm^-1");
  SetImportMode(m2::SpectrumFormatType::ContinuousProfile);
}
