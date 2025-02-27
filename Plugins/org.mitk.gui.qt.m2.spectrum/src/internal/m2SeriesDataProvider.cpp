/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/m2aia/m2aia for details.

===================================================================*/

#include "m2SeriesDataProvider.h"

#include <QColor>
#include <QHash>
#include <QLineSeries>
#include <QRandomGenerator>
#include <QScatterSeries>
#include <QVariant>
#include <QXYSeries>
#include <m2IntervalVector.h>
#include <mitkCoreServices.h>
#include <mitkExceptionMacro.h>
#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <signal/m2Binning.h>

m2::SeriesDataProvider::SeriesDataProvider()
  : m_DefaultColor(QColor::fromHsv(QRandomGenerator::global()->generateDouble() * 255, 255, 255))
{
}

void m2::SeriesDataProvider::SetData(const m2::IntervalVector *data, m2::SpectrumFormat format)
{
  if (format == m2::SpectrumFormat::None)
    m_Format = data->GetType();
  else
    m_Format = format;
  m_IntervalVector = data;

  
}

m2::SeriesDataProvider::PointsVector m2::SeriesDataProvider::ConvertToQVector(const std::vector<double> &xs,
                                                                              const std::vector<double> &ys)
{
  PointsVector data;
  data.reserve(xs.size());
  auto inserter = std::back_inserter(data);
  std::transform(
    xs.begin(), xs.end(), ys.begin(), inserter, [](const auto &x, const auto &y) { return QPointF(x, y); });
  return data;
}

void m2::SeriesDataProvider::InitializeSeries()
{
  if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid))
  {
    m_Series = new QLineSeries();
    SetProfileSpectrumDefaultStyle(m_Series);
    m_Series->setPointsVisible(true);
  }
  else if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Profile))
  {
    m_Series = new QLineSeries();
    SetProfileSpectrumDefaultStyle(m_Series);
    m_Series->setPointsVisible(false);
  }
}

void m2::SeriesDataProvider::InitializeLoDData() {
  if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid))
  {
    MITK_INFO << "Centroid " << (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid));
    m_DataLoD.clear();
    PointsVector target;
    m_xs = m_IntervalVector->GetXMean();
    m_ys = m_IntervalVector->GetYMax(); // Critical?
    if(m_ys.empty()) return;
    
    // auto min = *std::min_element(m_ys.begin(), m_ys.end());
    // auto max = *std::max_element(m_ys.begin(), m_ys.end());

    // std::transform(std::begin(m_ys), std::end(m_ys), std::begin(m_ys), [min, max](const auto &y) {
    //   return (y - min) / (max - min);
    // });
    
    using namespace std;
    transform(begin(m_xs),
              end(m_xs),
              begin(m_ys),
              back_inserter(target),
              [&](auto a, auto b)
              {
                return QPointF(a, b);
              });
    m_DataLoD.push_back(target);
  }
  else
  {
    MITK_INFO << "Profile = " << (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Profile));
    m_DataLoD.clear();
    m_xs = m_IntervalVector->GetXMean();
    m_ys = m_IntervalVector->GetYMean(); // Critical?
    if(m_ys.empty()) return;
    
    // auto min = *std::min_element(m_ys.begin(), m_ys.end());
    // auto max = *std::max_element(m_ys.begin(), m_ys.end());
    
    // std::transform(std::begin(m_ys), std::end(m_ys), std::begin(m_ys), [min, max](const auto &y) {
    //   return (y - min) / (max - min);
    // });
    
    for (unsigned int level : m_Levels)
      m_DataLoD.push_back(GenerateLoDData(m_xs, m_ys, level));

    
  }
}

void m2::SeriesDataProvider::GenerateSeriesDataWithinRange(double x1, double x2)
{
  using namespace std;

  if (m_xs.empty())
    return;

  if (x1 < 0 && x2 < 0)
  {
    x1 = m_xs.front();
    x2 = m_xs.back();
  }

  auto level = FindLoD(x1, x2); // always 0 for centroid data
  MITK_INFO << "Level: " << level;
  const auto &currentData = m_DataLoD[level];
  auto lower = std::lower_bound(std::begin(currentData),
                                std::end(currentData),
                                QPointF{x1 - 1, 0},
                                [](const auto &a, const auto &b) { return a.x() < b.x(); });
  const auto upper = std::upper_bound(
    lower, std::end(currentData), QPointF{x2 + 1, 0}, [](const auto &a, const auto &b) { return a.x() < b.x(); });

  QVector<QPointF> seriesData;

  if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid))
    seriesData.reserve(std::distance(lower, upper) * 3);
  else
    seriesData.reserve(std::distance(lower, upper));

  auto insert = std::back_inserter(seriesData);

  for (; lower != upper; ++lower)
  {
    if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid))
    {
      insert = QPointF{lower->x(), -0.001};
      insert = QPointF{lower->x(), lower->y()};
      insert = QPointF{lower->x(), -0.001};
    }
    else
    {
      insert = QPointF{lower->x(), lower->y()};
    }
  }
  MITK_INFO << "Replaced";
  MITK_INFO << m_Series->points().size();
  m_Series->replace(seriesData);
}

m2::SeriesDataProvider::PointsVector m2::SeriesDataProvider::GenerateLoDData(std::vector<double> &xs,
                                                                             std::vector<double> &ys,
                                                                             unsigned int level)
{
  if (xs.size() != ys.size())
    mitkThrow() << "Generate LODData faild: no equal xs size and ys size";

  const auto numBins = xs.size() / level;

  std::vector<m2::Interval> binnedData;
  m2::Signal::equidistantBinning(std::begin(xs), std::end(xs), std::begin(ys), std::back_inserter(binnedData), numBins);

  PointsVector target;
  target.reserve(binnedData.size());

  std::transform(std::begin(binnedData),
                 std::end(binnedData),
                 std::back_inserter(target),
                 [](const m2::Interval &p) { return QPointF{p.x.mean(), p.y.max()}; });
  return target;
}

void m2::SeriesDataProvider::SetProfileSpectrumDefaultStyle(QXYSeries *series)
{
  auto p = series->pen();
  p.setWidthF(.75);
  p.setCapStyle(Qt::FlatCap);
  series->setPen(p);
}

void m2::SeriesDataProvider::SetCentroidSpectrumDefaultMarkerStyle(QXYSeries *series)
{
  if (auto scatterSeries = dynamic_cast<QScatterSeries *>(series))
  {
    scatterSeries->setMarkerShape(QScatterSeries::MarkerShape::MarkerShapeRectangle);
    scatterSeries->setMarkerSize(3);
  }
}

void m2::SeriesDataProvider::SetMarkerSpectrumDefaultMarkerStyle(QXYSeries *series)
{
  if (auto scatterSeries = dynamic_cast<QScatterSeries *>(series))
  {
    scatterSeries->setMarkerShape(QScatterSeries::MarkerShape::MarkerShapeCircle);
    scatterSeries->setMarkerSize(3);
    scatterSeries->setColor(Qt::GlobalColor::red);
  }
}

int m2::SeriesDataProvider::FindLoD(double xMin, double xMax) const
{
  if (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid)){
    MITK_INFO << "Centroid " << (to_underlying(m_Format) & to_underlying(m2::SpectrumFormat::Centroid));
    return 0;
  }

  auto *preferencesService = mitk::CoreServices::GetPreferencesService();
  auto *preferences = preferencesService->GetSystemPreferences();
  auto pointsWanted = preferences->GetInt("m2aia.view.spectrum.bins", 15000);

  MITK_INFO << "Points wanted: " << pointsWanted;

  int level, levelIndex = 0;
  const auto wantedDensity = pointsWanted / double(xMax - xMin);
  double delta = std::numeric_limits<double>::max();
  for (const auto &dataLodVector : m_DataLoD)
  {
    const auto lodDensity = dataLodVector.size() / double(dataLodVector.back().x() - dataLodVector.front().x());

    if (std::abs(wantedDensity - lodDensity) < delta)
    {
      level = levelIndex;
      delta = std::abs(wantedDensity - lodDensity);
    }
    ++levelIndex;
  }
  return level;
}

void m2::SeriesDataProvider::SetColor(QColor c)
{
  m_DefaultColor = c;
  if (m_Series)
  {
    m_Series->setColor(c);
  }
}

void m2::SeriesDataProvider::SetColor(qreal r, qreal g, qreal b, qreal a = 1.0)
{
  QColor c;
  c.setRgbF(r, g, b, a);
  SetColor(c);
}