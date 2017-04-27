#!/bin/python

import sys
import json
from collections import OrderedDict

def main():

    probe_ring_map = OrderedDict()
    probe_mux_map = OrderedDict()
    
    with open('multiplexer-probe-map.md') as f:

        mux_id = None
        probe_id = None
        chan_id = None

        for line in f.read().split('\n'):

            if len(line) == 0:
                continue

            if line.startswith('##'):
                print line
                mux_id = int(line.split('ux')[1])
                probe_mux_map[mux_id] = OrderedDict()
            
            elif line.startswith('ch'):
                chan_id = line.split('-')[0].strip()
                probe_id = line.split('-')[1].strip()

                probe_mux_map[mux_id][chan_id] = probe_id

    with open('probe-map-check.md') as f:

        probe_id = None
        probe_label = None

        for line in f.read().split('\n'):

            if len(line) == 0:
                continue

            if line.startswith('UW'):
                probe_id = line.split(' - ')[0]
                probe_label = line.split(' - ')[1]
                probe_ring_map[probe_id] = probe_label
            
    for key in probe_mux_map.keys():
        print 'key: ', key
        print probe_mux_map[key]

    for key in probe_ring_map.keys():
        print 'key: ', key
        print probe_ring_map[key]

    # Generate a probe location/index map.
    idx = 0
    probe_labels = OrderedDict()

    for y in [chr(i) for i in xrange(ord('A'), ord('A')+12)]:
        for l in ['T', 'B']:
            for a in [str(i) for i in xrange(1, 7)]:
                for r in ['I', 'M', 'O']:

                    key = '-'.join([y, l, a, r])

                    if key in probe_ring_map.values():
                        probe_labels[key] = idx
                        idx += 1

                    else:
                        print key

    # Write the odbedit sequence config file.
    f = open('25-nmr-sequence.cmd', 'w')
    json_seq = {}

    for mux_idx in xrange(1, 21):

        mux_key = 'mux_%02i' % mux_idx
        odb_path = '/Settings/NMR Sequence/Fixed Probes/%s' % mux_key

        f.write('mkdir "%s"\n' % odb_path)
        f.write('cd "%s"\n' % odb_path)
        f.write('\n')

        json_seq[mux_key] = {}

        print probe_mux_map[mux_idx]
        for key in probe_mux_map[mux_idx].keys():
            
            ch = int(key[-2:])

            f.write('mkdir "%i"\n' % ch)
            f.write('create INT "%i/fixed"\n' % ch)

            probe_id = probe_mux_map[mux_idx]['ch%02i' % ch]
            probe_loc = probe_ring_map[probe_id]
            probe_idx = probe_labels[probe_loc]

            f.write('set "%i/fixed" %i\n' % (ch, probe_idx))

            json_seq[mux_key][ch] = {'fixed': probe_idx}
        
        f.write('\n')

    f.close()

    # And write the json sequence.
    f = open('fixed-probe-sequence.json', 'w')
    json.dump(json_seq, f, sort_keys=True, indent=2)

if __name__ == '__main__':
    sys.exit(main())
