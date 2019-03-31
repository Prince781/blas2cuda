#!/bin/env python3

import re
import sys
import argparse

enum_head_pattern = re.compile(r'typedef enum CLBlastStatusCode_ {')
enum_member_pattern = re.compile(r'\s*(\w+)\s*=\s*(-?\d+),\s*//\s*(.*)')
enum_tail_pattern = re.compile(r'}\s*CLBlastStatusCode;')

inside_enum = False

enum_values = {}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0])
    parser.add_argument('header', help='Location of clblast_c.h')

    args = parser.parse_args()

    with open(args.header) as header_file:
        for line in header_file:
            match_enum_member = re.match(enum_member_pattern, line)
            if inside_enum and match_enum_member:
                enum_values[int(match_enum_member.group(2))] = (match_enum_member.group(1), match_enum_member.group(3))

            if re.match(enum_tail_pattern, line):
                inside_enum = False
                break

            if re.match(enum_head_pattern, line):
                inside_enum = True;

    with open('clblast_ext.h', 'w') as header_file:
        print(f'// autogenerated from {sys.argv[0]}', file=header_file)
        print('#ifndef CLBLAST_EXT_H\n#define CLBLAST_EXT_H', file=header_file)
        print('static inline const char *CLBlastGetErrorString(int error) {', file=header_file)
        print('\tswitch (error) {', file=header_file)
        for val,(member,desc) in enum_values.items():
            print(f'\t\tcase {val}: return "{desc}";', file=header_file)
        print('\t\tdefault: return "CLBlastUnknown";', file=header_file)
        print('\t}', file=header_file)
        print('}', file=header_file)
        print('#endif // CLBLAST_EXT_H', file=header_file)

    print('Done. Check clblast_ext.h')
