# /// script
# dependencies = ["graphviz"]
# ///

from pathlib import Path
import graphviz
import json


bpf_progs = {}
bpf_maps = {}

"""
This part of the code merges data from multiple files
"""

# Load bpf progs
with open('out07.bpftool-prog-show') as file:
	for prog in json.load(file):
		# Skip bpf programs without a name
		if 'name' not in prog:
			continue

		if 'map_ids' not in prog:
			prog['map_ids'] = []

		bpf_progs[prog['id']] = prog

# Load bpf maps
with open('out06.bpftool-map-show') as file:
	for map in json.load(file):
		if 'contents' not in map:
			map['contents'] = []

		bpf_maps[map['id']] = map

# Load the prog_array map contents
for map_id in bpf_maps:
	filename = f'prog_array_map_{map_id}_contents.json'
	if Path(filename).is_file():
		with open(filename) as file:
			prog_array_map_contents = json.load(file)

			# Maps that no longer exist will return object with error
			if not isinstance(prog_array_map_contents, list):
				continue

			bpf_maps[map_id]['contents'] = [item['formatted'] for item in prog_array_map_contents]

"""
This part of the code draws the graph
"""

dot = graphviz.Digraph(comment='BPF')

for prog in bpf_progs.values():
	# Skip drawing non-XDP programs unless they're referenced by an XDP program, I think?
	if prog['type'] not in ['xdp']:
		continue

	# xdp_main don't seem to be connected to anything else
	# skip to make graph more compact
	if prog['name'] in ['xdp_main']:
		continue

	# Draw the prog node
	dot.node(f'prog_{prog['id']}', f'{prog['name']} (prog_{prog['id']})', style='filled', fillcolor='#40e0d0')

	# Draw references to bpf maps
	for map_id in prog['map_ids']:
		# Draw arrow from prog to each map in map_ids
		dot.edge(f'prog_{prog['id']}', f'map_{map_id}')

		if map_id not in bpf_maps:
			# If we don't have extended info about the map, draw a box with text "<unknown>"
			dot.node(f'map_{map_id}', f'<unknown> (map_{map_id})', style='filled', fillcolor=None, shape='box')
		else:
			map = bpf_maps[map_id]

			# Draw a box with text containing the map type and id and color it pink if it's a prog_array
			dot.node(f'map_{map_id}', f'{map['type']} (map_{map_id})', style='filled', fillcolor='#ff000042' if map['type'] == 'prog_array' else None, shape='box')

			# Draw arrows from the prog_array map back to a prog
			if map['type'] == 'prog_array':
				for item in map['contents']:
					dot.edge(f'map_{map_id}', f'prog_{item['value']}')

print(dot.source)

dot.view()
