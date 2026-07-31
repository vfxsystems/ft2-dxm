if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "INPUT, OUTPUT and SYMBOL must be set")
endif()

file(READ "${INPUT}" _hex HEX)
string(LENGTH "${_hex}" _hex_len)
math(EXPR _byte_len "${_hex_len} / 2")

string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," _bytes "${_hex}")
string(REGEX REPLACE "((0x[0-9a-fA-F][0-9a-fA-F],){16})" "\\1\n" _bytes "${_bytes}")

file(WRITE "${OUTPUT}"
"#include <stddef.h>

extern \"C\" {
extern const unsigned char ${SYMBOL}[] = {
${_bytes}
};
extern const size_t ${SYMBOL}_len = ${_byte_len};
}
")
