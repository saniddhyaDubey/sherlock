import { ChevronDown } from "lucide-react";

interface FileSelectorProps {
  selected: string;
  onSelect: (file: string) => void;
  availableFiles: string[];
}

export function FileSelector({ selected, onSelect, availableFiles }: FileSelectorProps) {
  return (
    <div className="relative">
      <select
        value={selected}
        onChange={(e) => onSelect(e.target.value)}
        className="appearance-none surface-card border border-border bg-card text-foreground text-sm font-mono-data px-4 py-2 pr-8 cursor-pointer hover:border-[hsl(var(--border-hover))] transition-colors focus:outline-none focus:ring-1 focus:ring-ring"
      >
        {availableFiles.map((f) => (
          <option key={f} value={f}>
            {f}.dat
          </option>
        ))}
      </select>
      <ChevronDown className="absolute right-2 top-1/2 -translate-y-1/2 w-4 h-4 text-muted-foreground pointer-events-none" />
    </div>
  );
}
