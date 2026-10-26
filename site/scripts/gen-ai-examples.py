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
    cap = kw.pop('cap', 64)
    d.update(kw)
    if len(str(s)) <= cap: d['s'] = str(s)  # a sample is at most 64 characters, 256 for a Grid's items (the demo value can be longer)
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


# ---------------------------------------------------------------- the 20 examples (each leans on a different family)
ex('news', 'Headlines', 'The newest stories of a news feed as real rows, one per story in the feed, with the source and how long ago.',
   'Make a Facet screen "Headlines" from my news feed sensor (sensor.news): its attribute stories is a JSON list of {t: title, s: source}. Show a count of new stories, then one row per story with the source above the title.',
   page(N('status_chip', tone='info', text=b('sensor.news', 'Updated 07:40', 'updated'), pulse=False),
        sec('top stories', count=b('sensor.news', '4'), tone='info'),
        rows('sensor.news', 'stories', [{'t': 'Rail strike called off', 's': 'BBC · 12 min'}, {'t': 'Rates held at 4%', 's': 'Reuters · 40 min'},
                                        {'t': 'Fen floods ease overnight', 's': 'Look East · 1 h'}, {'t': 'New Mars rover named', 's': 'BBC · 2 h'}],
             title='$item.t', over='$item.s', icon='none', trailing='chevron', rows=6, empty='No new stories')),
   better='Source logos need an image component.')

ex('game_runs', 'Game progress', 'How a roguelike is going: a ring of runs won, lost and quit, the streak and the latest runs.',
   'Make a Facet screen "Game progress" for my roguelike (sensor.isaac): a ring split into runs won, lost and quit with a key, four facts (streak, achievements, best time, runs), then the last runs as rows from the attribute last_runs.',
   page(grid([N('donut', a=b('sensor.isaac', '3', 'won_share'), b=b('sensor.isaac', '6', 'lost_share'), c=b('sensor.isaac', '1', 'quit_share'), size=128),
              N('legend', items=b('sensor.isaac', 'won 37,lost 68,quit 7', 'key'), ticks='', width=200)], cols=2, gap='group', aspect='row'),
        grid([stat('streak', b('sensor.isaac', '4', 'streak'), 'ok'), stat('achievements', b('sensor.isaac', '412', 'achievements', tpl='{}/637')),
              stat('best time', b('sensor.isaac', '21:08', 'best_time')), stat('runs', b('sensor.isaac', '112', 'runs'))], cols=2, gap='group', aspect='row'),
        sec('last runs'),
        rows('sensor.isaac', 'last_runs', [{'b': "Mom's Heart", 'c': 'Judas · 38 min', 'r': 'Won'}, {'b': 'The Lamb', 'c': 'Isaac · 52 min', 'r': 'Lost'},
                                           {'b': 'Mega Satan', 'c': 'Azazel · 44 min', 'r': 'Won'}],
             title='$item.b', sub='$item.c', value='$item.r', icon='none', trailing='value', rows=3, empty='No runs yet')),
   better='The ring has no value in its middle and takes parts 0 to 10, not raw counts.')

ex('ai_agents', 'AI agents', 'What each AI agent is doing, as a tile with a mascot per agent, and a tally of who needs you.',
   'Make a Facet screen "AI agents": status chips for how many agents are working and need me, then a two-column grid with one tile per agent (sensor.agent_docs, sensor.agent_firmware, sensor.agent_review, sensor.agent_research): the state is working, needs, done or idle and the attribute status says how far it has got.',
   page(grid([N('status_chip', tone='working', text=b('sensor.agents_working', '1', tpl='{} working'), pulse=True),
              N('status_chip', tone='warn', text=b('sensor.agents_needs', '1', tpl='{} needs you')),
              N('status_chip', tone='ok', text=b('sensor.agents_done', '1', tpl='{} done'))], cols=3, gap='near', aspect='row'),
        grid([N('tile', title=t, kind=b(e, k, map={'working': 'working', 'needs': 'needs', 'done': 'done'}, **{'else': 'idle'}), status=b(e, st, 'status'))
              for t, e, k, st in [('Docs writer', 'sensor.agent_docs', 'working', 'Page 3 of 5'), ('Firmware build', 'sensor.agent_firmware', 'needs', '1 question'),
                                  ('Code review', 'sensor.agent_review', 'done', '12 notes'), ('Research', 'sensor.agent_research', 'idle', 'Waiting')]],
             cols=2, gap='group', aspect='row')))

ex('shopping', 'Shopping list', 'A tick-off shopping list from a Home Assistant to-do list.',
   'Make a Facet screen "Shopping list" from todo.shopping: a count of what is left, then each item as a checkbox, ticked ones done.',
   page(sec('to buy', count=b('todo.shopping', '5'), tone='info'),
        check('Oat milk'), check('Sourdough loaf'), check('Bananas'), check('Washing-up liquid'), check('Coffee beans'),
        sec('in the basket'), check('Eggs', 'done'), check('Tomatoes', 'done')),
   better='The checkboxes are written out: a to-do list bound as real rows (one checkbox per item) is still to come.')

ex('hot_water', 'Hot water', 'The hot water tank: its temperature on a dial, the mode and what heats it.',
   'Make a Facet screen "Hot water" for water_heater.tank: a dial with the tank temperature from 40 to 65°, a three-way mode switch (off, auto, boost) and two facts: what heats it now and the next boost.',
   page(N('dial', value=b('water_heater.tank', '52', 'current_temperature'), min=40, max=65, step=1, unit='°', caption='Tank', on=True),
        N('segmented', options='Off,Auto,Boost', selected=b('water_heater.tank', 'auto', 'operation_mode'), width='fill'),
        grid([stat('heated by', b('sensor.tank_source', 'Solar'), 'ok'), stat('next boost', b('sensor.tank_next_boost', '17:00'))], cols=2, gap='group', aspect='row')),
   better='The dial and mode switch show the tank but cannot set it yet: their actions are for lights and the vacuum only.')

ex('bin_day', 'Bin day', 'Which bins go out next, a reminder chip on the night and each bin with its next date.',
   'Make a Facet screen "Bin day": a big count of days until the next collection (sensor.bin_days) with a chip that pulses on the night before, then a grid of each bin with its next date.',
   page(N('big_number', label='days to bin day', value=b('sensor.bin_days', '1'), unit='', decimals=0),
        N('status_chip', tone=b('sensor.bin_days', '1', map={'0': 'warn', '1': 'warn'}, **{'else': 'idle'}),
          text=b('sensor.bin_next', 'Recycling and garden out tonight'), pulse=True),
        sec('next collections'),
        grid([stat('recycling', b('sensor.bin_recycling', 'Tue 27'), 'ok'), stat('garden', b('sensor.bin_garden', 'Tue 27'), 'ok'),
              stat('general', b('sensor.bin_general', 'Tue 3')), stat('food', b('sensor.bin_food', 'Every Tue'))], cols=2, gap='group', aspect='row')),
   better='Bin colour icons need more icons in the set.')

ex('trains', 'Next trains', 'The next departure as a big time, its platform, then the trains after it as real rows.',
   'Make a Facet screen "Next trains" for my commute (sensor.next_trains from the National Rail integration): the first departure time big with a chip for its platform and delay, then the next departures from the attribute departures, a JSON list of {t: time and destination, p: platform, d: on time or the delay}.',
   page(hero(b('sensor.next_trains', '07:42', 'first_time'), 'next train', role='display'),
        N('status_chip', tone=b('sensor.next_trains', 'on time', 'first_status', map={'on time': 'ok'}, **{'else': 'warn'}),
          text=b('sensor.next_trains', 'Platform 2 · on time', 'first_line')),
        sec('to cambridge', meta='from Ely'),
        rows('sensor.next_trains', 'departures', [{'t': '08:12 Cambridge', 'p': 'Platform 1', 'd': 'On time'}, {'t': '08:42 Cambridge', 'p': 'Platform 2', 'd': '+4 min'},
                                                  {'t': '09:12 Cambridge', 'p': 'Platform 1', 'd': 'On time'}],
             title='$item.t', over='$item.p', value='$item.d', icon='none', trailing='value', rows=4, empty='No trains')),
   better='The big time is a fixed value: a countdown to the departure is still to come.')

ex('front_door', 'Front door', 'The front door at a glance: open or shut, someone there, the porch temperature and the porch light.',
   'Make a Facet screen "Front door" with state readouts for binary_sensor.front_door (open or closed and since when), binary_sensor.porch_person (someone at the door) and sensor.porch_temperature, then a switch row for light.porch.',
   page(N('readout', label='Front door', icon='door', state=b('binary_sensor.front_door', 'off', map={'on': 'Open', 'off': 'Closed'}), detail=b('binary_sensor.front_door', 'since 18:02', 'since'), tone='text'),
        N('readout', label='Porch', icon='person', state=b('binary_sensor.porch_person', 'on', map={'on': 'Visitor', 'off': 'Clear'}), detail='now',
          tone='text'),
        N('readout', label='Outside', icon='thermometer', state=b('sensor.porch_temperature', '9', tpl='{}°'), detail='feels like 6°', tone='text'),
        sec('porch'),
        N('list_row', title='Porch light', icon='bulb', trailing='toggle', on=b('light.porch', 'on', map={'on': '1'}, **{'else': '0'}))),
   better='The door cannot be locked from here: the lock parts are still in progress.')

ex('energy_price', 'Energy price', "The electricity price now as a big number and today's prices as a bar chart, coloured cheap to peak.",
   'Make a Facet screen "Energy price" from my Octopus Agile sensors: the price now big in pence per kWh, a bar chart of the 24 hourly prices coloured by band with now marked, a key and the cheapest slot.',
   page(N('big_number', label='pence per kwh now', value=b('sensor.electricity_price', '13.5'), unit='', prefix='', decimals=1),
        N('bar_chart', values=b('sensor.electricity_prices', '9,9,8,8,9,9,14,15,29,31,15,14,13,13,14,15,30,33,31,16,14,13,9,9'), colour='bands', now=13, width=400, height=96),
        N('legend', items='cheap 9p,normal 14p,peak 31p', ticks='00,06,12,18,24', width=400),
        grid([stat('cheapest', b('sensor.cheapest_slot', '02:00'), 'ok'), stat('peak from', '16:00', 'warn')], cols=2, gap='group', aspect='row')))

ex('plants', 'Plant care', 'Soil moisture of each plant as a row of gauges, with the ones to water today called out.',
   'Make a Facet screen "Plant care": a chip for how many plants need water, then four gauges side by side for the soil moisture of the fern, basil, monstera and snake plant, each with its name under it, the dry ones in amber.',
   page(N('status_chip', tone='warn', text=b('sensor.plants_thirsty', '2', tpl='{} plants to water today')),
        grid([N('arc', value=b(f'sensor.{p}_moisture', v), size=80) for p, v in [('fern', '18'), ('basil', '26'), ('monstera', '46'), ('snake_plant', '91')]]
             + [N('label', text=n, type='meta', colour=t) for n, p, v, t in [('Fern', 'fern', '18', 'warn'), ('Basil', 'basil', '26', 'warn'),
                                                                                    ('Monstera', 'monstera', '46', 'text-dim'), ('Snake', 'snake_plant', '91', 'ok')]],
             cols=4, gap='near', aspect='row')),
   better='A gauge with its value and name inside it (a cell made of more than one part) would make each plant one cell.')

ex('kitchenradio', 'Kitchen radio', 'What the kitchen speaker is playing, drawn as a turning cassette, with the controls and volume.',
   'Make a Facet screen "Kitchen radio" for media_player.kitchen: a cassette that turns while it plays with the title and artist on its label, previous / play / next buttons and a volume slider.',
   page(N('media_visual', visual='cassette', playing=b('media_player.kitchen', 'playing', map={'playing': '1'}, **{'else': '0'}),
          progress=b('media_player.kitchen', '42', 'media_progress'), title=b('media_player.kitchen', 'Harvest Moon', 'media_title'),
          artist=b('media_player.kitchen', 'Neil Young', 'media_artist')),
        grid([N('icon_button', style='round', icon='prev'), N('icon_button', style='play', icon='play'), N('icon_button', style='round', icon='next')], cols=3, gap='group', aspect='row'),
        N('slider', value=b('media_player.kitchen', '35', 'volume_pct'), min=0, max=100, step=5, width=288)),
   better='The buttons and slider show the player but cannot control it yet: media actions are still to come.')

ex('focus', 'Focus', 'A focus timer: the time left big, a dot per session of the day, the session length and start / skip.',
   'Make a Facet screen "Focus" for a pomodoro timer (timer.focus): the time left big, a row of four session icons (done ones ticked), a three-way choice of focus, short or long break, and start and skip buttons.',
   page(hero(b('timer.focus', '18:24', 'remaining'), 'focus left', role='display', accent=True),
        grid([], cell=N('icon', name='$item', fit='cell', tone='accent'), cols=4, gap='near', aspect='row', empty='No sessions yet', items=b('sensor.focus_sessions', 'check,check,minus,minus')),
        N('segmented', options='Focus,Short,Long', selected=b('input_select.focus_mode', 'focus'), width='fill'),
        grid([N('button', kind='primary', label='Pause', width=0), N('button', kind='secondary', label='Skip', width=0)], cols=2, gap='group', aspect='row')),
   better='The big time is fixed text from the timer: a live countdown is still to come.')

ex('homelab', 'Homelab', 'Servers at a glance: a chip per server, the NAS load as a line and its CPU, memory and disk.',
   'Make a Facet screen "Homelab": a grid of status chips, one per server (NAS, Proxmox, Backup box, Router), the backup box one in warning, then the NAS CPU over the last hour as a line and four facts: CPU, memory, disk and uptime.',
   page(grid([N('status_chip', tone=b(f'binary_sensor.{e}', s, map={'on': 'ok'}, **{'else': 'warn'}), text=t, pulse=(s == 'off'))
              for e, s, t in [('nas', 'on', 'NAS'), ('proxmox', 'on', 'Proxmox'), ('backup', 'off', 'Backup box'), ('router', 'on', 'Router')]],
             cols=2, gap='near', aspect='row'),
        sec('nas cpu', meta='last hour'),
        N('sparkline', values=b('sensor.nas_cpu_history', '22,25,31,28,35,52,47,39,41'), colour='accent', width=224, dot=True),
        grid([stat('cpu', b('sensor.nas_cpu', '41', tpl='{}%')), stat('memory', b('sensor.nas_memory', '63', tpl='{}%')),
              stat('disk', b('sensor.nas_disk', '78', tpl='{}%'), 'warn'), stat('uptime', b('sensor.nas_uptime', '12', tpl='{} days'))], cols=2, gap='group', aspect='row')))

ex('printer', '3D printer', 'A 3D print under way: a dial-like ring of progress, the percentage and the temperatures.',
   'Make a Facet screen "3D printer" for my Bambu printer: a 270° tick ring lit to the print progress with the percentage big under it, then a grid of time left, layer, nozzle and bed temperature, and pause and stop buttons.',
   page(N('tick_ring', count=60, range=270, size=192, stride='5', numbers=False, lit=b('sensor.printer_progress', '64')),
        N('big_number', label=b('sensor.printer_task', 'Benchy v2 · PLA'), value=b('sensor.printer_progress', '64'), unit='%', decimals=0),
        grid([stat('time left', b('sensor.printer_remaining', '1 h 12 min')), stat('layer', b('sensor.printer_layer', '148/230')),
              stat('nozzle', b('sensor.printer_nozzle', '215', tpl='{}°')), stat('bed', b('sensor.printer_bed', '60', tpl='{}°'))], cols=2, gap='group', aspect='row'),
        grid([N('button', kind='secondary', label='Pause', width=0), N('button', kind='outline', label='Stop', width=0)], cols=2, gap='group', aspect='row')),
   better='The percentage sits under the ring: a value inside a ring needs a stacked layout.')

ex('run_weather', 'Run weather', 'Is it good for a run or a ride: a big weather icon, the temperature, a verdict and the conditions.',
   'Make a Facet screen "Run weather" from weather.home: the weather icon big beside the temperature, a verdict chip, a run / ride choice, then wind, rain chance, UV and sunset.',
   page(grid([N('weather_icon', icon=b('weather.home', 'partlycloudy', map={'sunny': 'clear-day', 'partlycloudy': 'partly-day', 'cloudy': 'cloudy', 'rainy': 'rain-light', 'pouring': 'rain-heavy', 'fog': 'fog'}, **{'else': 'cloudy'}),
                              size='card', animated=True),
              N('big_number', label='degrees outside', value=b('weather.home', '14', 'temperature'), unit='', decimals=0)], cols=2, gap='group', aspect='row'),
        N('status_chip', tone='ok', text=b('sensor.run_verdict', 'Good for a run · dry until 18:00')),
        N('segmented', options='Run,Ride', selected=b('input_select.activity', 'run'), width='fill'),
        grid([stat('wind', b('weather.home', '12', 'wind_speed', tpl='{} km/h')), stat('rain', b('sensor.rain_chance', '5', tpl='{}%'), 'ok'),
              stat('uv', b('sensor.uv_index', '2', tpl='{} low')), stat('sunset', b('sun.sun', '17:41', 'next_setting'))], cols=2, gap='group', aspect='row')))

ex('guest_wifi', 'Guest Wi-Fi', 'A QR code guests scan to join the Wi-Fi, with the network name and password beside it.',
   'Make a Facet screen "Guest Wi-Fi" with a QR code for my guest network (WIFI:T:WPA;S:Guest;P:<password>;;) that phones can scan, and the name and password written under it.',
   page(N('qr_block', text='WIFI:T:WPA;S:Guest;P:sunny-otter-42;;', caption='Scan to join'),
        grid([stat('network', 'Guest'), stat('password', b('input_text.guest_wifi_password', 'sunny-otter-42'))], cols=2, gap='group', aspect='row')),
   )

ex('laundry', 'Laundry', 'The washer and dryer as tiles you can tap, and a card for when the wash ends.',
   'Make a Facet screen "Laundry": a tile each for the washing machine and the dryer (lit while running, with the time left), then a card for when the wash ends and its programme.',
   page(grid([N('entity_tile', icon='power', label='Washer', status=b('sensor.washer_remaining', '38', tpl='{} min left'), on=b('sensor.washer_state', 'Washing', map={'Washing': '1'}, **{'else': '0'}), act='none'),
              N('entity_tile', icon='power', label='Dryer', status=b('sensor.dryer_state', 'Idle'), on=b('sensor.dryer_state', 'Idle', map={'Drying': '1'}, **{'else': '0'}), act='none')],
             cols=2, gap='group', aspect='row'),
        N('card', label='Wash ends', icon='timer', size='l', mode='value', value=b('sensor.washer_end', '11:52'), sub=b('sensor.washer_programme', 'Cotton 40° · 1400 spin'), tone='dim')),
   better='Tiles and cards take a few icons only: the icon set has a washer, the tile cannot use it yet.')

ex('air_quality', 'Air quality', 'Indoor air: fine particles big with a verdict, the trend today as a line and CO2, VOC and humidity.',
   'Make a Facet screen "Air quality" from my indoor air sensor: PM2.5 big with a verdict chip, the PM2.5 trend today as a line with times under it, then CO2, VOC, humidity and temperature in a grid.',
   page(N('big_number', label='pm2.5 µg/m³', value=b('sensor.living_room_pm25', '8'), unit='', decimals=0),
        N('status_chip', tone='ok', text=b('sensor.living_room_air', 'Good · no need to open a window')),
        N('sparkline', values=b('sensor.living_room_pm25_history', '6,7,9,14,12,10,8,7,8'), colour='steel', width=224, dot=True),
        grid([stat('co2', b('sensor.living_room_co2', '640', tpl='{} ppm')), stat('voc', b('sensor.living_room_voc', 'Low'), 'ok'),
              stat('humidity', b('sensor.living_room_rh', '52', tpl='{}%')), stat('temperature', b('sensor.living_room_temp', '20.5', tpl='{}°'))], cols=2, gap='group', aspect='row')),
   better='The line has no axis or scale yet.')

ex('bedside', 'Bedside', 'A bedside clock: a ring clock with the date, the alarm switch and the lamp brightness.',
   'Make a Facet screen "Bedside": a ring clock with the date, a switch row for my alarm (input_boolean.alarm, the time in sensor.alarm_time) and a slider for the bedside lamp (light.bedside).',
   page(N('clock_face', face='ring', font='mono', format='24h', date='short', seconds=False),
        N('list_row', title='Alarm', sub=b('sensor.alarm_time', '06:45'), icon='none', trailing='toggle', on=b('input_boolean.alarm', 'on', map={'on': '1'}, **{'else': '0'})),
        N('slider', value=b('light.bedside', '30', 'brightness_pct'), min=0, max=100, step=5, width=288)))

ex('fixtures', 'Fixtures', 'The last score as a scoreboard, the form and league place, then the next fixtures as real rows.',
   'Make a Facet screen "Fixtures" for my football team from the TeamTracker sensor (sensor.team): the last result as a scoreboard (both teams and scores), a chip for the result and league position, then the next fixtures from the attribute next, a JSON list of {m: match, d: date}.',
   page(grid([N('big_number', label=b('sensor.team', 'norwich', 'home'), value=b('sensor.team', '2', 'home_score'), unit='', decimals=0),
              N('label', text='FT', type='label', colour='text-dim'),
              N('big_number', label=b('sensor.team', 'luton', 'away'), value=b('sensor.team', '1', 'away_score'), unit='', decimals=0)], cols=3, gap='group', aspect='row'),
        N('status_chip', tone='ok', text=b('sensor.team', 'Won · 6th in the league', 'summary')),
        sec('next fixtures'),
        rows('sensor.team', 'next', [{'m': 'Norwich v Ipswich', 'd': 'Sat 1 Nov'}, {'m': 'Leeds v Norwich', 'd': 'Tue 4 Nov'}, {'m': 'Norwich v Hull', 'd': 'Sat 8 Nov'}],
             title='$item.m', value='$item.d', icon='none', trailing='value', rows=3, empty='No fixtures')),
   better='Club crests need an image component.')


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
