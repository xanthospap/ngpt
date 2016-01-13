#! /usr/bin/python

import matplotlib.pyplot as plt
import numpy as np
import argparse
import sys, re
from mpl_toolkits.mplot3d import axes3d
from matplotlib import cm
from argparse import RawTextHelpFormatter

plot_options = {'wired': False,  ## plot as wire-frame
                'surfc': True }  ## normal, surface plot

def get_antenna_model(buf):
    """ Extract the antenna model from an input stream.
    """
    dummy_it = 0
    line = buf.readline()
    while line and dummy_it < 100:
        l = line.split()
        if len(l) > 0 and l[0] != '[DEBUG]': break
        else                : line = buf.readline()
        dummy_it += 1
    if dummy_it == 100:
        print >> sys.stderr, '!! ERROR. Failed to read new antenna. !!'
        sys.exit(1)
    l = line.split()
    if l[0] != 'ANT:':
        print >> sys.stderr, '!! ERROR. Invalid \'Ant\' line: !!'
        print >> sys.stderr, '!!        [' + line.strip() + '] !!'
        sys.exit(1)
    return line[5:]

def get_zen_axis(buf):
    """ Extract the zenith distance grid details from an input stream.
    """
    line = buf.readline()
    l    = line.split()
    if l[0] != 'ZEN:':
        print >> sys.stderr, 'ERROR. Invalid \'Zen\' line:'
        print >> sys.stderr, '       [' + line.strip() + ']'
        sys.exit(1)
    zen1, zen2, dzen = map(float, l[1:])
    # return np.arange(zen1, zen2, dzen)
    return zen1, zen2, dzen

def get_azi_axis(buf):
    """ Extract the azimouth grid details from an input stream.
    """
    line = buf.readline()
    l    = line.split()
    if l[0] != 'AZI:':
        print >> sys.stderr, 'ERROR. Invalid \'Azi\' line:'
        print >> sys.stderr, '       [' + line.strip() + ']'
        sys.exit(1)
    azi1, azi2, dazi = map(float, l[1:])
    # return np.arange(azi1, azi2, dazi)
    return azi1, azi2, dazi

def make_2d_plot(antenna, buf, fig_nr, zen1, zen2, dzen):
    pcv = []
    max_lines = int((zen2 - zen1)/dzen)
    idx       = 0
    while idx < max_lines:
        line = buf.readline()
        l    = line.split()
        if len(l) != 1:
            print >> sys.stderr, '!! ERROR. Invalid line PCV line !!',
            print >> sys.stderr, '!!        [' + line.strip() + '] !!'
            raise RuntimeError
        pcv.append(float(l[0]))
        idx += 1
        ##print 'read',idx,'/',max_lines, '(FIG=', fig_nr,')'
    ##  Setup plot env
    ## -------------------------------------------------------------------
    fig = plt.figure(fig_nr)
    fig.suptitle(re.sub(r'\s+', ' ', antenna) + ' (NOAZI)', fontsize=14, fontweight='bold')
    ax  = fig.gca()
    if args.plot_altitude:
        ax.set_xlabel('Altitude ($^\circ$)')
    else:
        ax.set_xlabel('Zenith Distance ($^\circ$)')
    ax.set_ylabel('Pcv (mm)') 
    X = np.arange(zen1, zen2, dzen)
    Y = np.array(pcv)
    ax.set_xlim(zen1-1, zen2+1)  ## zenith
    ax.set_ylim(min(pcv)-1,max(pcv)+1) ## pcv
    ax.plot(X, Y, 'yo-')

    ## Export (if needed)
    ## -------------------------------------------------------------------
    if args.save_format is not None:
        fn = (re.sub(r'\s+', '_', antenna) + '.' + args.save_format).lower()
        fig.savefig(fn, dpi=100, bbox_inches='tight', format=args.save_format)
        print '## Plot exported to:', fn,
    return fig


def make_3d_plot(antenna, buf, fig_nr):
    ## Read limits and pcv
    ## -----------------------------------------------------------------------
    pcv = []
    zen1, zen2, dzen = get_zen_axis(buf)
    azi1, azi2, dazi = get_azi_axis(buf)
    if azi1 == azi2 or dazi == 0:
        return make_2d_plot(antenna, buf, fig_nr, zen1, zen2, dzen)
    ## how many lines should i read ?? TODO limits are not drawn so NOT:
    #max_lines = int((zen2 - zen1)/dzen + 1) * int((azi2 - azi1)/dazi + 1)
    ## BUT
    max_lines = int((zen2 - zen1)/dzen) * int((azi2 - azi1)/dazi)
    idx       = 0
    while idx < max_lines:
        line = buf.readline()
        l    = line.split()
        if len(l) != 1:
            print >> sys.stderr, '!! ERROR. Invalid line PCV line !!',
            print >> sys.stderr, '!!        [' + line.strip() + '] !!'
            raise RuntimeError
        pcv.append(float(l[0]))
        idx += 1
    ##  Setup plot env
    ## -------------------------------------------------------------------
    fig = plt.figure(fig_nr)
    fig.suptitle(re.sub(r'\s+', ' ', antenna), fontsize=14, fontweight='bold')
    ax  = fig.gca(projection='3d')
    if args.plot_altitude:
        ax.set_xlabel('Altitude ($^\circ$)')
    else:
        ax.set_xlabel('Zenith Distance ($^\circ$)')
    ax.set_ylabel('Azimouth ($^\circ$)')
    ax.set_zlabel('Pcv (mm)') 
    ##  Make a (const) zero surface plot
    ##  This does not look good ....
    ##  ------------------------------------------------------------------
    if plot_options['wired'] == True:
        X, Y = np.meshgrid(np.arange(zen1-2*dzen, zen2+2*dzen, dzen),
            np.arange(azi1-2*dazi, azi2+2*dazi, dazi) )
        Z    = np.zeros(shape=X.shape)
        surf = ax.plot_surface(X, Y, Z,
            rstride=5,
            cstride=5,
            cmap=cm.autumn,
            linewidth=0,
            antialiased=False)
    ##  Plot the pcv values
    ##  ------------------------------------------------------------------
    zen  = np.arange(zen1, zen2, dzen)
    azi  = np.arange(azi1, azi2, dazi)
    X, Y = np.meshgrid(zen, azi)
    Z    = np.zeros((len(zen), len(azi)))
    k = 0
    for i in range(0, len(zen)):
        for j in range(0, len(azi)):
            Z[i,j] = pcv[k]
            k += 1
    Z = np.transpose(Z)
    ## Wired-plot
    if plot_options['wired'] == True:
        ax.plot_wireframe(X, Y, Z, rstride=5, cstride=5)
    if plot_options['surfc'] == True:
        ax.plot_surface(X, Y, Z, rstride=5, cstride=5, alpha=0.3)
    ## projections/contours
    cset = ax.contourf(X, Y, Z, zdir='z', offset=2*min(pcv), cmap=cm.coolwarm)
    #cset = ax.contourf(X, Y, Z, zdir='x', offset= -15,         cmap=cm.coolwarm)
    #cset = ax.contourf(X, Y, Z, zdir='y', offset=340,         cmap=cm.coolwarm)
    ##  Re-scale
    ## -----------------------------------------------------------------------
    ax.set_xlim(0, zen2)  ## zenith
    ax.set_ylim(0, azi2) ## azimuth
    ax.set_zlim(2*min(pcv), max(pcv)+1)

    ## Export (if needed)
    ## -------------------------------------------------------------------
    if args.save_format is not None:
        fn = (re.sub(r'\s+', '_', antenna) + '.' + args.save_format).lower()
        fig.savefig(fn, dpi=100, bbox_inches='tight', format=args.save_format)
        print '## Plot exported to:', fn,
    return fig

parser = argparse.ArgumentParser(
    formatter_class = RawTextHelpFormatter,
    description     = ''
    'Utility to plot GNSS antenna Phase Center Variation correction\n'
    'grid(s), as extracted from the ngpt atxtr program.\n'
    'By default, antex-view will read data from STDIN, but you\n'
    'can change this behavior via the \'-f\' option. You can\n'
    'simultaniously plot multiple antenna pcv grids/corrections.\n'
    'The plots will be (interactively) shown when all reading is\n'
    'done; should you want to save them, use the \'-s\' option\n'
    '(if you do not want to see the interactive plots a all, use\n'
    '\'-n\'). Normally, you should be able to save files in all\n'
    'commonly used formats, including:\n'
    '\tpng,\n'
    '\tpdf,\n'
    '\t[e]ps,\n'
    '\tsvg.\n'
    'The format of the input data is strict and even slight changes\n'
    'may cause the program to exit with failure.\n'
    'NOAZI pcv corrections, i.e. antennas with non-azimouth-dependent\n'
    'pcv corrections can (and will) be plotted if the \'AZI\' line is:\n'
    '\t\'AZI: 0 0 0\'.\n'
    'An exit status other than 0 (zero), denotes ERROR.\n\n'
    '---------example input data ---------------------------------------\n'
    'ANT: LEIAR25.R4      NONE\n'
    'ZEN: 0 90 1\n'
    'AZI: 0 360 1\n'
    '0.003804\n'
    '...\n'
    '...\n'
    '---------endof input data -----------------------------------------\n\n'
    'See \'atxtr\' for more info.',
    epilog = ''
    'Copyright 2015 National Technical University of Athens.\n'
    'This work is free. You can redistribute it and/or modify it under the\n'
    'terms of the Do What The Fuck You Want To Public License, Version 2,\n'
    'as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.\n'
    '\nSend bugs to: xanthos[AT]mail.ntua.gr, demanast[AT]mail.ntua.gr'
    )

parser.add_argument('-f', '--file',
    action   = 'store',
    required = False,
    help     = ''
    'The input file containing the PCV corrections.\n'
    'The format of this file is strict; see the\n'
    '\"atx.grd.example\" file for details. Note that\n'
    'more than one antennas PCVs can be plotted in one\n'
    'run. By default, antex-view will read from stdin.',
    metavar  = 'PVC_GRID',
    dest     = 'pcv_grid_file',
    default  = sys.stdin
    )

## TODO implement this
parser.add_argument('-i', '--altitude',
    action = 'store_true',
    help   = ''
    'Instead of plotting the zenith distance (versus\n'
    'azimouth), plot the altitude. Note that the zenith\n'
    'distance is the complement of altitude (i.e.\n'
    '90 - altitude).',
    dest   = 'plot_altitude'
    )

parser.add_argument('-s', '--save-as',
    action   = 'store',
    default  = None,
    help     = ''
    'Export the plot(s) to an output file. Only specify\n'
    'the format (i.e. extension) of the exported file;\n'
    'the name will be automatically deduced using the\n'
    'antenna name/radome/serial, where all whitespaces\n'
    'will be truncated to \'_\'.\n'
    'Supported formats (normally) include:\n'
    'png, pdf, [e]ps, svg.',
    required = False,
    nargs    = '?',
    metavar  = 'SAVE_FORMAT',
    dest     = 'save_format',
    choices  = ['png', 'pdf', 'eps', 'ps', 'svg']
    )

parser.add_argument('-n', '--non-interactive',
    action   = 'store_true',
    help     = ''
    'Do not show (supress) the plot; this can be useful\n'
    'if the user only wants the plot to be saved.',
    dest     = 'no_int_plot'
    )

##  Parse command line arguments
args = parser.parse_args()

FIG_NR      = 0
plots       = []
new_antenna = True

with open(args.pcv_grid_file, 'r') if args.pcv_grid_file is not sys.stdin else sys.stdin as fin:
    while new_antenna:
        try:
            antenna = get_antenna_model(fin)
            print '## Found new antenna:', antenna,
            FIG_NR += 1
            try:
                plots.append( make_3d_plot(antenna, fin, FIG_NR) )
                print '## Compiled plot for:', antenna,
            except:
                print >> sys.stderr, '!! Error compiling plot !!',
                print >> sys.stderr, '!! Unexpected error: ', sys.exc_info()[0]
        except:
            break
            new_antenna = False

if not args.no_int_plot:
    for p in plots: plt.show()