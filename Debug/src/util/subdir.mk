################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Util/SSLBox.cpp \
../src/Util/SqlConnection.cpp 

OBJS += \
./src/Util/SSLBox.o \
./src/Util/SqlConnection.o 

CPP_DEPS += \
./src/Util/SSLBox.d \
./src/Util/SqlConnection.d 


# Each subdirectory must supply rules for building sources it contributes
src/Util/%.o: ../src/Util/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -I"/home/xzl/workspace/ZLToolKit/src" -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


