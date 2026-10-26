#!/usr/bin/env python3
"""Writes the "Make screens with AI" example specs (renderer/specs/ai-examples/*.json, one screen spec each, checked by
the spec corpus: node renderer/tools/corpus.mjs --no-gen --extra renderer/specs/ai-examples), their demo Home Assistant
values (renderer/tools/demo_ha_examples.mjs, read by demo_ha.mjs) and the docs page data (site/src/lib/ai-examples.json:
title, what it is for, the prompt that makes it). Each bind's sample "s" = the raw Home Assistant value, so the docs
(fed from the demo values) and the corpus (samples only) draw the same screen. Run: python3 site/scripts/gen-ai-examples.py"""
import json, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
DEMO = {}
REACT = {}


def b(e, s, a=None, **kw):
    """a bind on (entity, attribute) with its demo value s"""
    DEMO.setdefault(e, {})[a or ''] = str(s)
    d = {'e': e}
    if a: d['a'] = a
    cap = kw.pop('cap', 64)
    d.update(kw)
    if len(str(s).encode()) <= cap: d['s'] = str(s)  # a sample is at most 64 characters, 256 for a Grid's items (the demo value can be longer)
    return d


def row(title, value='', sub='', over='', icon='none', trailing=None):
    p = {'title': title, 'icon': icon, 'trailing': trailing or ('value' if value else 'none')}
    if value: p['value'] = value
    if sub: p['sub'] = sub
    if over: p['over'] = over
    return {'c': 'list_row', 'p': p}


def hero(n, label, line='', accent=False, role='hero', label_type='label'):
    p = {'n': n, 'label': label, 'role': role, 'label_type': label_type}
    if line: p['line'] = line
    if accent: p['accent'] = True
    return {'c': 'hero_block', 'p': p}


def sec(title, count=None, tone='info', meta=None):
    p = {'title': title, 'badge': count is not None, 'right': 'meta' if meta else 'none'}
    if count is not None: p.update(count=dict(count, fmt='num') if isinstance(count, dict) else count, tone=tone, zero='muted')
    if meta: p['meta'] = meta
    return {'c': 'section_header', 'p': p}


def check(label, state='open'):
    return {'c': 'checkbox', 'p': {'check': state, 'label': label, 'interactive': True}}


def chip(text, tone='ok', pulse=False):
    return {'c': 'status_chip', 'p': {'tone': tone, 'text': text, 'pulse': pulse}}


EX = []


def ex(id, title, about, prompt, nodes, layout='list', better=None, label=None, react=None, held=None):
    """held: why the example is kept off the docs for now (it uses a component that is not released yet); its spec
    and demo values are still written and checked. react: the docs' fake Home Assistant reactions (site/src/lib/sample-ha.ts): what a tap on this screen changes, as
    rules {ev: event name, is: detail (a regex, whole), set: {"entity|attr": value, "$" = the detail}, add: {"entity|attr": n}}"""
    if react: REACT[id] = react
    EX.append({'id': id, 'title': title, 'about': about, 'prompt': prompt, 'better': better or '', 'held': held,
               'spec': {'v': 1, 'id': id, 'name': title, 'rev': 1, 'role': 'page',
                        'header': {'label': label or title, 'back': True}, 'layout': layout, 'nodes': nodes}})


# ---------------------------------------------------------------- layout helpers
def N(_comp, /, **p):
    """a node of component c with props p (None props dropped)"""
    return {"c": _comp, "p": {k: v for k, v in p.items() if v is not None}}


def grid(kids=(), cell=None, **p):
    """a Grid: static kids go into its cells in order (built with "in" / "slot" when the page is flattened)"""
    return ('grid', N('grid', **p), list(kids), cell)


def page(*parts):
    """flatten nodes and grids into the spec's node list: a grid's kids get "in" = the grid's index, "slot" = n"""
    out = []
    for x in parts:
        if isinstance(x, tuple):
            _, g, kids, cell = x
            if cell: g['cell'] = cell
            gi = len(out); out.append(g)
            for n, k in enumerate(kids):
                k = dict(k); k['in'] = gi; k['slot'] = n; out.append(k)
        else:
            out.append(x)
    return out


def rows(e, a, items, cap=256, **p):
    """real rows: one list_row per item of a JSON list bound to an entity attribute ($item.key fills each row)"""
    return N('list_row', list='stack', items=b(e, json.dumps(items, ensure_ascii=False, separators=(',', ':')), a, cap=cap), **p)


def stat(label, value, tone=None):
    return N('stat', label=label, value=value, tone=tone)


# ---------------------------------------------------------------- the 8 examples (each a different layout; every component used as its own demo uses it)
# One demo clock for every example: the panel's sample time 10:01 (Monday). Every time in the data agrees with it.
STORIES = [{'t': 'Rail strike off', 's': 'BBC · 6m'}, {'t': 'Rates held at 4%', 's': 'PA · 25m'},
           {'t': 'Fen floods ease', 's': 'ITV · 48m'}, {'t': 'Mars rover named', 's': 'BBC · 1h'},
           {'t': 'Winter fair opens', 's': 'BBC · 2h'}, {'t': 'Ely roof saved', 's': 'ITV · 3h'}]
ex('news', 'Headlines', 'The newest stories of a news feed: when it last updated, then one row per story with its source above the title.',
   'Make a Facet screen "Headlines" from my news feed sensor (sensor.news): its attribute updated says when it last updated and its attribute stories is a JSON list of {t: title, s: source and age}. A section header with the update time, then one row per story (up to six), the source above the title.',
   page(N('section_header', title='top stories', badge=False, right='meta', meta=b('sensor.news', '10:00', 'updated', tpl='updated {}')),
        rows('sensor.news', 'stories', STORIES, title='$item.t', over='$item.s', icon='none', trailing='none', rows=6, empty='No new stories')),
   better='A tap on a story does nothing yet: opening the full story is still to come, so the rows have no chevron.')

FACTS = [{'t': 'Win streak', 'v': '4'}, {'t': 'Runs', 'v': '112'}, {'t': 'Characters', 'v': '28 of 34'}, {'t': 'Last run', 'v': 'Won · 38 min'}]
ex('game_runs', 'Game progress', 'How far through a game you are: achievements as the one big number with a progress bar, three facts as rows with their values, then the last run.',
   'Make a Facet screen "Game progress" from my Steam sensor (sensor.isaac_achievements): the achievements unlocked as the big number with the total and the next goal beside it, a progress bar, then one row per fact (attribute facts, a JSON list of {t: name, v: value}) with the value at the right, then the last run as a row.',
   page(hero(b('sensor.isaac_achievements', '412'), 'achievements', line=b('sensor.isaac_achievements', 'of 637 · 225 to go for Dead God', 'line')),
        N('level_bar', value=b('sensor.isaac_achievements', '65', 'percent', fmt='num'), mark=-1, tone='accent', width=0),
        rows('sensor.isaac_achievements', 'facts', FACTS, title='$item.t', value='$item.v', icon='none', trailing='value', rows=4)),
   held='uses level_bar, not released yet')

AG = [('Firmware build', 'sensor.agent_firmware', 'needs', '3 of 4'), ('Docs writer', 'sensor.agent_docs', 'working', '3 of 5'),
      ('Code review', 'sensor.agent_review', 'working', '7 of 12'), ('Test runner', 'sensor.agent_tests', 'working', '18 of 40'),
      ('Media fetch', 'sensor.agent_media', 'done', '4 of 4'), ('House audit', 'sensor.agent_audit', 'idle', 'next 18:00')]
ex('ai_agents', 'AI agents', 'Your AI agents at a glance: how many are working, then a tile per agent with its mascot in the mood of what it is doing and how far it has got.',
   'Make a Facet screen "AI agents": the number of agents working as the big number (sensor.agents_working), then a grid of agent tiles three across for sensor.agent_firmware, sensor.agent_docs, sensor.agent_review, sensor.agent_tests, sensor.agent_media and sensor.agent_audit, the ones that need me first, then working, done and idle (state working, needs, done or idle; attribute status = how far it has got, as "3 of 5").',
   page(hero(b('sensor.agents_working', '3'), 'agents working'),
        grid([N('tile', title=t, kind=b(e, k, map={'working': 'working', 'needs': 'needs', 'done': 'done'}, **{'else': 'idle'}), status=b(e, st, 'status'))
              for t, e, k, st in AG], cols=3, gap='group', aspect='row')),
   better='The tiles have nothing to open yet: a screen per agent is still to come. The order is fixed in the spec: a grid that sorts its tiles by state is still to come.')

BUY = ['Oat milk', 'Sourdough loaf', 'Bananas', 'Coffee beans', 'Cheddar', 'Washing-up liquid']
BASKET = ['Eggs', 'Tomatoes', 'Pasta']
ex('shopping', 'Shopping list', 'A tick-off shopping list from a to-do list: what is left to buy with a count, and what is already in the basket.',
   'Make a Facet screen "Shopping list" from todo.shopping (its state is how many are left; attribute to_buy and attribute basket are JSON lists of item names): a caption with the count left, one checkbox per item to buy, then the ticked ones under "in the basket".',
   page(sec('to buy', meta=b('todo.shopping', str(len(BUY)), tpl='{} left')),
        grid(cell=N('checkbox', check='open', label='$item', interactive=True), cols=1, gap='near', aspect='row',
             items=b('todo.shopping', json.dumps(BUY, ensure_ascii=False, separators=(',', ':')), 'to_buy', cap=256), empty='Nothing left to buy'),
        sec('in the basket'),
        grid(cell=N('checkbox', check='done', label='$item', interactive=True), cols=1, gap='near', aspect='row',
             items=b('todo.shopping', json.dumps(BASKET, ensure_ascii=False, separators=(',', ':')), 'basket', cap=256), empty='Nothing in the basket yet')),
   better='Ticking an item changes the count here, but the to-do list itself is not updated yet: Facet has no to-do action.',
   react=[{'ev': 'toggle', 'is': 'done', 'add': {'todo.shopping': -1}}, {'ev': 'toggle', 'is': 'open', 'add': {'todo.shopping': 1}}])

DOSES = [('08:00 · Vitamin D', 'done'), ('08:00 · Ibuprofen 400 mg', 'done'), ('13:00 · Ibuprofen 400 mg', 'open'), ('20:00 · Ibuprofen 400 mg', 'open')]
ex('medication', 'Medication', "Today's doses: the next one big with what to take, a box to tick for each dose, and how long the pack will last.",
   'Make a Facet screen "Medication": how long until the next dose big (sensor.next_dose attribute in_hours) with its time and the medicine beside it, today\'s four doses as checkboxes, then two rows: tablets left and when to reorder.',
   page(hero(b('sensor.next_dose', '3', 'in_hours', tpl='in {} h'), 'next dose', line=b('sensor.next_dose', '13:00 · Ibuprofen 400 mg', 'what')),
        sec('today', meta=b('sensor.doses_left_today', '2', tpl='{} to take')),
        grid([check(l, s) for l, s in DOSES], cols=1, gap='near', aspect='row'),
        row('Left in pack', value=b('sensor.ibuprofen_left', '14', tpl='{} tablets')),
        row('Reorder by', value=b('sensor.ibuprofen_reorder', 'Wed 28 Oct'))),  # 14 left, 3 a day: lasts to Fri 30 Oct
   react=[{'ev': 'toggle', 'is': 'done', 'add': {'sensor.doses_left_today': -1, 'sensor.ibuprofen_left': -1}},
          {'ev': 'toggle', 'is': 'open', 'add': {'sensor.doses_left_today': 1, 'sensor.ibuprofen_left': 1}}])

ex('laundry', 'Washing machine', 'A wash in progress: how far through it is as a ring centred on the screen, then the stage, the time left, when it finishes and the program.',
   'Make a Facet screen "Washing machine" from sensor.washer (state = the stage; attributes progress 0-100, left, ends, program) a ring filled to the progress, centred in a wide cell, then the stage as a caption, the time left, and when it ends and the program as one quiet line.',
   page(grid([N('arc', value=b('sensor.washer', '62', 'progress', fmt='num'), size=160, knob=False)], cols=1, gap='none', aspect='wide'),
        N('status_block', caption=b('sensor.washer', 'rinse'), value=b('sensor.washer', '38 min left', 'left'), line=b('sensor.washer', 'done at 10:39', 'ends'),
          line2=b('sensor.washer', 'Cotton 40° · 1200 rpm', 'program'), tone='dim', size='value')),
   held='uses status_block, not released yet')

PRICES = '9,9,8,8,9,9,14,15,19,21,15,14,13,13,14,15,30,33,31,16,14,13,9,9'  # every bar agrees with its band (cheap 00-06 + 22-24, peak 16-19, normal between)
ex('energy_price', 'Energy price', "The electricity price now as the big number, today's 24 hourly prices as bars coloured cheap to peak with now marked, and the cheapest and dearest times.",
   'Make a Facet screen "Energy price" from my Octopus Agile sensors: the price now big (sensor.electricity_price, pence per kWh) with the band beside it, a full-width bar chart of the 24 hourly prices (attribute today, a comma list) coloured by band with now (attribute hour) marked, then two rows: the cheapest slot and the peak.',
   page(hero(b('sensor.electricity_price', '15', tpl='{}p'), 'per kwh now', line=b('sensor.electricity_price', 'Normal · peak from 16:00', 'band_line')),
        N('bar_chart', values=b('sensor.electricity_price', PRICES, 'today'), colour='bands', ticks=6, now=b('sensor.electricity_price', '10', 'hour', fmt='num'), width=0, height=136, scrub=False),
        row('Cheapest', value=b('sensor.electricity_price', '02:00 · 8p', 'cheapest')),
        row('Peak', value=b('sensor.electricity_price', '17:00 · 33p', 'peak'))),
   better='The chart has no hour labels under it yet, and reading a bar by pressing it would say kWh, not pence: it is left off here.')

LIGHTS = [('Ceiling', 'light.lounge_ceiling', 'on', 204), ('Sofa lamp', 'light.lounge_sofa_lamp', 'on', 102),
          ('Reading', 'light.lounge_reading_lamp', 'off', 0), ('Shelves', 'light.lounge_shelves', 'off', 0),
          ('TV glow', 'light.lounge_tv_glow', 'on', 64), ('Fireplace', 'light.lounge_fireplace', 'off', 0)]
def ltile(label, e, st, bri):
    return {'c': 'entity_tile', 'p': {'icon': 'bulb', 'label': label, 'act': 'toggle', 'on': b(e, st, fmt='on'),
                                      'status': b(e, st, map={'on': 'On', 'off': 'Off'}), 'state': b(e, st, fmt='avail')},
            'on': {'tap': {'do': 'light.toggle', 'e': e}}}
ex('lights', 'Living room lights', 'Every light in the room as a tile: a lit one shows its bulb and state in the accent colour, a tap switches it; the ceiling dimmer below.',
   'Make a Facet screen "Living room lights": a three-column grid of tiles for light.lounge_ceiling, light.lounge_sofa_lamp, light.lounge_reading_lamp, light.lounge_shelves, light.lounge_tv_glow and light.lounge_fireplace (a tap toggles each), then a full-width slider for the ceiling light\'s brightness.',
   page(grid([ltile(*l) for l in LIGHTS], cols=3, gap='group', aspect='row'),
        sec('ceiling brightness', meta=b('light.lounge_ceiling', '80', 'brightness_pct', tpl='{}%')),
        {'c': 'slider', 'p': {'value': b('light.lounge_ceiling', '80', 'brightness_pct', fmt='num'), 'min': 0, 'max': 100, 'step': 5, 'width': 0},
         'on': {'change': {'do': 'light.brightness', 'e': 'light.lounge_ceiling'}}}))


# ---------------------------------------------------------------- write
os.makedirs(os.path.join(ROOT, 'renderer/specs/ai-examples'), exist_ok=True)
for e in EX:
    with open(os.path.join(ROOT, f"renderer/specs/ai-examples/{e['id']}.json"), 'w') as f:
        f.write(json.dumps(e['spec'], ensure_ascii=False, indent=1) + '\n')
with open(os.path.join(ROOT, 'site/src/lib/ai-examples.json'), 'w') as f:
    json.dump([{k: e[k] for k in ('id', 'title', 'about', 'prompt', 'better')} for e in EX if not e['held']], f, ensure_ascii=False, indent=1)
with open(os.path.join(ROOT, 'renderer/tools/demo_ha_examples.mjs'), 'w') as f:
    f.write('// GENERATED by site/scripts/gen-ai-examples.py: the demo Home Assistant values of the "Make screens with AI" examples\n'
            '// (renderer/specs/ai-examples), the same as each bind\'s sample. Made-up devices, not a real home.\n'
            'export const EXAMPLES = ' + json.dumps(DEMO, ensure_ascii=False, indent=1) + ';\n'
            '// what a tap on each example changes in the docs\' fake Home Assistant (site/src/lib/sample-ha.ts), by screen id\n'
            'export const REACTIONS = ' + json.dumps(REACT, ensure_ascii=False, indent=1) + ';\n')
print(len(EX), 'examples,', sum(1 for e in EX if e['held']), 'held off the docs')
