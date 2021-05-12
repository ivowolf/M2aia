/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

#pragma once

#include <M2aiaCoreIOExports.h>
#include <m2ImzMLSpectrumImage.h>
#include <mitkAbstractFileIO.h>
#include <mitkIOMimeTypes.h>
#include <mitkImage.h>
#include <mitkItkImageIO.h>

namespace m2
{
  class M2AIACOREIO_EXPORT OpenSlideIO : public mitk::AbstractFileIO
  {
  public:
    OpenSlideIO();

    std::string OPENSLIDE_MIMETYPE_NAME()
    {
      static std::string name = mitk::IOMimeTypes::DEFAULT_BASE_NAME() + ".image.openslide";
      return name;
    }

    mitk::CustomMimeType OPENSLIDE_MIMETYPE()
    {
      mitk::CustomMimeType mimeType(OPENSLIDE_MIMETYPE_NAME());
	  // Aperio
      mimeType.AddExtension("svs");
	  // Sakura
      mimeType.AddExtension("svslide");
	  // Leica
      mimeType.AddExtension("scn");
      // Hamamatsu
      mimeType.AddExtension("vms");
      mimeType.AddExtension("vmu");
      mimeType.AddExtension("ndpi");
	  // Aperio, Ventana, Generic tiled TIFF
      mimeType.AddExtension("tif");
	  // Ventana
      mimeType.AddExtension("bif");
	  // Philips
      mimeType.AddExtension("tiff");
      
	  mimeType.SetCategory("OpenSlide");
      mimeType.SetComment("OpenSlide");
      return mimeType;
    }

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    ConfidenceLevel GetReaderConfidenceLevel() const override;
    void Write() override;

    ConfidenceLevel GetWriterConfidenceLevel() const override;

  private:
    OpenSlideIO *IOClone() const override;
  };
} // namespace m2
