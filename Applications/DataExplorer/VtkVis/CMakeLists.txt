set(SOURCES
    MeshFromRasterDialog.cpp
    QVtkDataSetMapper.cpp
    VisPrefsDialog.cpp
    VisualizationWidget.cpp
    VtkAddFilterDialog.cpp
    VtkAlgorithmProperties.cpp
    VtkAlgorithmPropertyCheckbox.cpp
    VtkAlgorithmPropertyLineEdit.cpp
    VtkAlgorithmPropertyVectorEdit.cpp
    VtkAppendArrayFilter.cpp
    VtkBGImageSource.cpp
    VtkColorByHeightFilter.cpp
    VtkColorLookupTable.cpp
    VtkCompositeFilter.cpp
    VtkCompositeColorByHeightFilter.cpp
    VtkCompositeColormapToImageFilter.cpp
    VtkCompositeContourFilter.cpp
    VtkCompositeElementSelectionFilter.cpp
    VtkCompositeGeoObjectFilter.cpp
    VtkCompositeImageToCylindersFilter.cpp
    VtkCompositeLineToTubeFilter.cpp
    VtkCompositeNodeSelectionFilter.cpp
    VtkCompositePointToGlyphFilter.cpp
    VtkCompositeTextureOnSurfaceFilter.cpp
    VtkCompositeThresholdFilter.cpp
    VtkConsoleOutputWindow.cpp
    VtkFilterFactory.cpp
    VtkGeoImageSource.cpp
    VtkImageDataToLinePolyDataFilter.cpp
    VtkPolylinesSource.cpp
    VtkPointsSource.cpp
    VtkRaster.cpp
    VtkStationSource.cpp
    VtkSurfacesSource.cpp
    VtkTextureOnSurfaceFilter.cpp
    VtkVisHelper.cpp
    VtkVisImageItem.cpp
    VtkVisPipeline.cpp
    VtkVisPipelineItem.cpp
    VtkVisPipelineView.cpp
    VtkVisPointSetItem.cpp
    VtkVisTabWidget.cpp
)

set(HEADERS
    VtkAppendArrayFilter.h
    VtkBGImageSource.h
    VtkColorByHeightFilter.h
    VtkColorLookupTable.h
    VtkCompositeFilter.h
    VtkCompositeColorByHeightFilter.h
    VtkCompositeColormapToImageFilter.h
    VtkCompositeContourFilter.h
    VtkCompositeElementSelectionFilter.h
    VtkCompositeGeoObjectFilter.h
    VtkCompositeImageToCylindersFilter.h
    VtkCompositeLineToTubeFilter.h
    VtkCompositeNodeSelectionFilter.h
    VtkCompositePointToGlyphFilter.h
    VtkCompositeTextureOnSurfaceFilter.h
    VtkCompositeThresholdFilter.h
    VtkFilterFactory.h
    VtkGeoImageSource.h
    VtkImageDataToLinePolyDataFilter.h
    VtkPolylinesSource.h
    VtkPointsSource.h
    VtkRaster.h
    VtkStationSource.h
    VtkSurfacesSource.h
    VtkTextureOnSurfaceFilter.h
    VtkVisHelper.h
    VtkVisImageItem.h
    VtkVisPipelineItem.h
    VtkVisPointSetItem.h
    MeshFromRasterDialog.h
    QVtkDataSetMapper.h
    VisPrefsDialog.h
    VisualizationWidget.h
    VtkAddFilterDialog.h
    VtkAlgorithmProperties.h
    VtkAlgorithmPropertyLineEdit.h
    VtkAlgorithmPropertyCheckbox.h
    VtkAlgorithmPropertyVectorEdit.h
    VtkVisPipeline.h
    VtkVisPipelineView.h
    VtkVisTabWidget.h
    VtkConsoleOutputWindow.h
)

# Visual Studio folder
source_group("Filter Header Files" REGULAR_EXPRESSION "[.]*Filter.h")
source_group("Filter Source Files" REGULAR_EXPRESSION "[.]*Filter.cpp")
source_group("Source Header Files" REGULAR_EXPRESSION "[.]*Source.h")
source_group("Source Source Files" REGULAR_EXPRESSION "[.]*Source.cpp")
file(GLOB_RECURSE UIS *.ui)
source_group("UI Files" FILES ${UIS})

set(SOURCE_DIR_REL ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(GUI_SOURCE_DIR_REL ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SOURCE_DIR_REL}/Applications/FileIO
    ${SOURCE_DIR_REL}/BaseLib
    ${SOURCE_DIR_REL}/GeoLib
    ${SOURCE_DIR_REL}/MathLib
    ${SOURCE_DIR_REL}/MeshLib
    ${GUI_SOURCE_DIR_REL}/VtkAct
    ${GUI_SOURCE_DIR_REL}/Base
    ${GUI_SOURCE_DIR_REL}/DataView
    ${GUI_SOURCE_DIR_REL}/VtkModules/Qt

    ${CMAKE_CURRENT_BINARY_DIR}/../../BaseLib
    ${CMAKE_CURRENT_BINARY_DIR}/../DataView
)

add_library(VtkVis
    ${SOURCES}
    ${HEADERS}
    ${UIS}
)

ADD_VTK_DEPENDENCY(VtkVis)

if(GEOTIFF_FOUND)
    include_directories(${GEOTIFF_INCLUDE_DIRS})
    target_link_libraries(VtkVis ${GEOTIFF_LIBRARIES})
endif() # GEOTIFF_FOUND

target_link_libraries(VtkVis Qt5::Gui QtDataView VtkAct)
set_property(TARGET VtkVis PROPERTY FOLDER "DataExplorer")
