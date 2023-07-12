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
#include <vector>

namespace mitk
{
  class Image;
}

namespace m2
{
  class M2AIACORE_EXPORT ISpectrumDataAccess
  {
  public:
    virtual void GetSpectrumFloat(unsigned int index,
                             std::vector<float> &xs,
                             std::vector<float> &ys,
                             unsigned int sourceIndex = 0) const = 0;

    virtual void GetSpectrum(unsigned int index,
                             std::vector<double> &xs,
                             std::vector<double> &ys,
                             unsigned int sourceIndex = 0) const = 0;

    virtual void GetIntensitiesFloat(unsigned int id,
                     std::vector<float> &xs,
                     unsigned int source = 0) const = 0;

    virtual void GetIntensities(unsigned int id,
                     std::vector<double> &xs,
                     unsigned int source = 0) const = 0;

    virtual void GetImage(double mz, double tol, const mitk::Image *mask, mitk::Image *img) const = 0;
  };

} // namespace m2