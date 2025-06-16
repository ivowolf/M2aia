/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include <functional>
#include <itkOpenSlideImageIO.h>
#include <itksys/SystemTools.hxx>
#include <m2FsmSpectrumImage.h>
#include <m2ZarrImageIO.h>
#include <map>
#include <mitkCoreServices.h>
#include <mitkIOUtil.h>
#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkImageCast.h>
#include <mitkImagePixelWriteAccessor.h>
#include <mitkPixelType.h>
#include <nlohmann/json.hpp>
#include <parallelreadzarr.h>
#include <zarr.h>
#include <lz4.h>

namespace m2
{
  ZarrImageIO::ZarrImageIO() : AbstractFileIO(mitk::Image::GetStaticNameOfClass(), Zarr_MIMETYPE(), "Zarr Image")
  {
    AbstractFileWriter::SetRanking(10);
    AbstractFileReader::SetRanking(10);

    // defaultOptions[OPTION_MERGE_POINTS()] = us::Any(true);
    // defaultOptions[A_IMPORT_VOLUME] = us::Any(true);

    this->RegisterService();
  }

  mitk::IFileIO::ConfidenceLevel ZarrImageIO::GetWriterConfidenceLevel() const
  {
    if (AbstractFileIO::GetWriterConfidenceLevel() == Unsupported)
      return Unsupported;
    // const auto *input = static_cast<const m2::ImzMLSpectrumImage *>(this->GetInput());
    // if (input)
    //   return Supported;
    // else
    return Unsupported;
  }

  void ZarrImageIO::Write()
  {
    ValidateOutputLocation();
  }

  void ZarrImageIO::LoadAssociatedData(mitk::BaseData *) {}

  mitk::IFileIO::ConfidenceLevel ZarrImageIO::GetReaderConfidenceLevel() const
  {
    if (AbstractFileIO::GetReaderConfidenceLevel() == Unsupported)
      return Unsupported;
    return Supported;
  }

  std::vector<mitk::BaseData::Pointer> ZarrImageIO::DoRead()
  {
    MITK_INFO << "ZarrImageIO::DoRead() " << this->GetInputLocation();

    // Assuming .zgroup as input location
    std::string cwd = this->GetInputLocation();
    auto groupDir = itksys::SystemTools::GetParentDirectory(cwd);
    
    auto hypercubeDir = groupDir + "/hypercube";
    auto hypercubeZarray = hypercubeDir + "/.zarray";
    if(!(itksys::SystemTools::PathExists(hypercubeDir) &&
       itksys::SystemTools::FileExists(hypercubeZarray))){
      MITK_ERROR << "ZarrImageIO::DoRead() No hypercube directory found";
      return {};
    }

    auto wvnmDir = groupDir + "/wvnm";
    auto wvnmZarray = wvnmDir + "/.zarray";
    if(!(itksys::SystemTools::PathExists(wvnmDir) &&
    itksys::SystemTools::FileExists(wvnmZarray))){
      MITK_ERROR << "ZarrImageIO::DoRead() No wvnm directory found";
      return {};
    }
    
    
    json hypercubeJson = json::parse(std::ifstream(hypercubeZarray));
    json wvnmJson = json::parse(std::ifstream(wvnmZarray));

    // Can be defined in the .zarray json
    mitk::AffineTransform3D::Pointer transform;
    int dimensions = 0;
    std::vector<std::string> kinds;
    
    bool vectorDimensionExists = false;
    int vDim = -1;
    unsigned int vDimSize = -1;
    std::array<unsigned int, 3> shape;
    std::vector<float> wvnmData;
    
    if (hypercubeJson.contains("Image"))
    {
      if (hypercubeJson["Image"].contains("Transform"))
      {
        transform = mitk::AffineTransform3D::New();
        mitk::FromJSON(hypercubeJson["Image"]["Transform"], transform);
      }
      
      if (hypercubeJson["Image"].contains("Kinds"))
      {
        hypercubeJson["Image"]["Kinds"].get_to(kinds);
        
        if (hypercubeJson["Image"].contains("Dimensions"))
        hypercubeJson["Image"]["Dimensions"].get_to(dimensions);
        
        auto vPosIt = std::find(std::begin(kinds), std::end(kinds), "vector");
        vectorDimensionExists = vPosIt != std::end(kinds);
        vDim = std::distance(std::begin(kinds), vPosIt);
        hypercubeJson["shape"].get_to(shape);
        vDimSize = shape[vDim];
      }
      if (hypercubeJson["Image"].contains("wvnm")){
        wvnmData.resize(shape[2]);
        hypercubeJson["Image"]["wvnm"].get_to(wvnmData);
      }
    }
    
    
    mitk::Image::Pointer outImage = nullptr;


    




    // auto initializeImage = []<typename T, int DIM = 3>(unsigned int components, unsigned int * dims, void * data) ->
    // mitk::Image::Pointer {

    //   return outImage;
    // };

    MITK_INFO << "ZarrImageIO::DoRead() cwd: " << cwd;
    try
    {

      auto hypercubeHandle = zarr(hypercubeDir.c_str());
      auto dtype = hypercubeHandle.get_dtype();
      // bool isLittleEndian = dtype[0] == '<';
      auto dtypeChar = dtype[1];
      auto dtypeSize = int(dtype[2]) - 48;

      // Read all data
      bool cOrder = true;
      auto data = readZarrParallelHelper(
        hypercubeDir.c_str(), 0, 0, 0, hypercubeHandle.get_shape(0), hypercubeHandle.get_shape(1), hypercubeHandle.get_shape(2), cOrder);




           
      
      
      // std::array<unsigned int, 3>DIM_3D_SLICE_PLUS_V{shape[0],shape[1],1};
      // std::array<unsigned int, 3>DIM_3D_VOLUME_PLUS_V{shape[0],shape[1],shape[2]};

      switch (dtypeChar)
      {
        case 'u':
          if (dtypeSize == 1)
          {
            MITK_INFO << "uint8";
          }
          else if (dtypeSize == 2)
          {
            MITK_INFO << "uint16";
          }
          else if (dtypeSize == 4)
          {
            MITK_INFO << "uint32";
          }
          else if (dtypeSize == 8)
          {
            MITK_INFO << "uint64";
          }
          break;
        case 'i':
          if (dtypeSize == 1)
          {
            MITK_INFO << "int8";
          }
          else if (dtypeSize == 2)
          {
            MITK_INFO << "int16";
          }
          else if (dtypeSize == 4)
          {
            MITK_INFO << "int32";
          }
          else if (dtypeSize == 8)
          {
            MITK_INFO << "int64";
          }
          break;
        case 'f':
          if (dtypeSize == 1)
          {
            MITK_INFO << "float8";
          }
          else if (dtypeSize == 2)
          {
            MITK_INFO << "float16";
          }
          else if (dtypeSize == 4)
          {
            if ((dimensions == 2 || dimensions == 3) && vectorDimensionExists)
            {
              float *dataF = static_cast<float *>(data);
              

              auto fsmImage = m2::FsmSpectrumImage::New();
              fsmImage->SetPropertyValue<unsigned>("dim_x", shape[0]); // n_x
              fsmImage->SetPropertyValue<unsigned>("dim_y", shape[1]); // n_z
              fsmImage->SetPropertyValue<unsigned>("dim_z", 1);

              auto origin = transform->GetOffset();
              fsmImage->SetPropertyValue<double>("[IMS:1000053] absolute position offset x", origin[0]); // x_init
              fsmImage->SetPropertyValue<double>("[IMS:1000054] absolute position offset y", origin[1]);
              fsmImage->SetPropertyValue<double>("absolute position offset z", 0);

              auto matrix = transform->GetMatrix();
              auto spacing = mitk::Vector3D();
                spacing[0] = matrix[0][0];
                spacing[1] = matrix[1][1];
                spacing[2] = matrix[2][2];
                fsmImage->SetPropertyValue<double>("spacing_x", m2::MicroMeterToMilliMeter(spacing[0])); // x_delta
              fsmImage->SetPropertyValue<double>("spacing_y", m2::MicroMeterToMilliMeter(spacing[1])); // y_delta
              fsmImage->SetPropertyValue<double>("spacing_z", m2::MicroMeterToMilliMeter(spacing[2]));

              // fsmImage->SetPropertyValue<double>("wavelength_delta", meta[2]); // z_delta
              // fsmImage->SetPropertyValue<double>("wavelength_start", meta[3]); // z_start
              // fsmImage->SetPropertyValue<double>("wavelength_end", meta[4]);   // z_end

              auto &wavelengths = fsmImage->GetXAxis();
              for (unsigned int i = 0 ; i < shape[2]; ++i)
              {
                wavelengths.push_back(wvnmData[i]);
              }

              fsmImage->InitializeGeometry();
              fsmImage->GetGeometry()->SetIndexToWorldTransform(transform);

              itk::Image<m2::DisplayImagePixelType, 3>::Pointer image;
              mitk::CastToItkImage(fsmImage, image);

              itk::ImageRegionIteratorWithIndex<itk::Image<m2::DisplayImagePixelType, 3>> it(
                image, image->GetLargestPossibleRegion());

              unsigned long id = 0;
              while (!it.IsAtEnd())
              {
                m2::FsmSpectrumImage::SpectrumData spectrum;
                spectrum.data.resize(vDimSize, 0);
                spectrum.index = it.GetIndex();

                auto x = spectrum.index[0];
                auto y = spectrum.index[1];

                for (unsigned int v = 0; v < vDimSize; ++v)
                {
                  spectrum.data[v] = dataF[(shape[0]-1-x)* shape[1] + y  + v * shape[0] * shape[1]];
                }

                spectrum.id = id;
                fsmImage->GetSpectra().emplace_back(spectrum);

                ++id;
                ++it;
              }
              return {fsmImage.GetPointer()};
            }

            // if ((dimensions == 2 || dimensions == 3) && vectorDimensionExists)
            // {
            //   using T = float;
            //   const auto dims = DIM_3D_SLICE_PLUS_V.data();
            //   outImage = mitk::Image::New();
            //   unsigned int dim = 3;

            //   auto pixelType = mitk::MakePixelType<itk::VectorImage<T, 3>>(vDimSize);
            //   outImage->Initialize(pixelType, dim, dims);
            //   MITK_INFO << outImage->GetPixelType().GetPixelTypeAsString();
            //   T *dataT = static_cast<T *>(data);
            //   // {
            //   //   mitk::ImagePixelWriteAccessor<T, 3> accessor(outImage);
            //   //   accessor.GetDimension(0);
            //   // }

            //   AccessVectorFixedDimensionByItk(outImage,
            //   (
            //     [dataT, dims](auto itkImage)
            //     {
            //       auto bufferPointer = itkImage->GetBufferPointer();
            //       std::copy(dataT, dataT + (dims[0] * dims[1] * dims[2]), bufferPointer);
            //     }),
            //   3);
            // }
            // if((dimensions == 2 || dimensions == 3) && vectorDimensionExists)
            //   outImage = initializeImage.template operator()<float, 3>(vDimSize, DIM_3D_SLICE_PLUS_V.data(), data);
            // else if(dimensions == 3 && vectorDimensionExists)
            //   outImage = initializeImage.template operator()<float, 3>(vDimSize, DIM_3D_VOLUME_PLUS_V.data(), data);

            // size[1] = handle.get_shape(0); // Y dimension
            // size[0] = handle.get_shape(1); // X dimension
            // size[2] = handle.get_shape(2); // Z dimension

            // if(outImage && transform){
            //   outImage->GetGeometry()->SetIndexToWorldTransform(transform);
            // }

            // return {outImage};
          }
          else if (dtypeSize == 8)
          {
            MITK_INFO << "float64";
          }
          break;
        default:
          mitkThrow() << "Unsupported dtype: " << dtype;
      }
    }

    catch (const std::exception &e)
    {
      MITK_ERROR << "ZarrImageIO::DoRead() exception: " << e.what();
      return {};
    }

    return {};
  } // namespace m2

  ZarrImageIO *ZarrImageIO::IOClone() const
  {
    return new ZarrImageIO(*this);
  }
} // namespace m2
