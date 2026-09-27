# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Test)

# One executable per behaviour area; each runs offline with no host, server
# or display.
foreach(test IN ITEMS tst_hostsampleparser tst_journalparser tst_llamaparsers)
    qt_add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE llamawatch_core Qt6::Test)
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
