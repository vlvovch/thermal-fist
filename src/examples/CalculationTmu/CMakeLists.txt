# Properties->C/C++->General->Additional Include Directories
include_directories ("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")

set(SRCS
CalculationTmu.cpp
)

# Set Properties->General->Configuration Type to Application(.exe)
# Creates app.exe with the listed sources (main.cxx)
# Adds sources to the Solution Explorer
add_executable (CalculationTmu ${SRCS})

# Properties->Linker->Input->Additional Dependencies
target_link_libraries (CalculationTmu ThermalFIST)

# Creates a folder "executables" and adds target 
# project (app.vcproj) under it
set_property(TARGET CalculationTmu PROPERTY FOLDER "examples")
#set_property(TARGET BagModelFit PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples")

# Adds logic to INSTALL.vcproj to copy app.exe to destination directory
install (TARGETS CalculationTmu
         RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin/examples)
		 