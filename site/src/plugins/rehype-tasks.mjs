// Task lists with Facet checkboxes. Turns GFM task items (`- [ ]`, `- [x]`) and in-progress items (`- [~]`) into
// read-only Facet checkboxes (role="checkbox", aria-checked true / false / mixed), and adds a "done / total" count to
// every h2 that has tasks under it. Styles: @facet/ui/checkbox.css plus .task-list in the site CSS.
const CHECK = 'M5.5 10.5 L8.5 13.5 L14.5 7';

const el = (tagName, properties = {}, children = []) => ({
  type: 'element',
  tagName,
  properties,
  children,
});
const classes = (node) => [].concat(node.properties?.className ?? []);

function box() {
  return el('span', { className: ['f-checkbox-box'], ariaHidden: 'true' }, [
    el('svg', { viewBox: '0 0 20 20', width: 20, height: 20 }, [
      el('path', {
        d: CHECK,
        fill: 'none',
        strokeWidth: 2,
        strokeLinecap: 'round',
        strokeLinejoin: 'round',
      }),
    ]),
  ]);
}

/** The task state of an <li>, and its children without the marker. */
function readTask(li) {
  const kids = [...li.children];
  // loose lists wrap the content in a <p>
  const host =
    kids.find((k) => k.type === 'element') && kids[0]?.type === 'element' && kids[0].tagName === 'p'
      ? kids[0]
      : li;
  const first = host.children.findIndex((k) => !(k.type === 'text' && !k.value.trim()));
  const node = host.children[first];
  if (node?.type === 'element' && node.tagName === 'input') {
    host.children.splice(first, 1);
    const t = host.children[first];
    if (t?.type === 'text') t.value = t.value.replace(/^\s+/, '');
    return node.properties.checked ? 'done' : 'open';
  }
  if (node?.type === 'text' && /^\s*\[~\]\s/.test(node.value)) {
    node.value = node.value.replace(/^\s*\[~\]\s+/, '');
    return 'doing';
  }
  return null;
}

function transformLists(node, onTask) {
  for (const child of node.children ?? []) {
    if (child.type !== 'element') continue;
    if (child.tagName === 'li') {
      const state = readTask(child);
      if (state) {
        onTask(state);
        const checked = state === 'done' ? 'true' : state === 'doing' ? 'mixed' : 'false';
        child.properties = { className: ['task'] };
        child.children = [
          el(
            'span',
            {
              className: ['f-checkbox'],
              role: 'checkbox',
              ariaChecked: checked,
              ariaReadOnly: 'true',
              dataState: state,
            },
            [box(), el('span', { className: ['f-checkbox-label'] }, child.children)],
          ),
        ];
        continue;
      }
    }
    if (
      (child.tagName === 'ul' || child.tagName === 'ol') &&
      classes(child).includes('contains-task-list')
    ) {
      child.properties.className = [
        ...classes(child).filter((c) => c !== 'contains-task-list'),
        'task-list',
      ];
    }
    transformLists(child, onTask);
  }
}

export function rehypeTasks() {
  return (tree) => {
    let heading = null;
    const counts = new Map();
    for (const node of tree.children) {
      if (node.type === 'element' && node.tagName === 'h2') {
        heading = node;
        continue;
      }
      if (node.type !== 'element') continue;
      // a [~] list has no task class from GFM, so look at every list
      transformLists({ children: [node] }, (state) => {
        if (!heading) return;
        const c = counts.get(heading) ?? { done: 0, total: 0 };
        c.total++;
        if (state === 'done') c.done++;
        counts.set(heading, c);
      });
    }
    // mark lists that hold [~] items too
    const mark = (n) => {
      for (const c of n.children ?? []) {
        if (c.type !== 'element') continue;
        if (
          (c.tagName === 'ul' || c.tagName === 'ol') &&
          c.children.some((li) => li.type === 'element' && classes(li).includes('task')) &&
          !classes(c).includes('task-list')
        ) {
          c.properties.className = [...classes(c), 'task-list'];
        }
        mark(c);
      }
    };
    mark(tree);
    for (const [h, c] of counts) {
      h.properties.className = [...classes(h), 'task-heading'];
      h.children.push(
        el(
          'span',
          { className: ['task-count', 'facet-label'], ariaLabel: `${c.done} of ${c.total} done` },
          [{ type: 'text', value: `${c.done}/${c.total}` }],
        ),
      );
    }
  };
}
