
# Generates a .c file containing the binary contents of a given file
# @param IN_FILE full path to the input file
# @param OUT_FILE full path to the output .c file

get_filename_component(SRC_FILE ${IN_FILE} NAME)
string(REGEX REPLACE "\\.| |-" "_" SYMBOL_NAME ${SRC_FILE})

file(READ ${IN_FILE} FILEDATA HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FILEDATA ${FILEDATA})
file(WRITE ${OUT_FILE} "const unsigned char _binary_${SYMBOL_NAME}[] = {${FILEDATA}};\n")
file(APPEND ${OUT_FILE} "const unsigned long _binary_${SYMBOL_NAME}_size = sizeof(_binary_${SYMBOL_NAME});\n")

