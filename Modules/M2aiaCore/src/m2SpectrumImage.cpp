/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/
#include <m2SpectrumImage.h>
#include <mitkDataNode.h>
#include <mitkImagePixelReadAccessor.h>
#include <mitkLevelWindowProperty.h>
#include <mitkLookupTableProperty.h>
#include <mitkOperation.h>
#include <signal/m2PeakDetection.h>

namespace m2
{
  itkEventMacroDefinition(InitializationFinishedEvent, itk::AnyEvent);
} // namespace m2

double m2::SpectrumImage::ApplyTolerance(double xValue) const
{
  if (this->GetUseToleranceInPPM())
    return m2::PartPerMillionToFactor(this->GetTolerance()) * xValue;
  else
    return this->GetTolerance();
}

mitk::Image::Pointer m2::SpectrumImage::GetNormalizationImage()
{
    return GetNormalizationImage(m_NormalizationStrategy);
}

mitk::Image::Pointer m2::SpectrumImage::GetNormalizationImage(m2::NormalizationStrategyType type)
{
  if (m_NormalizationImages.find(type) != m_NormalizationImages.end()){
      return m_NormalizationImages.at(type).image;
  }
  return nullptr;
}

bool m2::SpectrumImage::GetNormalizationImageStatus(m2::NormalizationStrategyType type)
{
  return m_NormalizationImages[type].isInitialized;
};

void m2::SpectrumImage::SetNormalizationImageStatus(m2::NormalizationStrategyType type, bool initialized){
  m_NormalizationImages[type].isInitialized = initialized;
}

void m2::SpectrumImage::SetNormalizationImage(mitk::Image::Pointer image, m2::NormalizationStrategyType type)
{
  m_NormalizationImages[type].image = image;
}


std::vector<double> &m2::SpectrumImage::GetSkylineSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Maximum)];
}

std::vector<double> &m2::SpectrumImage::GetMeanSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Mean)];
}

std::vector<double> &m2::SpectrumImage::GetSumSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Sum)];
}

std::vector<double> &m2::SpectrumImage::GetXAxis()
{
  return m_XAxis;
}

const std::vector<double> &m2::SpectrumImage::GetXAxis() const
{
  return m_XAxis;
}

void m2::SpectrumImage::GetImage(double, double, const mitk::Image *, mitk::Image *) const
{
  MITK_WARN("SpectrumImage") << "Get image is not implemented in derived class!";
}

m2::SpectrumImage::~SpectrumImage() {}
m2::SpectrumImage::SpectrumImage() : mitk::Image() {}
