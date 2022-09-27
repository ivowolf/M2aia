/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>
#include <m2ElxUtil.h>
// Qmitk
#include "m2PeakPickingView.h"

// Qt
#include <QFileDialog>
#include <QMessageBox>

// m2
#include <m2CoreCommon.h>
#include <m2ImzMLSpectrumImage.h>
#include <m2IonImageReference.h>
#include <m2MultiSliceFilter.h>
#include <m2PcaImageFilter.h>
#include <m2TSNEImageFilter.h>
#include <m2UIUtils.h>
#include <signal/m2MedianAbsoluteDeviation.h>
#include <signal/m2PeakDetection.h>

// itk
#include <itkExtractImageFilter.h>
#include <itkFixedArray.h>
#include <itkIdentityTransform.h>
#include <itkImageAlgorithm.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkShrinkImageFilter.h>
#include <itkVectorImageToImageAdaptor.h>

// mitk image
#include <mitkImage.h>
#include <mitkImageAccessByItk.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkProperties.h>

const std::string m2PeakPickingView::VIEW_ID = "org.mitk.views.m2.PeakPicking";

void m2PeakPickingView::SetFocus()
{
  // OnStartPeakPicking();
}

void m2PeakPickingView::CreateQtPartControl(QWidget *parent)
{
  // create GUI widgets from the Qt Designer's .ui file
  m_Controls.setupUi(parent);
  m_Controls.cbOverviewSpectra->addItems({"Skyline", "Mean", "Sum"});
  m_Controls.cbOverviewSpectra->setCurrentIndex(1);

  m_Controls.sliderCOR->setMinimum(0.0);
  m_Controls.sliderCOR->setMaximum(1.0);
  m_Controls.sliderCOR->setValue(0.95);
  m_Controls.sliderCOR->setSingleStep(0.01);

  m_Controls.nodeSelection->SetDataStorage(GetDataStorage());
  m_Controls.nodeSelection->SetAutoSelectNewNodes(true);
  m_Controls.nodeSelection->SetNodePredicate(
    mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<m2::SpectrumImageBase>::New(),
                                mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
  m_Controls.nodeSelection->SetSelectionIsOptional(true);
  m_Controls.nodeSelection->SetEmptyInfo(QString("Image selection"));
  m_Controls.nodeSelection->SetPopUpTitel(QString("Image"));

  // // if (auto button = m_Controls.tableWidget->findChild<QAbstractButton *>(QString(), Qt::FindDirectChildrenOnly))
  // // {
  // // this button is not a normal button, it doesn't paint text or icon
  // // so it is not easy to show text on it, the simplest way is tooltip
  // // button->setToolTip("Select/deselect all");

  // // disconnect the connected slots to the tableview (the "selectAll" slot)
  // // disconnect(button, Q_NULLPTR, m_Controls.tableWidget, Q_NULLPTR);
  // // connect "clear" slot to it, here I use QTableWidget's clear, you can connect your own
  auto tableWidget = m_Controls.tableWidget;
  
  connect(m_Controls.btnSelectAll,
          &QAbstractButton::clicked,
          m_Controls.tableWidget,
          [tableWidget]() mutable
          {
            for (int i = 0; i < tableWidget->rowCount(); ++i)
              tableWidget->item(i, 0)->setCheckState(Qt::CheckState::Checked);
          });
  
  connect(m_Controls.btnDeselectAll,
          &QAbstractButton::clicked,
          m_Controls.tableWidget,
          [tableWidget]() mutable
          {
            for (int i = 0; i < tableWidget->rowCount(); ++i)
              tableWidget->item(i, 0)->setCheckState(Qt::CheckState::Unchecked);
          });

  const auto itemHandler = [this](QTableWidgetItem *item)
  {
    auto node = m_Controls.nodeSelection->GetSelectedNode();
    if (auto spImage = dynamic_cast<m2::SpectrumImageBase *>(node->GetData()))
    {
      auto mz = std::stod(item->text().toStdString());
      emit m2::UIUtils::Instance()->UpdateImage(mz, spImage->ApplyTolerance(mz));
    }
  };

  connect(m_Controls.tableWidget, &QTableWidget::itemActivated, this, itemHandler);
  connect(m_Controls.tableWidget, &QTableWidget::itemDoubleClicked, this, itemHandler);
  connect(m_Controls.btnStartPeakPicking, &QCommandLinkButton::clicked, this, &m2PeakPickingView::OnStartPeakPicking);
  connect(m_Controls.btnPCA, &QCommandLinkButton::clicked, this, &m2PeakPickingView::OnStartPCA);
  connect(m_Controls.btnTSNE, &QCommandLinkButton::clicked, this, &m2PeakPickingView::OnStartTSNE);
  connect(m_Controls.sliderSNR, &ctkSliderWidget::valueChanged, this, &m2PeakPickingView::OnStartPeakPicking);
  connect(m_Controls.sliderHWS, &ctkSliderWidget::valueChanged, this, &m2PeakPickingView::OnStartPeakPicking);
  connect(m_Controls.sliderCOR, &ctkSliderWidget::valueChanged, this, &m2PeakPickingView::OnStartPeakPicking);
  connect(m_Controls.nodeSelection, &QmitkSingleNodeSelectionWidget::CurrentSelectionChanged, this, &m2PeakPickingView::OnStartPeakPicking);
}

void m2PeakPickingView::OnStartPeakPicking()
{
  // if (m_Connection)
  //   disconnect(m_Connection);
  m_PeakList.clear();

  if (auto node = m_Controls.nodeSelection->GetSelectedNode())
  {
    if (auto imageBase = dynamic_cast<m2::SpectrumImageBase *>(node->GetData()))
    {
      if(!imageBase->GetIsDataAccessInitialized()) return;
      if (imageBase->GetSpectrumType().Format == m2::SpectrumFormat::ContinuousCentroid || 
          imageBase->GetSpectrumType().Format == m2::SpectrumFormat::ProcessedCentroid)
      {
        // MITK_INFO << "Processed/ContinuousCentroid";
        auto &peakList = imageBase->GetPeaks();
        peakList.clear();
        unsigned i = 0;
        for (auto &x : imageBase->GetXAxis())
          peakList.emplace_back(i++, x, 0);
        m_PeakList = peakList;
      }
      else
      {
        // MITK_INFO << "ContinuousProfile";
        std::vector<double> ys, xs;
        if (m_Controls.cbOverviewSpectra->currentIndex() == 0) // skyline
          ys = imageBase->SkylineSpectrum();
        if (m_Controls.cbOverviewSpectra->currentIndex() == 1) // mean
          ys = imageBase->MeanSpectrum();
        if (m_Controls.cbOverviewSpectra->currentIndex() == 2) // sum
          ys = imageBase->SumSpectrum();

        xs = imageBase->GetXAxis();

        auto mad = m2::Signal::mad(ys);
        auto &peaks = imageBase->GetPeaks();
        peaks.clear();
        m2::Signal::localMaxima(std::begin(ys),
                                std::end(ys),
                                std::begin(xs),
                                std::back_inserter(peaks),
                                m_Controls.sliderHWS->value(),
                                mad * m_Controls.sliderSNR->value());
        if (m_Controls.ckbMonoisotopic->isChecked())
        {
          peaks = m2::Signal::monoisotopic(peaks,
                                           {3, 4, 5, 6, 7, 8, 9, 10},
                                           m_Controls.sliderCOR->value(),
                                           m_Controls.sbTolerance->value(),
                                           m_Controls.sbDistance->value());
        }
        imageBase->PeakListModified();

        m_PeakList = peaks;
      }
    }
    OnImageSelectionChangedUpdatePeakList(0);
    // emit m2::UIUtils::Instance()->BroadcastProcessingNodes("", node);
  }
}

void m2PeakPickingView::OnUpdatePeakListLabel(){
  u_int32_t selected = 0;
  for (int i = 0; i < m_Controls.tableWidget->rowCount(); ++i)
    if(m_Controls.tableWidget->item(i, 0)->checkState() == Qt::CheckState::Checked)
      selected += 1;
  m_Controls.labelPeakList->setText(QString("Peaks (%1/%2)").arg(selected).arg((int)m_PeakList.size()));

    
}

void m2PeakPickingView::OnImageSelectionChangedUpdatePeakList(int)
{
  if (!m_PeakList.empty())
  {
    m_Controls.tableWidget->clearContents();
    m_Controls.tableWidget->setRowCount(m_PeakList.size());
    
    unsigned int row = 0;
    m_Controls.tableWidget->blockSignals(true);
    for (auto &p : m_PeakList)
    {
      auto item = new QTableWidgetItem(std::to_string(p.GetX()).c_str());
      item->setCheckState(Qt::CheckState::Checked);
      m_Controls.tableWidget->setItem(row++, 0, item);
    }
    m_Controls.tableWidget->blockSignals(false);
    this->OnUpdatePeakListLabel();
  }
}

void m2PeakPickingView::OnStartPCA()
{
  if (auto node = m_Controls.nodeSelection->GetSelectedNode())
  {
    if (auto imageBase = dynamic_cast<m2::SpectrumImageBase *>(node->GetData()))
    {
      auto filter = m2::PcaImageFilter::New();
      filter->SetMaskImage(imageBase->GetMaskImage());

      const auto &peakList = m_PeakList;

      std::vector<mitk::Image::Pointer> temporaryImages;

      size_t inputIdx = 0;
      for (size_t row = 0; row < peakList.size(); ++row)
      {
        if (m_Controls.tableWidget->item(row, 0)->checkState() != Qt::CheckState::Checked)
          continue;

        temporaryImages.push_back(mitk::Image::New());
        temporaryImages.back()->Initialize(imageBase);

        imageBase->GetImage(peakList[row].GetX(),
                            imageBase->ApplyTolerance(peakList[row].GetX()),
                            imageBase->GetMaskImage(),
                            temporaryImages.back());
        filter->SetInput(inputIdx, temporaryImages.back());
        ++inputIdx;
      }

      if (temporaryImages.size() == 0)
      {
        QMessageBox::warning(nullptr,
                             "Select images first!",
                             "Select at least three peaks!",
                             QMessageBox::StandardButton::NoButton,
                             QMessageBox::StandardButton::Ok);
        return;
      }

      filter->SetNumberOfComponents(m_Controls.pca_dims->value());
      filter->Update();

      auto outputNode = mitk::DataNode::New();
      mitk::Image::Pointer data = filter->GetOutput(0);
      outputNode->SetData(data);
      outputNode->SetName("PCA");
      this->GetDataStorage()->Add(outputNode, node.GetPointer());
      imageBase->InsertImageArtifact("PCA", data);

      // auto outputNode2 = mitk::DataNode::New();
      // mitk::Image::Pointer data2 = filter->GetOutput(1);
      // outputNode2->SetData(data2);
      // outputNode2->SetName("pcs");
      // this->GetDataStorage()->Add(outputNode2, node.GetPointer());
    }
  }
}

void m2PeakPickingView::OnStartTSNE()
{
  if (auto node = m_Controls.nodeSelection->GetSelectedNode())
  {
    if (auto imageBase = dynamic_cast<m2::SpectrumImageBase *>(node->GetData()))
    {
      auto p = node->GetProperty("name")->Clone();
      static_cast<mitk::StringProperty *>(p.GetPointer())->SetValue("PCA");
      auto derivations = this->GetDataStorage()->GetDerivations(node, mitk::NodePredicateProperty::New("name", p));
      if (derivations->size() == 0)
      {
        QMessageBox::warning(nullptr,
                             "PCA required!",
                             "The t-SNE uses the top five features of the PCA transformed images! Start a PCA first.",
                             QMessageBox::StandardButton::NoButton,
                             QMessageBox::StandardButton::Ok);
        return;
      }

      auto pcaImage = dynamic_cast<mitk::Image *>(derivations->front()->GetData());
      const auto pcaComponents = pcaImage->GetPixelType().GetNumberOfComponents();

      auto filter = m2::TSNEImageFilter::New();
      filter->SetPerplexity(m_Controls.tsne_perplexity->value());
      filter->SetIterations(m_Controls.tnse_iters->value());
      filter->SetTheta(m_Controls.tsne_theta->value());

      using MaskImageType = itk::Image<mitk::LabelSetImage::PixelType, 3>;
      MaskImageType::Pointer maskImageItk;
      mitk::Image::Pointer maskImage;
      mitk::CastToItkImage(imageBase->GetMaskImage(), maskImageItk);
      auto caster = itk::ShrinkImageFilter<MaskImageType, MaskImageType>::New();
      caster->SetInput(maskImageItk);
      caster->SetShrinkFactor(0, m_Controls.tsne_shrink->value());
      caster->SetShrinkFactor(1, m_Controls.tsne_shrink->value());
      caster->SetShrinkFactor(2, 1);
      caster->Update();

      mitk::CastToMitkImage(caster->GetOutput(), maskImage);

      filter->SetMaskImage(maskImage);
      // const auto &peakList = imageBase->GetPeaks();
      size_t index = 0;

      mitk::ImageReadAccessor racc(pcaImage);
      auto *inputData = static_cast<const DisplayImageType::PixelType *>(racc.GetData());

      std::vector<mitk::Image::Pointer> bufferedImages(pcaComponents);
      unsigned int n = pcaImage->GetDimensions()[0] * pcaImage->GetDimensions()[1] * pcaImage->GetDimensions()[2];
      for (auto &I : bufferedImages)
      {
        I = mitk::Image::New();
        I->Initialize(imageBase);
        {
          mitk::ImageWriteAccessor acc(I);
          auto outCData = static_cast<DisplayImageType::PixelType *>(acc.GetData());
          for (unsigned int k = 0; k < n; ++k)
            *(outCData + k) = *(inputData + (k * pcaComponents) + index);
        }

        DisplayImageType::Pointer cImage;
        mitk::CastToItkImage(I, cImage);

        auto caster = itk::ShrinkImageFilter<DisplayImageType, DisplayImageType>::New();
        caster->SetInput(cImage);
        caster->SetShrinkFactor(0, m_Controls.tsne_shrink->value());
        caster->SetShrinkFactor(1, m_Controls.tsne_shrink->value());
        caster->SetShrinkFactor(2, 1);
        caster->Update();

        // Buffer the image
        mitk::CastToMitkImage(caster->GetOutput(), I);
        filter->SetInput(index, I);
        ++index;
      }
      filter->Update();

      auto outputNode = mitk::DataNode::New();
      auto data =
        m2::MultiSliceFilter::ConvertMitkVectorImageToRGB(ResampleVectorImage(filter->GetOutput(), imageBase));
      outputNode->SetData(data);
      outputNode->SetName("tSNE");
      this->GetDataStorage()->Add(outputNode, node.GetPointer());
      imageBase->InsertImageArtifact("tSNE", data);
    }
  }
}

mitk::Image::Pointer m2PeakPickingView::ResampleVectorImage(mitk::Image::Pointer vectorImage,
                                                            mitk::Image::Pointer referenceImage)
{
  const unsigned int components = vectorImage->GetPixelType().GetNumberOfComponents();

  VectorImageType::Pointer vectorImageItk;
  mitk::CastToItkImage(vectorImage, vectorImageItk);

  DisplayImageType::Pointer referenceImageItk;
  mitk::CastToItkImage(referenceImage, referenceImageItk);

  auto resampledVectorImageItk = VectorImageType::New();
  resampledVectorImageItk->SetOrigin(referenceImageItk->GetOrigin());
  resampledVectorImageItk->SetDirection(referenceImageItk->GetDirection());
  resampledVectorImageItk->SetSpacing(referenceImageItk->GetSpacing());
  resampledVectorImageItk->SetRegions(referenceImageItk->GetLargestPossibleRegion());
  resampledVectorImageItk->SetNumberOfComponentsPerPixel(components);
  resampledVectorImageItk->Allocate();
  itk::VariableLengthVector<m2::DisplayImagePixelType> v(components);
  v.Fill(0);
  resampledVectorImageItk->FillBuffer(v);

  auto inAdaptor = VectorImageAdaptorType::New();
  auto outAdaptor = VectorImageAdaptorType::New();
  using LinearInterpolatorType = itk::LinearInterpolateImageFunction<VectorImageAdaptorType>;
  using TransformType = itk::IdentityTransform<m2::DisplayImagePixelType, 3>;

  for (unsigned int i = 0; i < components; ++i)
  {
    inAdaptor->SetExtractComponentIndex(i);
    inAdaptor->SetImage(vectorImageItk);
    inAdaptor->SetOrigin(vectorImageItk->GetOrigin());
    inAdaptor->SetDirection(vectorImageItk->GetDirection());
    inAdaptor->SetSpacing(vectorImageItk->GetSpacing());

    outAdaptor->SetExtractComponentIndex(i);
    outAdaptor->SetImage(resampledVectorImageItk);

    auto resampler = itk::ResampleImageFilter<VectorImageAdaptorType, DisplayImageType>::New();
    resampler->SetInput(inAdaptor);
    resampler->SetOutputParametersFromImage(referenceImageItk);
    resampler->SetInterpolator(LinearInterpolatorType::New());
    resampler->SetTransform(TransformType::New());
    resampler->Update();

    itk::ImageAlgorithm::Copy<DisplayImageType, VectorImageAdaptorType>(
      resampler->GetOutput(),
      outAdaptor,
      resampler->GetOutput()->GetLargestPossibleRegion(),
      outAdaptor->GetLargestPossibleRegion());
  }

  mitk::Image::Pointer outImage;
  mitk::CastToMitkImage(resampledVectorImageItk, outImage);

  return outImage;
}
