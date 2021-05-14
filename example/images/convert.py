#!/usr/bin/env python
#coding=utf-8
import os
import string

statement = "extern uint8_t FILE_start[] asm(\"_binary_FILE_start\");\nextern uint8_t FILE_end[]   asm(\"_binary_FILE_end\");\n"

def re_file():
    file_list = []
    path = os.getcwd() + "/frames"
    for root, dirs,files in os.walk(path):
        for  name in files:
            fdir = os.path.join(root,name)
            file_list.append(os.path.basename(name))

    return file_list

f = open("frames.h",'w')

file_list = re_file()
file_list.sort(key=lambda x: int(x.split('_')[1][:-4]))

for s in file_list:
    t = s.replace(".", "_")
    res = statement.replace("FILE", t)
    f.write(res)

f.write("\nstatic uint8_t* g_frames[][2]={\n")
for s in file_list:
    print("\"%s\"" % s)
    t = s.replace(".", "_")
    f.write("    %s_start, %s_end,\n" % (t, t))

f.write("};\n\n")

f.close()
