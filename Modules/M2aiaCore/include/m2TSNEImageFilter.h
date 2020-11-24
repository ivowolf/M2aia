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
#include <MitkM2aiaCoreExports.h>

#include <mitkImageSource.h>
#include <mitkImageToImageFilter.h>
#include <m2MassSpecVisualizationFilter.h>

namespace m2
{
  class MITKM2AIACORE_EXPORT TSNEImageFilter : public m2::MassSpecVisualizationFilter
  {
  public:
    mitkClassMacro(TSNEImageFilter, MassSpecVisualizationFilter);
    itkFactorylessNewMacro(Self);
    itkCloneMacro(Self);

    void SetNumberOfOutputDimensions(unsigned int v);
    itkGetMacro(NumberOfOutputDimensions, unsigned int);

    itkGetMacro(Perplexity, unsigned int);
    itkSetMacro(Perplexity, unsigned int);

    itkGetMacro(Iterations, unsigned int);
    itkSetMacro(Iterations, unsigned int);

  protected:
    using RGBPixel = itk::RGBPixel<unsigned char>;
    itk::DataObject::Pointer MakeOutput(const DataObjectIdentifierType &name);

    itk::DataObject::Pointer MakeOutput(DataObjectPointerArraySizeType idx);

    unsigned int m_NumberOfOutputDimensions = 3;
    unsigned int m_NumberOfInputDimensions = 0;
    unsigned int m_NumberOfValidPixels = 0;
    unsigned int m_Perplexity = 2;
    unsigned int m_Iterations = 200;

    /*!
    \brief standard constructor
    */
    TSNEImageFilter();
    /*!
    \brief standard destructor
    */
    ~TSNEImageFilter () override;
    /*!
    \brief Method generating the output information of this filter (e.g. image dimension, image type, etc.).
    The interface ImageToImageFilter requires this implementation. Everything is taken from the input data.
    */
    // void GenerateOutputInformation() override;
    /*!
    \brief Method generating the output of this filter. Called in the updated process of the pipeline.
    This method generates the PCA output image.
    */
    void GenerateData() override;
  };

} // namespace mitk
