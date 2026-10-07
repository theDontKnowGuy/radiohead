"""Compare Fritzing's own exported XML netlist with the source terminal list."""
import csv
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

here = Path(__file__).resolve().parent
expected = defaultdict(set)
nc = set()
refs = set()
for r in csv.DictReader((here.parent / 'connections.csv').open()):
    refs.add(r['Reference'])
    endpoint = (r['Reference'], r['Terminal'])
    if r['Net'] == 'NC':
        nc.add(endpoint)
    else:
        expected[r['Net']].add(endpoint)

actual = []
isolated_nc = set()
for net in ET.parse(here / 'radiohead-relay-power_netlist.xml').getroot().findall('net'):
    endpoints = {(c.find('part').get('label'), c.get('name')) for c in net
                 if c.find('part').get('label') in refs}
    if not endpoints:
        continue
    if endpoints & nc:
        assert len(endpoints) == 1, ('NC terminal was connected', endpoints)
        assert len(net) == 1, ('NC terminal was connected to another part', list(net))
        isolated_nc.update(endpoints)
    else:
        actual.append(endpoints)

assert isolated_nc == nc
assert {frozenset(v) for v in expected.values()} == {frozenset(v) for v in actual}
assert len(actual) == len(expected)
print(f'PASS: {len(expected)} native nets match every required terminal; {len(nc)} NC terminals isolated.')
