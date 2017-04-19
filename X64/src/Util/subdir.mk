################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Util/File.cpp \
../src/Util/MD5.cpp \
../src/Util/SSLBox.cpp \
../src/Util/SqlConnection.cpp \
../src/Util/util.cpp 

OBJS += \
./src/Util/File.o \
./src/Util/MD5.o \
./src/Util/SSLBox.o \
./src/Util/SqlConnection.o \
./src/Util/util.o 

CPP_DEPS += \
./src/Util/File.d \
./src/Util/MD5.d \
./src/Util/SSLBox.d \
./src/Util/SqlConnection.d \
./src/Util/util.d 


# Each subdirectory must supply rules for building sources it contributes
src/Util/%.o: ../src/Util/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++0x -DENABLE_MYSQL -DENABLE_OPENSSL -I../src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


