/**
 * A small observable store. Screens read from it and re-render when it changes; adapters (Home Assistant, a mock
 * house, a test) write to it. Nothing in Facet's UI talks to a backend directly.
 *
 * ```ts
 * const store = createStore({ light: { on: false, brightness: 0 } });
 * store.subscribe((s) => console.log(s.light));
 * store.patch({ light: { on: true, brightness: 60 } });
 * ```
 */
export interface Store<T> {
  /** The current state. */
  get(): T;
  /** Replace the whole state. */
  set(next: T): void;
  /** Shallow-merge some top-level keys. */
  patch(partial: Partial<T>): void;
  /** Called after every change, and once straight away. Returns an unsubscribe function. */
  subscribe(fn: (state: T) => void): () => void;
}

export function createStore<T extends object>(initial: T): Store<T> {
  let state = initial;
  const subs = new Set<(s: T) => void>();
  const emit = () => subs.forEach((fn) => fn(state));
  return {
    get: () => state,
    set(next) {
      state = next;
      emit();
    },
    patch(partial) {
      state = { ...state, ...partial };
      emit();
    },
    subscribe(fn) {
      subs.add(fn);
      fn(state);
      return () => subs.delete(fn);
    },
  };
}

/**
 * Call `fn` once a second, aligned to the wall clock's second, so every clock on the page ticks together.
 * Returns a stop function.
 */
export function startTicker(fn: (now: Date) => void): () => void {
  let timer: ReturnType<typeof setTimeout> | undefined;
  let stopped = false;
  const run = () => {
    if (stopped) return;
    fn(new Date());
    timer = setTimeout(run, 1000 - (Date.now() % 1000) + 5);
  };
  run();
  return () => {
    stopped = true;
    clearTimeout(timer);
  };
}
