import re

pattern = re.compile("\\.ri-(\\S+):.*content: \\\"\\\\(\\w+)\\\"")

print("css file name:")
cssFilePath = input()

print("output header name:")
outputHeaderPath = input()

with open(outputHeaderPath, "w") as outputHeaderFile:
    with open(cssFilePath, "r") as cssFile:
        for line in cssFile:
            match = pattern.search(line)
            if not match == None:
                groups = match.groups()
                iconName = "ICON_" + groups[0].replace("-", "_").upper()
                utf8Bytes = bytes(chr(int(groups[1], 16)), "UTF-8")
                outputHeaderFile.write("const char* const ")
                outputHeaderFile.write(iconName)
                outputHeaderFile.write(" = \"")
                for byte in utf8Bytes:
                    hexStr = "{0:x}".format(byte)
                    outputHeaderFile.write("\\x" + hexStr)
                outputHeaderFile.write("\";\n")