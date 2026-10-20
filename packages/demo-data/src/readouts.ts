import type { ReadoutSource } from '@facet/ui';

/** The mock house's state readouts: someone in the Study, the garden door open a few minutes, the shed's temperature,
 *  and who's home. `readoutsWith(now)`; `READOUT_SAMPLES` = every kind and state, for docs and the explorer. */
export function readoutsWith(now: number): ReadoutSource[] {
  const ago = (min: number) => now - min * 60_000;
  return [
    { kind: 'binary', label: 'Study', deviceClass: 'occupancy', on: true, since: ago(12) },
    { kind: 'binary', label: 'Garden door', deviceClass: 'door', on: true, since: ago(4) },
    { kind: 'value', label: 'Shed', value: 9.6, unit: '°C', lo: 5 },
    {
      kind: 'people',
      people: [
        { name: 'Alex', home: true, since: ago(180) },
        { name: 'Sam', home: false, since: ago(95) },
      ],
    },
  ];
}

export function readoutSamples(now: number): [string, ReadoutSource][] {
  const ago = (min: number) => now - min * 60_000;
  return [
    [
      'occupancy · on',
      { kind: 'binary', label: 'Study', deviceClass: 'occupancy', on: true, since: ago(12) },
    ],
    [
      'occupancy · off',
      { kind: 'binary', label: 'Study', deviceClass: 'occupancy', on: false, since: ago(200) },
    ],
    [
      'door · open',
      { kind: 'binary', label: 'Garden door', deviceClass: 'door', on: true, since: ago(4) },
    ],
    [
      'door · closed',
      { kind: 'binary', label: 'Garden door', deviceClass: 'door', on: false, since: ago(180) },
    ],
    [
      'window · open',
      { kind: 'binary', label: 'Bedroom window', deviceClass: 'window', on: true, since: ago(25) },
    ],
    [
      'leak · wet',
      { kind: 'binary', label: 'Under the sink', deviceClass: 'moisture', on: true, since: ago(2) },
    ],
    ['temperature', { kind: 'value', label: 'Shed', value: 9.6, unit: '°C', lo: 5 }],
    [
      'temperature · too cold',
      { kind: 'value', label: 'Greenhouse', value: 3.2, unit: '°C', lo: 5 },
    ],
    [
      'people · some home',
      {
        kind: 'people',
        people: [
          { name: 'Alex', home: true },
          { name: 'Sam', home: false },
        ],
      },
    ],
    [
      'people · nobody',
      {
        kind: 'people',
        people: [
          { name: 'Alex', home: false, since: ago(95) },
          { name: 'Sam', home: false, since: ago(140) },
        ],
      },
    ],
    [
      'offline',
      { kind: 'binary', label: 'Study', deviceClass: 'occupancy', on: null, since: ago(26 * 60) },
    ],
  ];
}
