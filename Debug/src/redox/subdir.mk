################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/redox/client.cpp \
../src/redox/command.cpp \
../src/redox/subscriber.cpp 

OBJS += \
./src/redox/client.o \
./src/redox/command.o \
./src/redox/subscriber.o 

CPP_DEPS += \
./src/redox/client.d \
./src/redox/command.d \
./src/redox/subscriber.d 


# Each subdirectory must supply rules for building sources it contributes
src/redox/%.o: ../src/redox/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -I"/home/xzl/workspace/ZLToolKit/src" -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


