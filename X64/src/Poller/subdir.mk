################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Poller/EventPoller.cpp \
../src/Poller/Pipe.cpp \
../src/Poller/SelectWrap.cpp \
../src/Poller/Timer.cpp 

OBJS += \
./src/Poller/EventPoller.o \
./src/Poller/Pipe.o \
./src/Poller/SelectWrap.o \
./src/Poller/Timer.o 

CPP_DEPS += \
./src/Poller/EventPoller.d \
./src/Poller/Pipe.d \
./src/Poller/SelectWrap.d \
./src/Poller/Timer.d 


# Each subdirectory must supply rules for building sources it contributes
src/Poller/%.o: ../src/Poller/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++0x -DENABLE_MYSQL -DENABLE_OPENSSL -I../src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


