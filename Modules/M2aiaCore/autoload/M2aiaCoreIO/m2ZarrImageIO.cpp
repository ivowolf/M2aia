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
#include <m2SpectrumContainerImage.h>
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

#include <regex>
#include <filesystem>

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

  std::string ZarrImageIO::FindInfoFile(std::string currentDir) const{
    // Search in all parent directories for a txt file with "Info" (case insensitive)
    
    std::string infoFilePath;
    
    while (!currentDir.empty() && currentDir != "/")
    {
      std::vector<std::string> files;
      for (const auto &entry : std::filesystem::directory_iterator(currentDir))
      {
        if (entry.is_regular_file())
        {
          auto file = entry.path().filename().string();
          if (itksys::SystemTools::GetFilenameLastExtension(file) == ".txt")
          {
            std::string lowerFile = itksys::SystemTools::LowerCase(file);
            if (lowerFile.find("info") != std::string::npos)
            {
              infoFilePath = entry.path().string();
              MITK_INFO << "Found info file: " << infoFilePath;
              return infoFilePath;
            }
          }
        }
      }
      if (!infoFilePath.empty())
        break;
      currentDir = itksys::SystemTools::GetParentDirectory(currentDir);
    }
    return "";
  } 

  std::map<std::string,std::string> ZarrImageIO::ReadInfoFile(std::string infoFilePath) const
  {
    std::map<std::string,std::string> infoMap;
    
    if (infoFilePath.empty())
    {
      MITK_ERROR << "ZarrImageIO::ReadInfoFile() No info file found";
      return infoMap;
    }

    std::ifstream infoFile(infoFilePath);
    if (!infoFile.is_open())
    {
      MITK_ERROR << "ZarrImageIO::ReadInfoFile() Could not open info file: " << infoFilePath;
      return infoMap;
    }

    std::string line;
    while (std::getline(infoFile, line))
    {
      auto pos = line.find(':');
      if (pos != std::string::npos)
      {
        auto trim = [](std::string s) {
          s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
          s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
          return s;
        };
        auto key = trim(line.substr(0, pos));
        auto value = trim(line.substr(pos + 1));
        infoMap[key] = value;
      }
    }
    
    return infoMap;
  }

  std::vector<mitk::BaseData::Pointer> ZarrImageIO::DoRead()
  {
    

    // Assuming .zgroup as input location
    std::string cwd = this->GetInputLocation();
    auto groupDir = itksys::SystemTools::GetParentDirectory(cwd);


    float xPixelSize = -1.0f, yPixelSize = -1.0f;
    std::string infoFile = FindInfoFile(groupDir);

    if(!infoFile.empty()){
      auto infoMap = ReadInfoFile(infoFile);
      if (infoMap.find("X pixelsize") != infoMap.end())
      {
        xPixelSize = std::stof(infoMap["X pixelsize"]) / 1000.0; // Convert to mm);
        MITK_INFO << "X pixelsize: " << xPixelSize ;
      }
      if (infoMap.find("Y pixelsize") != infoMap.end())
      {
        yPixelSize = std::stof(infoMap["Y pixelsize"]) / 1000.0; // Convert to mm
        MITK_INFO << "Y pixelsize: " << yPixelSize;
      }
    } else {
      MITK_ERROR << "No info file found";
      
    }
    
    auto hypercubeDir = groupDir + "/hypercube";
    auto hypercubeZarray = hypercubeDir + "/.zarray";
    if(!(itksys::SystemTools::PathExists(hypercubeDir) &&
       itksys::SystemTools::FileExists(hypercubeZarray))){
      MITK_ERROR << "No hypercube directory found";
      return {};
    }

    auto wvnmDir = groupDir + "/wvnm";
    auto wvnmZarray = wvnmDir + "/.zarray";
    if(!(itksys::SystemTools::PathExists(wvnmDir) &&
    itksys::SystemTools::FileExists(wvnmZarray))){
      MITK_ERROR << "No wvnm directory found";
      return {};
    }
    
    
    json hypercubeJson = json::parse(std::ifstream(hypercubeZarray));
    json wvnmJson = json::parse(std::ifstream(wvnmZarray));

    std::array<unsigned int, 3> shape;
    hypercubeJson["shape"].get_to(shape);


    // Default Values
    mitk::AffineTransform3D::Pointer transform;
    int dimensions = 2;
    std::vector<std::string> kinds = {"domain", "domain", "vector"};
    
    int vDim = 2;
    unsigned int vDimSize = shape[vDim];
    bool vectorDimensionExists = true;

    transform = mitk::AffineTransform3D::New();
    mitk::AffineTransform3D::MatrixType matrix;
    mitk::AffineTransform3D::OutputVectorType offset;

    std::vector<float> defaultPixelSize;
    
    if(!infoFile.empty() && xPixelSize > 0.0f && yPixelSize > 0.0f)
    {
      defaultPixelSize = {xPixelSize, yPixelSize, 0.01f};
    }
    else
    {
      defaultPixelSize = {0.0043, 0.0043, 0.01}; // Default pixel size if not found in info file
    }
    
    
    
    matrix[0][0] = defaultPixelSize[0]; matrix[0][1] = 0.0;                 matrix[0][2] = 0.0;
    matrix[1][0] = 0.0;                 matrix[1][1] = defaultPixelSize[1]; matrix[1][2] = 0.0; // Negative y-direction for image coordinates
    matrix[2][0] = 0.0;                 matrix[2][1] = 0.0;                 matrix[2][2] = defaultPixelSize[2];
    
    offset[0] = defaultPixelSize[0] * shape[0]; // Assuming the offset in x-direction is the width of the image
    offset[1] = defaultPixelSize[1] * shape[1]; // Assuming the offset in y-direction is the height of the image
    offset[2] = 0.0;
    
    transform->SetMatrix(matrix);
    transform->SetOffset(offset);
    
    float defaultOffset = 954.0f;
    float defaultDelta = 2.0f;
    std::vector<float> wvnmData;
    for(unsigned int i = 0 ; i < shape[vDim]; ++i)
      wvnmData.push_back(defaultOffset + i * defaultDelta);
    
    
    
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
        vDimSize = shape[vDim];
      }
      
      if (hypercubeJson["Image"].contains("wvnm")){
        wvnmData.resize(shape[2]);
        hypercubeJson["Image"]["wvnm"].get_to(wvnmData);
      }
    }

    mitk::Image::Pointer outImage = nullptr;

    MITK_INFO << "cwd: " << cwd;
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
              

              auto fsmImage = m2::SpectrumContainerImage::New();
              fsmImage->SetPropertyValue<unsigned>("dim_x", shape[1]); 
              fsmImage->SetPropertyValue<unsigned>("dim_y", shape[0]); 
              fsmImage->SetPropertyValue<unsigned>("dim_z", 1);

              auto origin = transform->GetOffset();
              fsmImage->SetPropertyValue<double>("[IMS:1000053] absolute position offset x", origin[0]); // x_init
              fsmImage->SetPropertyValue<double>("[IMS:1000054] absolute position offset y", origin[1]);
              fsmImage->SetPropertyValue<double>("absolute position offset z", 0);

              auto matrix = transform->GetMatrix();
              auto spacing = mitk::Vector3D();
                spacing[0] = std::abs(matrix[0][0]);
                spacing[1] = std::abs(matrix[1][1]);
                spacing[2] = std::abs(matrix[2][2]);
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
                m2::SpectrumContainerImage::SpectrumData spectrum;
                spectrum.data.resize(vDimSize, 0);
                spectrum.index = it.GetIndex();

                auto x = spectrum.index[0];
                auto y = spectrum.index[1];

                for (unsigned int v = 0; v < vDimSize; ++v)
                {
                  spectrum.data[v] = dataF[(shape[0]-y-1)* shape[1] + x  + v * shape[0] * shape[1]];
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
      MITK_ERROR << "exception: " << e.what();
      return {};
    }

    return {};
  } // namespace m2

  ZarrImageIO *ZarrImageIO::IOClone() const
  {
    return new ZarrImageIO(*this);
  }
} // namespace m2
