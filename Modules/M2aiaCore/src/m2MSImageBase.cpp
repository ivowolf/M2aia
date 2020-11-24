/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/
#include <m2MSImageBase.h>
#include <m2NoiseEstimators.hpp>
#include <m2PeakDetection.hpp>
#include <mitkDataNode.h>
#include <mitkLevelWindowProperty.h>
#include <mitkLookupTableProperty.h>
#include <mitkOperation.h>

void m2::MSImageBase::ApplyMoveOriginOperation(const std::array<int, 2> &v)
{
  auto geometry = this->GetGeometry();
  auto pos = geometry->GetOrigin();
  auto space = geometry->GetSpacing();
  pos[0] = pos[0] + v.at(0) * space[0];
  pos[1] = pos[1] + v.at(1) * space[1];
  geometry->SetOrigin(pos);

  for (auto kv : m_ImageArtifacts)
  {
    geometry = kv.second->GetGeometry();
	pos = geometry->GetOrigin();
    space = geometry->GetSpacing();
    pos[0] = pos[0] + v.at(0) * space[0];
    pos[1] = pos[1] + v.at(1) * space[1];
    geometry->SetOrigin(pos);
  }
}

void m2::MSImageBase::ApplyGeometryOperation(mitk::Operation *op)
{
  auto manipulatedGeometry = this->GetGeometry()->Clone();
  manipulatedGeometry->ExecuteOperation(op);
  this->GetGeometry()->SetIdentity();
  this->GetGeometry()->Compose(manipulatedGeometry->GetIndexToWorldTransform());

  for (auto kv : m_ImageArtifacts)
  {
    auto manipulatedGeometry = kv.second->GetGeometry()->Clone();
    manipulatedGeometry->ExecuteOperation(op);
    kv.second->GetGeometry()->SetIdentity();
    kv.second->GetGeometry()->Compose(manipulatedGeometry->GetIndexToWorldTransform());
  }
}


m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::SkylineSpectrum()
{
  return m_SpectraArtifacts[m2::OverviewSpectrumType::Maximum];
}

m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::PeakIndicators()
{
  return m_SpectraArtifacts[m2::OverviewSpectrumType::PeakIndicators];
}

m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::MeanSpectrum()
{
  return m_SpectraArtifacts[m2::OverviewSpectrumType::Mean];
}

m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::SumSpectrum()
{
  return m_SpectraArtifacts[m2::OverviewSpectrumType::Sum];
}

m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::MassAxis()
{
  return m_MassAxis;
}

const m2::MSImageBase::SpectrumArtifactVectorType &m2::MSImageBase::MassAxis() const
{
  return m_MassAxis;
}

mitk::Image::Pointer m2::MSImageBase::GetNormalizationImage()
{
  if (m_ImageArtifacts.find("NormalizationImage") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image*>(m_ImageArtifacts.at("NormalizationImage").GetPointer());
  return nullptr;
}

mitk::Image::Pointer m2::MSImageBase::GetMaskImage()
{
  if (m_ImageArtifacts.find("mask") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image*>(m_ImageArtifacts.at("mask").GetPointer());
  return nullptr;
}

mitk::Image::Pointer m2::MSImageBase::GetIndexImage()
{
  if (m_ImageArtifacts.find("index") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image*>(m_ImageArtifacts.at("index").GetPointer());
  return nullptr;
}

void m2::MSImageBase::GrabIonImage(double mz, double tol, const mitk::Image *mask, mitk::Image *img) const
{
  GrabImageStart.Send();
  m_Processor->GrabIonImagePrivate(mz, tol, mask, img);
  GrabImageEnd.Send();
}

void m2::MSImageBase::GrabIntensity(unsigned int index, std::vector<double> &ints, unsigned int sourceIndex) const
{
  GrabSpectrumStart.Send();
  m_Processor->GrabIntensityPrivate(index, ints, sourceIndex);
  GrabSpectrumEnd.Send();
}

void m2::MSImageBase::GrabMass(unsigned int index, std::vector<double> &mzs, unsigned int sourceIndex) const
{
  GrabSpectrumStart.Send();
  m_Processor->GrabMassPrivate(index, mzs, sourceIndex);
  GrabSpectrumEnd.Send();
}

void m2::MSImageBase::GrabSpectrum(unsigned int index,
                                         std::vector<double> &mzs,
                                         std::vector<double> &ints,
                                         unsigned int sourceIndex) const
{
  using namespace m2;
  GrabSpectrumStart.Send();
  // Grab spectrum intensities and masses
  m_Processor->GrabMassPrivate(index, mzs, sourceIndex);

  // ints may be a preprocessed version of the raw intensities
  m_Processor->GrabIntensityPrivate(index, ints, sourceIndex);

  GrabSpectrumEnd.Send();
}

m2::MSImageBase::~MSImageBase() {}
