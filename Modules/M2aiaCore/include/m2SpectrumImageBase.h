/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/
#pragma once

#include <M2aiaCoreExports.h>
#include <itkMetaDataObject.h>
#include <itkMultiThreaderBase.h>
#include <m2CoreCommon.h>
#include <m2ElxRegistrationHelper.h>
#include <m2ISpectrumDataAccess.h>
#include <m2IntervalVector.h>
#include <m2IonImageReference.h>
#include <m2SpectrumInfo.h>
#include <mitkBaseData.h>
#include <mitkImage.h>
#include <mitkImageStatisticsHolder.h>
#include <signal/m2SignalCommon.h>

namespace m2
{
  class M2AIACORE_EXPORT SpectrumImageBase : public ISpectrumDataAccess, public mitk::Image
  {
  public:
    using ImageArtifactMapType = std::map<std::string, mitk::BaseData::Pointer>;
    using SpectrumArtifactDataType = double;
    using SpectrumArtifactVectorType = std::vector<SpectrumArtifactDataType>;
    using SpectrumArtifactMapType = std::map<m2::SpectrumType, SpectrumArtifactVectorType>;
    using IonImageReferenceVectorType = std::vector<IonImageReference::Pointer>;
    using IntervalVectorType = std::vector<m2::Interval>;
    using TransformParameterVectorType = std::vector<std::string>;

    mitkClassMacro(SpectrumImageBase, mitk::Image);

    itkSetEnumMacro(NormalizationStrategy, NormalizationStrategyType);
    itkGetEnumMacro(NormalizationStrategy, NormalizationStrategyType);

    itkSetEnumMacro(IntensityTransformationStrategy, IntensityTransformationType);
    itkGetEnumMacro(IntensityTransformationStrategy, IntensityTransformationType);

    itkSetEnumMacro(RangePoolingStrategy, RangePoolingStrategyType);
    itkGetEnumMacro(RangePoolingStrategy, RangePoolingStrategyType);

    itkSetEnumMacro(SmoothingStrategy, SmoothingType);
    itkGetEnumMacro(SmoothingStrategy, SmoothingType);

    itkSetEnumMacro(BaselineCorrectionStrategy, BaselineCorrectionType);
    itkGetEnumMacro(BaselineCorrectionStrategy, BaselineCorrectionType);

    virtual void GetXValues(unsigned int /*id*/, std::vector<double> & /*xs*/, unsigned int /*source*/ = 0)
    {
      MITK_WARN(GetStaticNameOfClass()) << "GetXValues[double] is not implemented!";
    }

    virtual void GetXValues(unsigned int /*id*/, std::vector<float> & /*xs*/, unsigned int /*source*/ = 0)
    {
      MITK_WARN(GetStaticNameOfClass()) << "GetXValues[float] is not implemented!";
    }

    virtual void GetYValues(unsigned int /*id*/, std::vector<double> & /*ys*/, unsigned int /*source*/ = 0)
    {
      MITK_WARN(GetStaticNameOfClass()) << "GetYValues[double] is not implemented!";
    }
    virtual void GetYValues(unsigned int /*id*/, std::vector<float> & /*ys*/, unsigned int /*source*/ = 0)
    {
      MITK_WARN(GetStaticNameOfClass()) << "GetYValues[float] is not implemented!";
    }

    itkSetMacro(BaseLineCorrectionHalfWindowSize, unsigned int);
    itkGetConstReferenceMacro(BaseLineCorrectionHalfWindowSize, unsigned int);

    itkSetMacro(SmoothingHalfWindowSize, unsigned int);
    itkGetConstReferenceMacro(SmoothingHalfWindowSize, unsigned int);

    itkSetMacro(BinningTolerance, double);
    itkGetConstReferenceMacro(BinningTolerance, double);

    itkSetMacro(NumberOfBins, int);
    itkGetConstReferenceMacro(NumberOfBins, int);

    itkSetMacro(Tolerance, double);
    itkGetConstReferenceMacro(Tolerance, double);

    itkSetMacro(UseToleranceInPPM, bool);
    itkGetConstReferenceMacro(UseToleranceInPPM, bool);

    itkGetMacro(Intervals, IntervalVectorType &);
    itkGetConstReferenceMacro(Intervals, IntervalVectorType);

    itkGetConstReferenceMacro(NumberOfThreads, unsigned int);
    itkSetMacro(NumberOfThreads, unsigned int);

    itkGetConstReferenceMacro(NumberOfValidPixels, unsigned int);

    itkGetMacro(ImageArtifacts, ImageArtifactMapType &);
    itkGetConstReferenceMacro(ImageArtifacts, ImageArtifactMapType);

    itkGetMacro(SpectraArtifacts, SpectrumArtifactMapType &);
    itkGetConstReferenceMacro(SpectraArtifacts, SpectrumArtifactMapType);

    itkGetMacro(IonImageReferenceVector, IonImageReferenceVectorType &);
    itkGetConstReferenceMacro(IonImageReferenceVector, IonImageReferenceVectorType);

    itkGetObjectMacro(CurrentIonImageReference, IonImageReference);
    itkGetConstObjectMacro(CurrentIonImageReference, IonImageReference);
    itkSetObjectMacro(CurrentIonImageReference, IonImageReference);

    mitk::Image::Pointer GetNormalizationImage();
    mitk::Image::Pointer GetMaskImage();
    mitk::Image::Pointer GetIndexImage();
    mitk::Image::Pointer GetNormalizationImage() const;
    mitk::Image::Pointer GetMaskImage() const;
    mitk::Image::Pointer GetIndexImage() const;

    itkGetConstMacro(UseExternalMask, bool);
    itkSetMacro(UseExternalMask, bool);
    itkBooleanMacro(UseExternalMask);

    itkGetConstMacro(UseExternalIndices, bool);
    itkSetMacro(UseExternalIndices, bool);
    itkBooleanMacro(UseExternalIndices);

    itkGetConstMacro(UseExternalNormalization, bool);
    itkSetMacro(UseExternalNormalization, bool);
    itkBooleanMacro(UseExternalNormalization);

    itkGetConstMacro(IsDataAccessInitialized, bool);

    virtual void InitializeImageAccess() = 0;
    virtual void InitializeGeometry() = 0;
    virtual void InitializeProcessor() = 0;

    SpectrumArtifactVectorType &SkylineSpectrum();
    SpectrumArtifactVectorType &SumSpectrum();
    SpectrumArtifactVectorType &MeanSpectrum();
    SpectrumArtifactVectorType &GetXAxis();
    const SpectrumArtifactVectorType &GetXAxis() const;

    void GetImage(double mz, double tol, const mitk::Image *mask, mitk::Image *img) const override;
    void InsertImageArtifact(const std::string &key, mitk::Image *img);

    template <class T>
    void SetPropertyValue(const std::string &key, const T &value);

    template <class T>
    const T GetPropertyValue(const std::string &key, T def = T()) const;

    void ApplyGeometryOperation(mitk::Operation *);
    void ApplyMoveOriginOperation(const mitk::Vector3D &v);

    inline void SaveModeOn() const { this->m_InSaveMode = true; }
    inline void SaveModeOff() const { this->m_InSaveMode = false; }
    double ApplyTolerance(double xValue) const;

    void SetElxRegistrationHelper(const std::shared_ptr<m2::ElxRegistrationHelper> &d) { m_ElxRegistrationHelper = d; }

    const SpectrumInfo &GetSpectrumType() const { return m_SpectrumType; }
    SpectrumInfo &GetSpectrumType() { return m_SpectrumType; }
    void SetSpectrumType(const SpectrumInfo &other) { m_SpectrumType = other; }

    SpectrumInfo &GetExportSpectrumType() { return m_ExportSpectrumType; }
    const SpectrumInfo &GetExportSpectrumType() const { return m_ExportSpectrumType; }
    void SetExportSpectrumType(const SpectrumInfo &other) { m_ExportSpectrumType = other; }

    virtual void PeakListModified();

    /**
     * @brief Get the Intensity Data representing a matrix of shape [#intervals, #pixels] as a contiguous array of floats.
     * 
     * @param intervals: list of intervals (e.g. a peak center). 
     * @return std::vector<float> 
     */
    virtual std::vector<float> GetIntensityData(const std::vector<m2::Interval> & intervals) const;

    /**
     * @brief Get the shape of the assumed data matrix generated with GetIntensityData(...)
     * 
     * @param intervals: list of intervals (e.g. a peak center). Same as used in GetIntensityData(...).
     * @return std::vector<unsigned long> 
     */
    virtual std::vector<unsigned long> GetIntensityDataShape(const std::vector<m2::Interval> & intervals)const;

  protected:
    bool mutable m_InSaveMode = false;
    double m_Tolerance = 10;
    double m_BinningTolerance = 50;
    int m_NumberOfBins = 2000;

    bool m_UseExternalMask = false;
    bool m_UseExternalIndices = false;
    bool m_UseExternalNormalization = false;
    bool m_UseToleranceInPPM = true;
    bool m_IsDataAccessInitialized = false;

    std::shared_ptr<m2::ElxRegistrationHelper> m_ElxRegistrationHelper;

    unsigned int m_NumberOfValidPixels = 0;
    unsigned int m_BaseLineCorrectionHalfWindowSize = 100;
    unsigned int m_SmoothingHalfWindowSize = 4;
    unsigned int m_NumberOfThreads = itk::MultiThreaderBase::GetGlobalMaximumNumberOfThreads();
    // unsigned int m_NumberOfThreads = 2;

    // 
    IntervalVectorType m_Intervals;

    ImageArtifactMapType m_ImageArtifacts;
    SpectrumArtifactMapType m_SpectraArtifacts;

    BaselineCorrectionType m_BaselineCorrectionStrategy = m2::BaselineCorrectionType::None;
    SmoothingType m_SmoothingStrategy = m2::SmoothingType::None;
    IntensityTransformationType m_IntensityTransformationStrategy = m2::IntensityTransformationType::None;

    SpectrumInfo m_SpectrumType;
    SpectrumInfo m_ExportSpectrumType;

    /**
     * @brief Vector of ion images associated with this ims file. E.g. peaks or individual picked ion
     * images.
     */
    IonImageReferenceVectorType m_IonImageReferenceVector;

    /**
     * @brief Information about the represented ion image distribution captured by this image instance.
     * This property is updated by UpdateImage.
     *
     */
    IonImageReference::Pointer m_CurrentIonImageReference;

    NormalizationStrategyType m_NormalizationStrategy = NormalizationStrategyType::TIC;
    RangePoolingStrategyType m_RangePoolingStrategy = RangePoolingStrategyType::Sum;

    SpectrumImageBase();
    ~SpectrumImageBase() override;

    SpectrumArtifactVectorType m_XAxis;
  };

  itkEventMacroDeclaration(PeakListModifiedEvent, itk::AnyEvent);
  itkEventMacroDeclaration(InitializationFinishedEvent, itk::AnyEvent);

} // namespace m2

template <class T>
inline void m2::SpectrumImageBase::SetPropertyValue(const std::string &key, const T &value)
{
  auto dd = this->GetPropertyList();
  auto prop = dd->GetProperty(key);
  using TargetProperty = mitk::GenericProperty<T>;

  auto entry = dynamic_cast<TargetProperty *>(prop);
  if (!entry)
  {
    auto entry = TargetProperty::New(value);
    dd->SetProperty(key, entry);
  }
  else
  {
    entry->SetValue(value);
  }
}

template <class T>
inline const T m2::SpectrumImageBase::GetPropertyValue(const std::string &key, T def) const
{
  auto dd = this->GetPropertyList();
  const mitk::GenericProperty<T> *entry = dynamic_cast<mitk::GenericProperty<T> *>(dd->GetProperty(key));
  if (entry)
  {
    return entry->GetValue();
  }
  else
  {
    MITK_WARN << "No meta data object found! " << key;
    return def;
  }
}
