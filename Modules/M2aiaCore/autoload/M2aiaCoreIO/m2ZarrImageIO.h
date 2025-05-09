/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/
#pragma once


#include <M2aiaCoreIOExports.h>
#include <mitkAbstractFileIO.h>
#include <mitkIOMimeTypes.h>
#include <mitkImage.h>
#include <mitkItkImageIO.h>

namespace m2
{
  class M2AIACOREIO_EXPORT ZarrImageIO : public mitk::AbstractFileIO
  {
  public:
    ZarrImageIO();

    std::string Zarr_MIMETYPE_NAME()
    {
      static std::string name = mitk::IOMimeTypes::DEFAULT_BASE_NAME() + ".image.zarr";
      return name;
    }

    mitk::CustomMimeType Zarr_MIMETYPE()
    {
      mitk::CustomMimeType mimeType(Zarr_MIMETYPE_NAME());
	    mimeType.AddExtension("zarr");
      mimeType.AddExtension("zgroup");
      mimeType.AddExtension("zarray");
	    mimeType.SetCategory("Zarr Image");
      mimeType.SetComment("Image in Zarr format");
      return mimeType;
    }

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    ConfidenceLevel GetReaderConfidenceLevel() const override;
    void Write() override;

    ConfidenceLevel GetWriterConfidenceLevel() const override;
    void LoadAssociatedData(mitk::BaseData *object);

  private:
    ZarrImageIO *IOClone() const override;
    
  };
} // namespace m2
