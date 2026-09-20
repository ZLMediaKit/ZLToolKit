file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ssl.p12 DESTINATION ${PROJECT_BINARY_DIR}/bin)

aux_source_directory(. TEST_SRC_LIST)

#部分测试程序直接使用了std::thread与pthread接口，glibc 2.34以前的平台需要显式链接线程库，
#否则这些程序会链接失败(本目录下的test_ringBuffer等即是如此)
#Some test programs use std::thread and the pthread interfaces directly; platforms with a
#glibc older than 2.34 need the thread library to be linked explicitly, otherwise those
#programs fail to link (test_ringBuffer in this directory for instance)
find_package(Threads REQUIRED)

#Windows上先定位MySQL的运行时dll，下面每个测试程序都要用到
#Locate the MySQL runtime dll on Windows first, every test program below needs it
if(WIN32 AND MYSQL_FOUND)
    get_filename_component(MYSQL_LIB_DIR "${MYSQL_LIBRARIES}" DIRECTORY)
    find_file(MYSQL_RUNTIME_DLL
              NAMES libmysql.dll
              HINTS "${MYSQL_LIB_DIR}" "${MYSQL_LIB_DIR}/../bin" "${MYSQL_LIB_DIR}/../lib"
              NO_DEFAULT_PATH)
    if(NOT MYSQL_RUNTIME_DLL)
        message(WARNING "未找到libmysql.dll，Windows上的测试程序可能无法启动"
                        " (libmysql.dll not found, the test programs may fail to start on Windows)")
    endif()
endif()

#门禁用例白名单：只收录不依赖网络、命令行参数与外部服务，能自行结束并以退出码表明结果的程序。
#本目录其余多为服务型或压测程序(运行后不会自行退出)，全量注册会让ctest一直挂起，故用白名单。
#Gating test whitelist: only programs that need no network, no command-line argument and no
#external service, and that finish on their own and report the outcome through the exit code.
#Most of the others here are long-running services or benchmarks that never exit by themselves,
#so registering everything would hang ctest; hence a whitelist.
#其余程序的现状：
#  test_semaphore   程序自身在退出时强制关闭消费线程，必定触发terminate，待修复后并入
#  test_sql         需要可用的MySQL服务
#  test_ssl         阻塞在std::cin读取，无人值守时会空转到超时
#  test_shell       需要连接对端服务
#  其余(test_tcpClient等)  为服务型程序，不会自行退出
#State of the remaining programs: test_semaphore always terminates on purpose while shutting down
#its consumer thread and will be added once fixed; test_sql needs a running MySQL; test_ssl blocks
#on std::cin and would spin until the timeout when unattended; test_shell needs a peer to connect
#to; the rest are long-running services.
#注：收录标准是"能无人值守跑完"，而不是"不碰套接字"——test_ringBuffer用到EventPoller，
#其PipeWrap在Windows上本就是以127.0.0.1的TCP实现的，按后者衡量并无意义。
#Note: the criterion is being able to run unattended, not staying away from sockets: test_ringBuffer
#uses EventPoller, whose PipeWrap is implemented on top of a 127.0.0.1 TCP connection on Windows
#anyway, so the latter criterion would make little sense.
#test_ringBuffer与test_udpSocketBufferConfig在Windows上会卡在进程退出阶段：流水线
#实测中test_udpSocketBufferConfig的断言已全部通过并打印出"regression passed"，进程
#却始终不退出，最终被120秒超时判为失败；test_ringBuffer同样停在事件回调之后。两者
#都使用了EventPollerPool，可见是ZLToolKit在Windows上事件轮询线程于进程退出时未能
#结束所致。该问题不是本次改动引入的，但也不应为了让流水线变绿而把用例从白名单里
#摘掉——那等于把门禁刚暴露出来的问题重新藏起来。故此处照常收录，Windows流水线会
#因此保持失败，直至该问题被单独修复。
#test_ringBuffer and test_udpSocketBufferConfig hang while the process exits on Windows: the
#pipeline shows test_udpSocketBufferConfig passing every assertion and printing
#"regression passed", yet the process never exits and is failed by the 120 second timeout;
#test_ringBuffer likewise stops after its event callbacks. Both use EventPollerPool, so the
#cause is that the event polling threads of ZLToolKit fail to finish while the process exits
#on Windows. The problem predates this change, but dropping the cases from the whitelist to
#turn the pipeline green would put back into hiding exactly what the gate just surfaced, so
#they stay listed and the Windows pipeline stays red until it is fixed separately.
set(GATING_TESTS
    test_creator
    test_ini
    test_logger
    test_resourcePool
    test_ringBuffer
    test_threadPool
    test_udpSocketBufferConfig
    test_variant)

foreach(TEST_SRC ${TEST_SRC_LIST})
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TESTER ${TEST_SRC})
    message(STATUS "add tester :${TESTER}")
    add_executable(${TESTER} ${TEST_SRC})
    target_link_libraries(${TESTER} PRIVATE ${PROJECT_NAME} Threads::Threads)
    set_target_properties(${TESTER} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

    if(WIN32 AND BUILD_SHARED_LIBS)
        #Windows没有RPATH机制，动态库又属于RUNTIME产物、不受本工程已设置的
        #LIBRARY_OUTPUT_DIRECTORY约束，于是dll与测试程序分处两地，运行时必然
        #以0xc0000135(找不到dll)失败。此处把dll复制到测试程序旁边，而不是去改动
        #库自身的产物位置：把本工程作为子项目引入的工程(如ZLMediaKit)往往用
        #EXECUTABLE_OUTPUT_PATH等全局变量统一收拢产物，而目标属性的优先级高于这些
        #全局变量，一旦在本工程内设置RUNTIME_OUTPUT_DIRECTORY，反而会把dll从上层
        #工程的产物目录里拽走，导致上层的可执行文件找不到它
        #Windows has no RPATH and a shared library is a RUNTIME artifact, so it is not covered
        #by the LIBRARY_OUTPUT_DIRECTORY this project already sets; the dll and the test
        #executable therefore end up in different directories and every test fails with
        #0xc0000135 (dll not found). The dll is copied next to the test executable instead of
        #relocating the library itself: projects embedding this one (ZLMediaKit for instance)
        #tend to collect every artifact through globals such as EXECUTABLE_OUTPUT_PATH, and
        #since target properties win over those globals, setting RUNTIME_OUTPUT_DIRECTORY here
        #would drag the dll out of the embedding project's output directory and break it
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
            #TARGET_RUNTIME_DLLS会沿依赖链收集该测试程序运行时需要的全部dll，
            #只复制本工程的dll并不够：它自身依赖的第三方dll同样要在旁边
            #TARGET_RUNTIME_DLLS walks the dependency chain and collects every dll the test
            #needs at run time; copying only this project's dll is not enough, the third party
            #dlls it depends on have to sit next to it as well
            add_custom_command(TARGET ${TESTER} POST_BUILD
                               COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       $<TARGET_RUNTIME_DLLS:${TESTER}> $<TARGET_FILE_DIR:${TESTER}>
                               COMMAND_EXPAND_LISTS)
        else()
            #更老的CMake没有TARGET_RUNTIME_DLLS，只能复制本工程的dll
            #Older CMake has no TARGET_RUNTIME_DLLS, only this project's dll can be copied
            add_custom_command(TARGET ${TESTER} POST_BUILD
                               COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${TESTER}>)
        endif()

        #MySQL是以导入库(.lib)的绝对路径链接进来的，并非imported target，
        #TARGET_RUNTIME_DLLS无从得知对应的dll，需要自行找出并复制；
        #否则Windows上构建出来的程序会因缺少libmysql.dll而根本无法启动
        #MySQL is linked through the absolute path of its import library rather than as an
        #imported target, so TARGET_RUNTIME_DLLS cannot know about the matching dll and it has
        #to be located and copied here; otherwise the binaries built on Windows fail to start
        #at all for the lack of libmysql.dll
        if(MYSQL_FOUND AND MYSQL_RUNTIME_DLL)
            add_custom_command(TARGET ${TESTER} POST_BUILD
                               COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       ${MYSQL_RUNTIME_DLL} $<TARGET_FILE_DIR:${TESTER}>)
        endif()
    endif()

    #此处不用if(... IN_LIST ...)，那需要CMP0057策略(CMake 3.3起)，而本工程声明的最低版本是
    #3.1.3；版本范围语法又要CMake 3.12才被识别，老版本会退化为3.1.3从而导致配置报错
    #if(... IN_LIST ...) is not used here: it needs the CMP0057 policy (CMake 3.3 and later)
    #while this project declares 3.1.3 as its minimum, and the version range syntax is only
    #understood from CMake 3.12 on, older ones fall back to 3.1.3 and the configuration fails
    list(FIND GATING_TESTS ${TESTER} GATING_TEST_INDEX)
    if(NOT GATING_TEST_INDEX EQUAL -1)
        add_test(NAME ${TESTER} COMMAND ${TESTER})
        #单个用例正常在秒级内结束，超时即判失败，避免卡死整条流水线
        #A single case finishes within seconds; time out instead of stalling the whole pipeline
        set_tests_properties(${TESTER} PROPERTIES TIMEOUT 120)

    endif()
endforeach()

#白名单里若残留已删除或改名的用例，上面的循环只会静默跳过，门禁将少跑用例而无人察觉，
#这与引入门禁要解决的问题是同一类，故此处显式校验
#A case left in the whitelist after being deleted or renamed would just be skipped silently
#above, and the gate would quietly cover less than it claims, which is the very kind of rot
#this gate is meant to prevent; hence the explicit check
foreach(GATING_TEST ${GATING_TESTS})
    if(NOT TARGET ${GATING_TEST})
        message(FATAL_ERROR "门禁白名单中的用例不存在: ${GATING_TEST}"
                            " (gating test not found, check GATING_TESTS in tests/CMakeLists.txt)")
    endif()
endforeach()
