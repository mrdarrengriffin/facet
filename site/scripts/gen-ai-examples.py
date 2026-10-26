#!/usr/bin/env python3
"""Writes the "Make screens with AI" example specs (renderer/specs/ai-examples/*.json, one screen spec each, checked by
the spec corpus: node renderer/tools/corpus.mjs --no-gen --extra renderer/specs/ai-examples), their demo Home Assistant
values (renderer/tools/demo_ha_examples.mjs, read by demo_ha.mjs) and the docs page data (site/src/lib/ai-examples.json:
title, what it is for, the prompt that makes it). Each bind's sample "s" = the raw Home Assistant value, so the docs
(fed from the demo values) and the corpus (samples only) draw the same screen. Run: python3 site/scripts/gen-ai-examples.py"""
import json, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
DEMO = {}


def b(e, s, a=None, **kw):
    """a bind on (entity, attribute) with its demo value s"""
    DEMO.setdefault(e, {})[a or ''] = str(s)
    d = {'e': e}
    if a: d['a'] = a
    d.update(kw)
    if len(str(s)) <= 64: d['s'] = str(s)  # a sample is at most 64 characters (the demo value can be longer)
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
    if count is not None: p.update(count=count, tone=tone, zero='muted')
    if meta: p['meta'] = meta
    return {'c': 'section_header', 'p': p}


def check(label, state='open'):
    return {'c': 'checkbox', 'p': {'check': state, 'label': label, 'interactive': True}}


def chip(text, tone='ok', pulse=False):
    return {'c': 'status_chip', 'p': {'tone': tone, 'text': text, 'pulse': pulse}}


EX = []


def ex(id, title, about, prompt, nodes, layout='list', better=None, label=None):
    EX.append({'id': id, 'title': title, 'about': about, 'prompt': prompt, 'better': better or '',
               'spec': {'v': 1, 'id': id, 'name': title, 'rev': 1, 'role': 'page',
                        'header': {'label': label or title, 'back': True}, 'layout': layout, 'nodes': nodes}})


# ---------------------------------------------------------------- the 20 examples
ex('news', 'Headlines', 'The top stories of a news feed, newest first, with the source and how long ago.',
   'Make a Facet screen "Headlines" listing the five newest stories from my news feed sensors (sensor.news_story_1 to 5, the title is the state, the source in the attribute source). Keep each headline on one line.',
   [sec('top stories', meta=b('sensor.news_story_1', '07:40', 'updated')),
    *[{'c': 'list_row', 'p': {'title': b(f'sensor.news_story_{i}', t), 'over': b(f'sensor.news_story_{i}', o, 'source'), 'icon': 'none', 'trailing': 'chevron'}}
      for i, (t, o) in enumerate([('Rail strike called off', 'BBC News · 12 min'), ('Rates held at 4%', 'Reuters · 40 min'),
                                  ('Fen floods ease overnight', 'Look East · 1 h'), ('New Mars rover named', 'BBC Science · 2 h'),
                                  ('Cup draw: home tie for city', 'BBC Sport · 3 h')], 1)]],
   better='Source logos would come with the icon component.')

ex('game_runs', 'Game progress', 'Wins, a streak and the latest runs of a roguelike, from a Steam or save-file sensor.',
   'Make a Facet screen "Game progress" for my roguelike: the hero is my win count (sensor.isaac_wins) of all runs (sensor.isaac_runs), then my win streak, achievements and the last three runs with how they ended.',
   [hero(b('sensor.isaac_wins', '37'), 'runs won', b('sensor.isaac_runs', '112', tpl='of {} runs')),
    sec('progress'),
    row('Win streak', b('sensor.isaac_streak', '4', tpl='{} in a row')),
    row('Achievements', b('sensor.isaac_achievements', '412', tpl='{} of 637')),
    sec('last runs'),
    row("Mom's Heart", 'Won', sub='Judas · 38 min'),
    row('The Lamb', 'Lost', sub='Isaac · 52 min'),
    row('Mega Satan', 'Won', sub='Azazel · 44 min')],
   better='Character portraits and achievement badges would come with the icon component.')

ex('ai_agents', 'AI agents', 'What each AI agent is doing: working, needs you, done or idle, with a mascot that shows it.',
   'Make a Facet screen "AI agents" with one tile per agent (sensor.agent_docs, sensor.agent_firmware, sensor.agent_review, sensor.agent_research): the state is working, needs, done or idle and the attribute status says how far it has got.',
   [{'c': 'tile', 'p': {'title': t, 'kind': b(e, k, map={'working': 'working', 'needs': 'needs', 'done': 'done'}, **{'else': 'idle'}), 'status': b(e, st, 'status')}}
    for t, e, k, st in [('Docs writer', 'sensor.agent_docs', 'working', 'Page 3 of 5'), ('Firmware build', 'sensor.agent_firmware', 'needs', '1 question'),
                        ('Code review', 'sensor.agent_review', 'done', '12 notes'), ('Research', 'sensor.agent_research', 'idle', 'Waiting')]],
   layout='cards', better='A grid layout would let a screen mix agent tiles with a summary.')

ex('shopping', 'Shopping list', 'A tick-off shopping list from a Home Assistant to-do list.',
   'Make a Facet screen "Shopping list" from todo.shopping: a count of what is left, then each item as a checkbox, ticked ones done.',
   [sec('to buy', count=b('todo.shopping', '5'), tone='info'),
    check('Oat milk'), check('Sourdough loaf'), check('Bananas'), check('Washing-up liquid'), check('Coffee beans'),
    check('Eggs', 'done'), check('Tomatoes', 'done')])

ex('medication', 'Medication', "Today's doses, which are taken and when the next one is due.",
   'Make a Facet screen "Medication" for today: the next dose time as the hero (sensor.next_dose), then each dose of the day as a checkbox, the ones taken ticked.',
   [hero(b('sensor.next_dose', '13:00'), 'next dose', b('sensor.next_dose', 'Ibuprofen 200 mg', 'medicine'), accent=True),
    sec('today'),
    check('Vitamin D · 08:00', 'done'), check('Ibuprofen · 13:00'), check('Ibuprofen · 19:00')])

ex('bin_day', 'Bin day', 'Which bins go out next and how many days are left, from a council collection sensor.',
   'Make a Facet screen "Bin day": how many days until the next collection (sensor.bin_days) as the hero with which bins go out, then each bin with its next date.',
   [hero(b('sensor.bin_days', '2'), 'days to bin day', b('sensor.bin_next', 'Recycling and garden · Tue')),
    sec('next collections'),
    row('Recycling', b('sensor.bin_recycling', 'Tue 27 Oct')),
    row('Garden waste', b('sensor.bin_garden', 'Tue 27 Oct')),
    row('General waste', b('sensor.bin_general', 'Tue 3 Nov')),
    row('Food caddy', b('sensor.bin_food', 'Every Tue'))],
   better='Bin colour icons would come with the icon component.')

ex('trains', 'Next trains', 'The next departures from your station with platforms and delays.',
   'Make a Facet screen "Next trains" for my commute (sensor.next_train_1 to 3 from the National Rail integration): the first train as the hero with its platform, then the next departures with on time or the delay.',
   [hero(b('sensor.next_train_1', '07:42'), 'next train', b('sensor.next_train_1', 'Platform 2 · on time', 'summary'), role='display'),
    sec('to cambridge', meta='Ely'),
    {'c': 'list_row', 'p': {'title': '08:12 Cambridge', 'over': 'Platform 1', 'icon': 'none', 'trailing': 'value', 'value': b('sensor.next_train_2', 'On time', 'status')}},
    {'c': 'list_row', 'p': {'title': '08:42 Cambridge', 'over': 'Platform 2', 'icon': 'none', 'trailing': 'value', 'value': b('sensor.next_train_3', '+4 min', 'status')}},
    {'c': 'list_row', 'p': {'title': '09:12 Cambridge', 'over': 'Platform 1', 'icon': 'none', 'trailing': 'value', 'value': 'On time'}}],
   better='A grid would put platform and delay in columns.')

ex('parcels', 'Parcels', 'Deliveries due today and this week, with where each one is.',
   'Make a Facet screen "Parcels" from my parcel tracking sensors: the number arriving today as the hero, then each parcel with its status and when it is due.',
   [hero(b('sensor.parcels_today', '2'), 'arriving today', 'Out for delivery'),
    sec('on the way'),
    row('Running shoes', b('sensor.parcel_shoes', 'By 14:00'), sub='Out for delivery'),
    row('Printer filament', b('sensor.parcel_filament', 'Today'), sub='With the courier'),
    row('Birthday present', b('sensor.parcel_present', 'Thu'), sub='Dispatched'),
    sec('delivered'),
    row('Light bulbs', 'Mon', sub='In the porch')],
   better='Courier logos would come with the icon component.')

ex('energy_price', 'Energy price', 'The electricity price now, the cheap and peak hours of the day and when to run the dishwasher.',
   'Make a Facet screen "Energy price" from my Octopus Agile sensors: the price now as the hero in pence per kWh, the day as cheap / normal / peak hours with now marked, and the cheapest slot.',
   [hero(b('sensor.electricity_price', '13.5', tpl='{}p'), 'per kWh now', 'Normal rate until 16:00'),
    {'c': 'rate_strip', 'p': {'bands': b('sensor.electricity_bands', 'cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,peak,peak,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap'), 'now': 13, 'width': 400}},
    {'c': 'legend', 'p': {'items': 'cheap 9p,normal 14p,peak 31p', 'ticks': '00,06,12,18,24', 'width': 400}},
    sec('cheapest'),
    row('Run the dishwasher', b('sensor.cheapest_slot', '02:00'), sub='9.1p for 3 hours')])

ex('plants', 'Plant care', 'Which plants need water today, by their soil moisture.',
   'Make a Facet screen "Plant care": how many plants need water as the hero, then each plant with its soil moisture and when it needs water.',
   [hero(b('sensor.plants_thirsty', '2'), 'plants to water', 'Fern and basil are dry', accent=True),
    sec('soil moisture'),
    row('Fern', b('sensor.fern_moisture', '18', tpl='{}%'), sub='Water today'),
    row('Basil', b('sensor.basil_moisture', '26', tpl='{}%'), sub='Water today'),
    row('Monstera', b('sensor.monstera_moisture', '46', tpl='{}%'), sub='In 3 days'),
    row('Snake plant', b('sensor.snake_plant_moisture', '91', tpl='{}%'), sub='Fine for 2 weeks')],
   better='The plant gauge, once released, would draw each moisture as a bar.')

ex('chores', 'Chores', "Each child's jobs for today, ticked off as they go.",
   'Make a Facet screen "Chores" for my two kids: how many jobs are left today as the hero, then each job as a checkbox starting with whose job it is, done ones ticked.',
   [hero(b('sensor.chores_left', '3'), 'jobs left', b('sensor.chores_left', 'Mia 1 · Leo 2', 'by_child')),
    sec('today'),
    check('Mia · Pack school bag'), check('Leo · Tidy the lego'), check('Leo · Water the plants'),
    check('Mia · Feed the cat', 'done'), check('Leo · Make the bed', 'done')],
   better='Avatars per child would come with the icon component.')

ex('focus', 'Focus', 'A focus session timer: time left, which session it is and a break after.',
   'Make a Facet screen "Focus" for a pomodoro timer (timer.focus): the time left as the hero, a ring that fills as the session runs, then the session count and what comes next.',
   [hero(b('timer.focus', '18:24', 'remaining'), 'focus left', 'Session 3 of 4', accent=True),
    {'c': 'tick_ring', 'p': {'count': 60, 'range': 360, 'size': 200, 'stride': '5', 'numbers': False, 'lit': b('sensor.focus_progress', '38')}},
    row('Next', 'Break · 5 min'),
    row('Done today', b('counter.focus_sessions', '2', tpl='{} sessions'))])

ex('homelab', 'Homelab', 'Servers and services at a glance: what is up, CPU load and anything that needs a look.',
   'Make a Facet screen "Homelab": how many services are up as the hero with anything that needs a look, each server with its load, then the NAS CPU over the last hour as a small line.',
   [hero(b('sensor.services_up', '11'), 'services up', 'Backup box needs a look'),
    sec('servers'),
    row('NAS', b('sensor.nas_cpu', '41', tpl='{}%'), sub='Up 12 days'),
    row('Proxmox', b('sensor.proxmox_cpu', '18', tpl='{}%'), sub='6 guests running'),
    row('Backup box', 'Check', sub='Last backup 3 days ago'),
    row('Router', b('sensor.router_cpu', '7', tpl='{}%'), sub='Up 41 days'),
    sec('nas cpu', meta='last hour'),
    {'c': 'sparkline', 'p': {'values': b('sensor.nas_cpu_history', '22,25,31,28,35,52,47,39,41'), 'colour': 'accent', 'width': 224, 'dot': True}}])

ex('printer', '3D printer', 'A 3D print under way: how far along, time left and the temperatures.',
   'Make a Facet screen "3D printer" for my Bambu printer: the print progress as the hero with the file name, then time left, layer, nozzle and bed temperature.',
   [hero(b('sensor.printer_progress', '64', tpl='{}%'), 'printed', b('sensor.printer_task', 'Benchy v2 · PLA'), accent=True),
    sec('details'),
    row('Time left', b('sensor.printer_remaining', '1 h 12 min')),
    row('Layer', b('sensor.printer_layer', '148 of 230')),
    row('Nozzle', b('sensor.printer_nozzle', '215', tpl='{}°')),
    row('Bed', b('sensor.printer_bed', '60', tpl='{}°'))])

ex('run_weather', 'Run weather', 'Is it good for a run or a ride: the weather now and sunset.',
   'Make a Facet screen "Run weather" from weather.home: the temperature with a one-line verdict for a run, then wind, rain chance, UV and the next dry window.',
   [hero(b('weather.home', '14', 'temperature', tpl='{}°'), 'good for a run', 'Dry until 18:00, light wind'),
    sec('conditions'),
    row('Wind', b('weather.home', '12', 'wind_speed', tpl='{} km/h')),
    row('Rain', b('sensor.rain_chance', '5', tpl='{}%')),
    row('UV', b('sensor.uv_index', '2', tpl='{} low')),
    row('Sunset', b('sun.sun', '17:41', 'next_setting'))],
   better='A weather icon beside the temperature would fit once a row layout is released.')

ex('guest_wifi', 'Guest Wi-Fi', 'A QR code guests scan to join the Wi-Fi, with the network name and password.',
   'Make a Facet screen "Guest Wi-Fi" with a QR code for my guest network (WIFI:T:WPA;S:Guest;P:<password>;;) that phones can scan, and the name and password written under it.',
   [{'c': 'qr_block', 'p': {'text': 'WIFI:T:WPA;S:Guest;P:sunny-otter-42;;', 'caption': 'Scan to join'}},
    row('Network', 'Guest'),
    row('Password', b('input_text.guest_wifi_password', 'sunny-otter-42'))])

ex('laundry', 'Laundry', 'The washer and dryer: minutes left, the programme and when it ends.',
   'Make a Facet screen "Laundry": the washing machine time left as the hero with its programme and end time, then each machine and the detergent left.',
   [hero(b('sensor.washer_remaining', '38'), 'minutes left', b('sensor.washer_programme', 'Cotton 40° · ends 11:52')),
    sec('machines'),
    row('Washing machine', b('sensor.washer_state', 'Washing'), sub='Started 10:34'),
    row('Dryer', b('sensor.dryer_state', 'Idle'), sub='Last run yesterday'),
    row('Detergent', b('sensor.washer_detergent', 'About 9 washes'))])

ex('air_quality', 'Air quality', 'Indoor air: fine particles, CO2 and humidity, with the trend today.',
   'Make a Facet screen "Air quality" from my indoor air sensor: PM2.5 as the hero with a one-line verdict, then CO2, VOC and humidity, and the PM2.5 trend today as a small line.',
   [hero(b('sensor.living_room_pm25', '8'), 'pm2.5 µg/m³', 'Good · no need to open a window'),
    sec('living room'),
    row('CO2', b('sensor.living_room_co2', '640', tpl='{} ppm')),
    row('VOC', b('sensor.living_room_voc', 'Low')),
    row('Humidity', b('sensor.living_room_rh', '52', tpl='{}%')),
    sec('pm2.5 today'),
    {'c': 'sparkline', 'p': {'values': b('sensor.living_room_pm25_history', '6,7,9,14,12,10,8,7,8'), 'colour': 'steel', 'width': 224, 'dot': True}}])

ex('birthdays', 'Birthdays', 'Days to the next birthday and who comes after, from a calendar.',
   'Make a Facet screen "Birthdays" from calendar.birthdays: days to the next birthday as the hero with who and how old, then the next three.',
   [hero(b('sensor.next_birthday_days', '12'), 'days to go', b('sensor.next_birthday', 'Mia turns 9 · Sat 6 Nov'), accent=True),
    sec('coming up'),
    row('Grandad', '21 Nov', sub='Turns 70'),
    row('Sam', '2 Dec', sub='Turns 34'),
    row('Aunt Jo', '18 Dec', sub='Turns 51')])

ex('fixtures', 'Fixtures', "Your team's next match, the latest result and the league position.",
   'Make a Facet screen "Fixtures" for my football team from the TeamTracker sensor: the next kick-off as the hero, the last result and league position, then the next three fixtures.',
   [hero(b('sensor.team_next_kickoff', 'Sat 15:00'), 'next match', b('sensor.team_next_match', 'v Ipswich · home')),
    sec('form'),
    row('Last result', b('sensor.team_last_result', 'Won 2-1'), sub='v Luton, away'),
    row('League', b('sensor.team_position', '6', tpl='{}th')),
    sec('fixtures'),
    row('Norwich v Ipswich', 'Sat 1 Nov'),
    row('Leeds v Norwich', 'Tue 4 Nov'),
    row('Norwich v Hull', 'Sat 8 Nov')],
   better='Club crests would come with the icon component; a grid would line up scores.')


# ---------------------------------------------------------------- write
os.makedirs(os.path.join(ROOT, 'renderer/specs/ai-examples'), exist_ok=True)
for e in EX:
    with open(os.path.join(ROOT, f"renderer/specs/ai-examples/{e['id']}.json"), 'w') as f:
        f.write(json.dumps(e['spec'], ensure_ascii=False, indent=1) + '\n')
with open(os.path.join(ROOT, 'site/src/lib/ai-examples.json'), 'w') as f:
    json.dump([{k: e[k] for k in ('id', 'title', 'about', 'prompt', 'better')} for e in EX], f, ensure_ascii=False, indent=1)
with open(os.path.join(ROOT, 'renderer/tools/demo_ha_examples.mjs'), 'w') as f:
    f.write('// GENERATED by site/scripts/gen-ai-examples.py: the demo Home Assistant values of the "Make screens with AI" examples\n'
            '// (renderer/specs/ai-examples), the same as each bind\'s sample. Made-up devices, not a real home.\n'
            'export const EXAMPLES = ' + json.dumps(DEMO, ensure_ascii=False, indent=1) + ';\n')
print(len(EX), 'examples')
