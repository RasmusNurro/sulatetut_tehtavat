*** Settings ***
Library    SerialLibrary

*** Variables ***
${com}      COM12
${baud}     115200
${board}    nRF5340

*** Test Cases ***
Connect Serial
    Log To Console    Connecting to ${board} on ${com}
    Add Port    ${com}    baudrate=${baud}    encoding=ascii
    Port Should Be Open    ${com}
    Reset Input Buffer
    Reset Output Buffer

Valid Time Simple
    # 000120 -> 1 min 20 s -> 80 sekuntia
    Send Time And Check    000120    80X

Valid Time At Upper Boundary
    # 005959 -> 59 min 59 s -> 3599 sekuntia
    Send Time And Check    005959    3599X

Invalid Seconds Over Limit
    # sekunnit 67 > 59 -> TIME_VALUE_ERROR
    Send Time And Check    001067    -3X

Invalid Minutes Over Limit
    # minuutit 60 > 59 -> TIME_VALUE_ERROR
    Send Time And Check    006000    -3X

Invalid Hours Over Limit
    # tunnit 99 > 23 -> TIME_VALUE_ERROR
    Send Time And Check    999999    -3X

Invalid Length Too Short
    # vain 4 merkkia, pitaisi olla tasan 6 -> TIME_LEN_ERROR
    Send Time And Check    1234    -1X

Invalid Non Digit Characters
    # sisaltaa kirjaimen 'A' -> TIME_ARRAY_ERROR
    Send Time And Check    00A120    -2X

Disconnect Serial
    Log To Console    Disconnecting ${board}
    [Teardown]    Delete Port    ${com}

*** Keywords ***
Send Time And Check
    [Arguments]    ${time_string}    ${expected}
    Write Data    ${time_string}X    encoding=ascii
    Log To Console    Sent ${time_string}X
    ${read} =    Read Until    terminator=88    encoding=ascii
    Log To Console    Received ${read}
    Should Be Equal As Strings    ${read}    ${expected}
