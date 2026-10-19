export type ButtonVariant = 'primary' | 'secondary' | 'ghost';

export interface ButtonOptions {
  label: string;
  variant?: ButtonVariant;
  disabled?: boolean;
  onPress?: (event: MouseEvent) => void;
}

/**
 * A button. Returns a plain `<button>` element styled by `@facet/ui/styles.css`, which reads
 * its colours, type and sizes from the token CSS variables.
 *
 * ```ts
 * const button = createButton({ label: 'Start', onPress: () => startTimer() });
 * container.append(button);
 * ```
 */
export function createButton(options: ButtonOptions): HTMLButtonElement {
  const { label, variant = 'primary', disabled = false, onPress } = options;

  const button = document.createElement('button');
  button.type = 'button';
  button.className = `facet-button facet-button--${variant}`;
  button.textContent = label;
  button.disabled = disabled;

  if (onPress) {
    button.addEventListener('click', onPress);
  }

  return button;
}
