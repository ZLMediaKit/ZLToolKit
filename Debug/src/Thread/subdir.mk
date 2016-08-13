################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Thread/AsyncTaskThread.cpp \
../src/Thread/WorkThreadPool.cpp 

OBJS += \
./src/Thread/AsyncTaskThread.o \
./src/Thread/WorkThreadPool.o 

CPP_DEPS += \
./src/Thread/AsyncTaskThread.d \
./src/Thread/WorkThreadPool.d 


# Each subdirectory must supply rules for building sources it contributes
src/Thread/%.o: ../src/Thread/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -I"/home/xzl/workspace/ZLToolKit/src" -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


