#!/usr/bin/env python

import os
import sys
import re

import xml.etree.ElementTree as ET
from optparse import OptionParser

tab = ' ' * 4

dict_ctype = {
    'char*' : 'optarg',
    'char' : '1',
    'int' : 'strtoul(optarg, NULL, 0)',
    'unsigned int' : 'strtoul(optarg, NULL, 0)',
    'double' : 'strtod(optarg, NULL);'
}

dict_has_arg = {
    'no_argument' : '',
    'required_argument' : ':',
    'optional_argument' : '::'
}

codeText = """
#include <getopt.h>

enum {
    @CODE_ENUM
    OPT_HELP
};

typedef struct {
    @CODE_STRUCT
} opts_t;

static struct option long_options[] = {
    @CODE_LONG_OPTIONS
    {0, 0, 0, 0}
};

#define OPTSTRING "@CODE_SHORT_OPTIONS"

static void printUsage ()
{
    @CODE_USAGE
}

static void parseArgs(opts_t *opts, int argc, char **argv)
{
    int opt;
    while ((opt = getopt_long (argc, argv, OPTSTRING, long_options, NULL)) != -1)
        switch (opt) {
            @CODE_PARSE
            case OPT_HELP:
            default:
                printUsage();
                exit(0);
                break;
        }
}

opts_t opts = {
    @CODE_DEFAULT
};
"""

class opt:
    def __init__(self, element):
        self.element = element
    def get(self, o):
        if self.element.find(o) is not None:
            return self.element.find(o).text
        else:
            return None

def enum(name):
    return 'OPT_'+ name.upper()

def getIndent(code):
    global codeText
    m = re.search(r'([ ]*)'+re.escape(code), codeText)
    if m:
        return m.group(1)
    return None

def getShortOption(usage):
    m = re.search('^-(\w),', usage)
    if m:
        return m.group(1)
    return None

def getMaxLen(options, key):
    maxlen = 0
    for o in options:
        val = o.get(key)
        if val and len(val) > maxlen:
            maxlen = len(val)
    return maxlen

def genMaxLen(options, keys):
    maxlen = {}
    for key in keys:
        maxlen[key] = getMaxLen(options, key)
    return maxlen

def genCode_enum(options):
    lines = []
    for o in options:
        name = o.get('name')
        lines.append('%s,' %(enum(name)))
    return '\n'.join(lines)

def genCode_struct(options, maxlen):
    lines = []
    for o in options:
        name = o.get('name')
        ctype = o.get('ctype')
        array = o.get('array')
        paddings = ' ' * (maxlen['ctype'] - len(ctype))
        if array:
            lines.append('%s%s %s[%s];' %(ctype, paddings, name, array))
        else:
            lines.append('%s%s %s;' %(ctype, paddings, name))
        paddings_cnt = ' ' * (maxlen['ctype'] - len('int'))
        lines.append('%s%s %s_cnt;' %('int', paddings_cnt, name))
    return '\n'.join(lines)

def genCode_longOptions(options, maxlen):
    lines = []
    for o in options:
        name = o.get('name')
        has_arg = o.get('has_arg')
        paddings = ' ' * (maxlen['name'] - len(name) + maxlen['has_arg'] - len(has_arg))
        lines.append('{\"%s\",%s %s, 0, %s},' %(name, paddings, has_arg, enum(name)))
    paddings_help = ' ' * (maxlen['name'] - len('help') + maxlen['has_arg'] - len('no_argument'))
    lines.append('{\"%s\",%s %s, 0, %s},' %('help', paddings_help, 'no_argument', 'OPT_HELP'))
    return '\n'.join(lines)

def genCode_shortOptions(options):
    global dict_has_arg
    txt = ''
    for o in options:
        usage = o.get('usage')
        has_arg = o.get('has_arg')
        shortOption = getShortOption(usage)
        if shortOption:
            txt += '%s%s' %(shortOption, dict_has_arg[has_arg])
    return txt

def genCode_usage(options, maxlen):
    lines = []
    for o in options:
        usage = o.get('usage')
        desc = o.get('desc')
        default = o.get('default')
        paddings = ' ' * (maxlen['usage'] - len(usage))
        if default:
            lines.append('printf(\"\\t%s %s: %s (Default: %s)\\n\");' %(usage, paddings, desc, default))
        else:
            lines.append('printf(\"\\t%s %s: %s\\n\");' %(usage, paddings, desc))
    return '\n'.join(lines)

def genCode_parse(options):
    global tab, dict_ctype
    lines = []
    for o in options:
        name = o.get('name')
        ctype = o.get('ctype')
        usage = o.get('usage')
        array = o.get('array')
        shortOption = getShortOption(usage)
        if shortOption:
            lines.append('case \'%s\':' %(shortOption))
        lines.append('case %s:' %(enum(name)))
        if array:
            lines.append('%sif (opts->%s_cnt < %s) opts->%s[opts->%s_cnt] = %s;' %(tab, name, array, name, name, dict_ctype[ctype]))
        else:
            lines.append('%sopts->%s = %s;' %(tab, name, dict_ctype[ctype]))
        lines.append('%sopts->%s_cnt++;' %(tab, name))
        lines.append('%sbreak;' %(tab))
    return '\n'.join(lines)

def genCode_default(options):
    lines = []
    for o in options:
        name = o.get('name')
        ctype = o.get('ctype')
        default = o.get('default')
        if default:
            if ctype == 'char*':
                lines.append('.%s = "%s",' %(name, default))
            else:
                lines.append('.%s = %s,' %(name, default))
    if len(lines) > 0:
        return '\n'.join(lines)
    return '0'

def genCode(root):

    global codeText

    options = []
    for e in root.findall('./options/option'):
        options.append(opt(e))

    maxlen = genMaxLen(options, ['name', 'ctype', 'has_arg', 'usage', 'desc', 'default'])

    enumText = genCode_enum(options).replace('\n', '\n'+getIndent('@CODE_ENUM'))
    codeText = codeText.replace('@CODE_ENUM', enumText)

    structText = genCode_struct(options, maxlen).replace('\n', '\n'+getIndent('@CODE_STRUCT'))
    codeText = codeText.replace('@CODE_STRUCT', structText)

    longOptionsText = genCode_longOptions(options, maxlen).replace('\n', '\n'+getIndent('@CODE_LONG_OPTIONS'))
    codeText = codeText.replace('@CODE_LONG_OPTIONS', longOptionsText)

    shortOptionsText = genCode_shortOptions(options)
    codeText = codeText.replace('@CODE_SHORT_OPTIONS', shortOptionsText)

    usageText = genCode_usage(options, maxlen).replace('\n', '\n'+getIndent('@CODE_USAGE'))
    codeText = codeText.replace('@CODE_USAGE', usageText)

    parseText = genCode_parse(options).replace('\n', '\n'+getIndent('@CODE_PARSE'))
    codeText = codeText.replace('@CODE_PARSE', parseText)

    defaultText = genCode_default(options).replace('\n', '\n'+getIndent('@CODE_DEFAULT'))
    codeText = codeText.replace('@CODE_DEFAULT', defaultText)

    print(codeText)
    return

def main():

    parser = OptionParser()
    parser.add_option("-i", "--input", dest="input")
    parser.add_option("-o", "--output", dest="output")
    (options, args) = parser.parse_args()

    conf = options.input or 'options.xml'
    code = options.output or 'options.h'

    if not os.path.exists(conf):
        print('Not Found : ' + conf)
        return

    tree = ET.parse(conf)
    root = tree.getroot()

    if code != '-':
        sys.stdout = open(code, "w")

    genCode(root)

    return

if __name__ == "__main__":
    main()
