################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Network/Socket.cpp \
../src/Network/TcpClient.cpp \
../src/Network/sockutil.cpp 

OBJS += \
./src/Network/Socket.o \
./src/Network/TcpClient.o \
./src/Network/sockutil.o 

CPP_DEPS += \
./src/Network/Socket.d \
./src/Network/TcpClient.d \
./src/Network/sockutil.d 


# Each subdirectory must supply rules for building sources it contributes
src/Network/%.o: ../src/Network/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++0x -DENABLE_MYSQL -DENABLE_OPENSSL -I../src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


