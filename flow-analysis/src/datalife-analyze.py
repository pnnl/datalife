#! /usr/bin/env python
# -*-Mode: python;-*-

# $HeadURL: https:// $
# $Id$

#****************************************************************************
#
#****************************************************************************

import sys
import argparse

from datalife.analyze import DataLife
from datalife.sankeydata import SankeyData

datalife_help = '''
datalife-analyze produces data flow lifecycle (DFL) graph to guide
decisions regarding coordinating tasks and data flows on distributed
resources.
'''


def main():

    parser = argparse.ArgumentParser(description=datalife_help,
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-i', '--input', help='read I/O monitor stats from directory path')
    parser.add_argument('-o', '--output', help='write a graph output to a file')
    args = parser.parse_args()
   
    global dlife

    dlife = DataLife()
    if args.input:
        read_stats(args.input)
    else:
        parser.print_help(sys.stderr)
        return
    write_graph(args.output) if args.output else ''


def read_stats(dirpath):

    dlife.read_stats(dirpath)
    msg = dlife.get_stat_summary()
    print(msg)


def write_graph(dirpath):

    dlife.write_graph(dirpath)


def build_sankey():

    sd = SankeyData()
    sd.load_stat_all(dlife.get_df_all())
    sd.build_links()
    sd.plot()


#****************************************************************************

if (__name__ == "__main__"):
    sys.exit(main())

#****************************************************************************
